/*
* Copyright (c) 2015, CESAR.
* All rights reserved.
*
* This software may be modified and distributed under the terms
* of the BSD license. See the LICENSE file for details.
*
*/
#ifndef ARDUINO

#define _GNU_SOURCE
#include <stdlib.h>
#include <glib.h>
#include <sys/socket.h>
#include <sys/poll.h>

#include "nrf24l01_server.h"
#include "nrf24l01.h"
#include "nrf24l01_proto_net.h"
#include "util.h"
#include "list.h"

#define BROADCAST		0

#define JOIN_TIMEOUT	NRF24_TIMEOUT		//ms
#define JOIN_RETRY		NRF24_RETRIES
#define JOIN_DELAY			20									//ms
#define JOIN_INTERVAL	50									//20ms <= interval <= 1s

#define AVAILABLE_POLLTIME_NS	1000000	//1ms

#define TIMEOUT_SERVICE_THREAD	 1				//1ms

#define INVALID_EVENTFD		((eventfd_t)-2)

enum {
	eUNKNOWN,
	eCLOSE,
	eOPEN,

	eJOIN,
	eJOIN_PENDING,
	eJOIN_TIMEOUT,
	eJOIN_INTERMITION,
	eJOIN_ECONNREFUSED,
	eJOIN_CHANNEL_BUSY,

	ePRX,
	ePTX
} ;

typedef struct  __attribute__ ((packed)) {
	struct list_head	node;
	int	len,
			pipe;
	uint8_t	raw[];
} data_t;

typedef struct  __attribute__ ((packed)) {
	int	cli;
	int 	srv;
} fds_t;

typedef struct  {
	fds_t			fds;
	int				state,
						pipe;
	uint16_t		net_addr;
	uint32_t		hashid;
	ulong_t		heartbeat_sec;
	data_t			*rxmsg;
} client_t;

static volatile int			m_fd = SOCKET_INVALID,
										m_state = eUNKNOWN,
										m_nref = 0;
static GSList					*m_pclients = NULL,
										*m_prx_queue = NULL;
static client_t				*m_pipes[NRF24L01_PIPE_MAX] =  { NULL, NULL, NULL, NULL, NULL };
static eventfd_t			m_naccept = 0;
static data_t					*m_join_data = NULL;

static LIST_HEAD(m_ptx_list);
static G_LOCK_DEFINE(m_ptx_list);

GThread *m_gthread = NULL;

static void client_free(gpointer pentry)
{
	close(((client_t*)pentry)->fds.srv);
	g_free(pentry);
}

void client_close(gpointer pentry, gpointer user_data)
{
	close(((client_t*)pentry)->fds.srv);
	((client_t*)pentry)->fds.srv = SOCKET_INVALID;
}

static inline gint state_match(gconstpointer pentry, gconstpointer pdata)
{
	return ((const client_t*)pentry)->state - *((const int*)pdata);
}

static inline gint join_match(gconstpointer pentry, gconstpointer pdata)
{
	return !(((const client_t*)pentry)->net_addr == ((const nrf24_payload*)pdata)->hdr.net_addr &&
				  ((const client_t*)pentry)->hashid == ((const nrf24_payload*)pdata)->msg.join.hashid);
}

static inline int get_delay_random(void)
{
	srand((unsigned)time(NULL));
	return (((rand() % JOIN_INTERVAL) + 1)  * JOIN_DELAY);
}

static int poll_fd(int fd, long int timeout)
{
	struct pollfd fd_poll;
	struct timespec time;

	fd_poll.fd = fd;
	fd_poll.events = POLLIN | POLLPRI | POLLRDHUP;
	fd_poll.revents = 0;
	time.tv_sec = 0;
	time.tv_nsec = timeout;
	return ppoll(&fd_poll, 1, &time, NULL);
}

static int get_pipe_free(void)
{
	int 			pipe;
	client_t	**ppc = m_pipes;

	for (pipe=NRF24L01_PIPE_MAX; pipe!=0 && *ppc != NULL; --pipe, ++ppc);

	return (pipe != 0 ? (NRF24L01_PIPE_MAX - pipe) + 1 : NRF24L01_NO_PIPE);
}

static int set_pipe_free(int pipe, client_t *pc)
{
	--pipe;
	if(pipe < NRF24L01_PIPE_MAX && m_pipes[pipe] == NULL) {
		m_pipes[pipe] = pc;
		return SUCCESS;
	}
	return ERROR;
}

static data_t *build_data(int pipe, void *praw, len_t len, data_t *msg)
{
	len_t size = (msg != NULL) ? msg->len : 0;

	msg = (data_t*)realloc(msg, sizeof(data_t) + size + len);
	if (msg == NULL) {
		return NULL;
	}

	INIT_LIST_HEAD(&msg->node);
	memcpy(msg->raw + size, praw, len);
	msg->len = size + len;
	msg->pipe = pipe;
	return msg;
}

static data_t *build_ptx_payload(int pipe, int addr, int type, int offset, pdata_t praw, len_t len)
{
	nrf24_payload payload;
	data_t *pd = NULL;

	if (len <= sizeof(payload.msg.raw)) {
		payload.hdr.net_addr = addr;
		payload.hdr.msg_type = type;
		payload.hdr.offset = offset;
		memcpy(payload.msg.raw, praw, len);
		pd = build_data(pipe, &payload, sizeof(nrf24_header)+len, NULL);
	}
	return pd;
}

static inline void put_ptx_queue(data_t *pd)
{
	G_LOCK(m_ptx_list);
	list_move_tail(&pd->node, &m_ptx_list);
	G_UNLOCK(m_ptx_list);
}

static int waiting_tx_immediate(int timeout)
{
//
//	uint32_t start = millis();
//
//	while ( ! (read_register(FIFO_STATUS) & _BV(TX_EMPTY)) ){
//		if ( get_status() & _BV(MAX_RT)){
//			write_register(NRF_STATUS,_BV(MAX_RT) );
//				ce(LOW);										  //Set re-transmit
//				ce(HIGH);
//				if (millis() - start >= timeout){
//					ce(LOW); flush_tx(); return 0;
//				}
//		}
//		if ( millis() - start > (timeout+85)){
//			errNotify();
//			#if defined (FAILURE_HANDLING)
//			return 0;
//			#endif
//		}
//	}
//	ce(LOW);				   //Set STANDBY-I mode
	return 1;
}

static int prx_service(void)
{
	nrf24_payload	data;
	int	pipe,
			len;
	GSList *pentry;
	client_t *pc;
	data_t *pdata;

	for (pipe=nrf24l01_prx_pipe_available(); pipe!=NRF24L01_NO_PIPE; pipe=nrf24l01_prx_pipe_available()) {
		printf("%s pipe:%d STATUS:%#04x FIFO_STATUS:%#04x\n", __func__, pipe, nrf24l01_command(NOP), nrf24l01_inr(FIFO_STATUS));
		len = nrf24l01_prx_data(&data, sizeof(data));
		printf("%s pipe:%d len:%d STATUS:%#04x FIFO_STATUS:%#04x\n", __func__, pipe, len, nrf24l01_command(NOP), nrf24l01_inr(FIFO_STATUS));
		dump_data("\t", pipe, &data, len);
		if (len >= sizeof(data.hdr)) {
			printf("%s type:%d\n", __func__, data.hdr.msg_type);
			switch (data.hdr.msg_type) {
			case NRF24_MSG_JOIN_LOCAL:
				if (len != NRF24_JOIN_PW_SIZE || pipe != BROADCAST || data.hdr.offset != 0 ||
					data.msg.join.maj_version > NRF24_VERSION_MAJOR ||
					data.msg.join.min_version > NRF24_VERSION_MINOR) {
					break;
				}
				data.hdr.msg_type = NRF24_MSG_JOIN_RESULT;
				pentry = g_slist_find_custom(m_pclients, &data, join_match);
				if (pentry == NULL) {
					int pipe_free = get_pipe_free();
					if (pipe_free == NRF24L01_NO_PIPE) {
						data.msg.join.result == NRF24_ECONNREFUSED;
						put_ptx_queue(build_data(pipe, &data, len, NULL));
						break;
					}
					pc = g_new0(client_t, 1);
					if (pc == NULL || socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, (int*)&pc->fds) < 0) {
						//printf("serial: socketpair(): %s(%d)\n", strerror(err), err);
						g_free(pc);
						break;
					}
					pc->state = eOPEN;
					pc->pipe = pipe_free;
					pc->net_addr = data.hdr.net_addr;
					pc->hashid = data.msg.join.hashid;
					if(set_pipe_free(pipe_free, pc) == ERROR) {
						client_close(pc, NULL);
						g_free(pc);
						break;
					}
					m_pclients = g_slist_append(m_pclients, pc);
					eventfd_write(m_fd, 1);
				} else {
					pc = (client_t*)pentry->data;
				}
				pc->heartbeat_sec = tline_sec();
				data.msg.join.pipe = pc->pipe;
				data.msg.join.result == NRF24_SUCCESS;
				put_ptx_queue(build_data(pipe, &data, len, NULL));
				break;

			case NRF24_MSG_JOIN_GATEWAY:
				if (len != NRF24_JOIN_PW_SIZE || pipe != BROADCAST) {
 					break;
				}
				data.hdr.msg_type = NRF24_MSG_JOIN_RESULT;
				data.msg.join.result == NRF24_ECONNREFUSED;
				put_ptx_queue(build_data(pipe, &data, len, NULL));
				break;

			case NRF24_MSG_HEARTBEAT:
				if (len != NRF24_JOIN_PW_SIZE || data.hdr.offset != 0 ||
					data.msg.join.maj_version > NRF24_VERSION_MAJOR ||
					data.msg.join.min_version > NRF24_VERSION_MINOR) {
					break;
				}
				data.hdr.msg_type = NRF24_MSG_JOIN_RESULT;
				data.msg.join.result == NRF24_ECONNREFUSED;
				pentry = g_slist_find_custom(m_pclients, &data, join_match);
				if (pentry != NULL) {
					pc = (client_t*)pentry->data;
					if(pc ->pipe == pipe && pc ->pipe == data.msg.join.pipe) {
						pc->heartbeat_sec = tline_sec();
						data.msg.join.result == NRF24_SUCCESS;
					}
				}
				put_ptx_queue(build_data(pipe, &data, len, NULL));
				break;

			case NRF24_MSG_APP:
			case NRF24_MSG_APP_FRAG:
				break;

			//case NRF24_MSG_UNJOIN_LOCAL:
			//case NRF24_MSG_JOIN_RESULT:
			}
		}
	}

//	while (!list_empty(&m_ptx_list)) {
//		G_LOCK(m_ptx_list);
//		pdata = list_first_entry(&m_ptx_list, data_t, node);
//		G_UNLOCK(m_ptx_list);
//		nrf24l01_set_ptx(pdata->pipe);
//		nrf24l01_ptx_data(pdata->raw, pdata->len, false);
//		nrf24l01_ptx_empty();
//
//
//
//		list_del_init(&pch_conf->dial_node);
//
//	}
//
//	G_LOCK(m_ptx_list);
//	list_move_tail(&pd->node, &m_ptx_list);
//	G_UNLOCK(m_ptx_list);


//	if (debug) {
//		printk(MODULE_NAME ": %s started...\n", __func__);
//	}
//
//	while (m_dsp_enable) {
//		msleep_interruptible(DIAL_JOB_TIMEOUT);
//
//		spin_lock(&m_e1diallist_lock);
//		list_for_each_entry_safe(pdata, pnext, &m_e1diallist, e1dial_node) {
//			if (pdata->eacs != EACS_INVALIDO && pdata->dial.pdigit != NULL) {
//				if (*pdata->dial.pdigit != '\0') {
//					send_dahdi_digit(pdata, *pdata->dial.pdigit++);
//				} else {
//					list_del_init(&pdata->node);
//				}
//			}
//		}
//		spin_unlock(&m_e1diallist_lock);
//	}

	return ePRX;
}

static data_t *build_join_msg(void)
{
	nrf24_join_local join;

	join.maj_version = NRF24_VERSION_MAJOR;
	join.min_version = NRF24_VERSION_MINOR;
	join.pipe = BROADCAST;
	join.result = NRF24_SUCCESS;

	srand((unsigned int)time(NULL));
	join.hashid = rand() ^ (rand() * 65536);
	return build_ptx_payload(BROADCAST,
													rand() ^ rand(),
													NRF24_MSG_JOIN_GATEWAY,
													0,
													&join,
													sizeof(join));
}

static int join_read(ulong_t start)
{
	nrf24_payload	data,
								*join_payload = (nrf24_payload*)&m_join_data->raw;
	int	pipe,
			len;

	for (pipe=nrf24l01_prx_pipe_available(); pipe!=NRF24L01_NO_PIPE; pipe=nrf24l01_prx_pipe_available()) {
		len = nrf24l01_prx_data(&data, sizeof(data));
		printf("%s pipe:%d len:%d\n", __func__, pipe, len);
		dump_data("\t", pipe, &data, len);
		if (len == NRF24_JOIN_PW_SIZE && pipe == BROADCAST &&
			data.hdr.msg_type == NRF24_MSG_JOIN_RESULT &&
			data.hdr.offset == 0 &&
			data.hdr.net_addr == join_payload->hdr.net_addr &&
			data.msg.join.hashid == join_payload->msg.join.hashid &&
			data.msg.join.result == NRF24_ECONNREFUSED) {
			return eJOIN_ECONNREFUSED;
		}
	}

	if (tline_out(tline_ms(), start, JOIN_TIMEOUT)) {
		printf("%s eJOIN_TIMEOUT STATUS:%#04x\n", __func__, nrf24l01_command(NOP));
		return eJOIN_TIMEOUT;
	}

	return eJOIN_PENDING;
}

static int join(bool reset)
{
	static int	state = eJOIN,
					 	retry = JOIN_RETRY,
					 	ch = 0,
					 	ch0 = 0;
	static ulong_t	start = 0,
								delay = 0;

	if (reset) {
		ch = nrf24l01_get_channel();
		ch0 = ch;
		retry = JOIN_RETRY;
		state = eJOIN;
	}

	switch(state) {
	case eJOIN:
		if (m_join_data == NULL) {
			return eJOIN_CHANNEL_BUSY;
		}
		nrf24l01_set_ptx(m_join_data->pipe);
		printf("%s STATUS:%#04x\n", __func__, nrf24l01_ptx_data(m_join_data->raw, m_join_data->len, false));
		nrf24l01_ptx_wait_datasent();
		printf("%s STATUS:%#04x FIFO_STATUS:%#04x\n", __func__, nrf24l01_command(NOP), nrf24l01_inr(FIFO_STATUS));
		dump_data("\t", -1, m_join_data->raw, m_join_data->len);
		nrf24l01_set_prx();
		start = tline_ms();
		state = eJOIN_PENDING;
		break;

	case eJOIN_PENDING:
		state = join_read(start);
		break;

	case eJOIN_ECONNREFUSED:
		nrf24l01_set_standby();
		ch += 2;
		if (nrf24l01_set_channel(ch) == ERROR) {
			ch = CH_MIN;
			nrf24l01_set_channel(ch);
		}
		if(ch == ch0) {
			return eJOIN_CHANNEL_BUSY;
		}
		retry = JOIN_RETRY;
		state = eJOIN;
		break;

	case eJOIN_TIMEOUT:
		if (--retry == 0) {
			return ePRX;
		}
		nrf24l01_set_standby();
		state = eJOIN_INTERMITION;
		delay = get_delay_random();
		start = tline_ms();
		break;

	case eJOIN_INTERMITION:
		if (tline_out(tline_ms(), start, delay)) {
			state = eJOIN;
		}
		break;
	}
	return eJOIN;
}

static gboolean server_service(gpointer dummy)
{
	gboolean result = G_SOURCE_CONTINUE;

	if (++m_nref == 1) {
		if (m_fd == SOCKET_INVALID && m_state != eUNKNOWN) {
			m_state = eCLOSE;
		} else if (m_fd != SOCKET_INVALID && m_state == eUNKNOWN) {
			m_state = eOPEN;
		}
		switch(m_state) {
		case eCLOSE:
			nrf24l01_set_standby();
			nrf24l01_close_pipe(BROADCAST);
			if (m_pclients != NULL) {
				g_slist_foreach(m_pclients, client_close, NULL);
				//g_slist_free_full(m_pclients, client_free);
			}
			memset(m_pipes, 0, sizeof(m_pipes));
			m_state = eUNKNOWN;
			result = G_SOURCE_REMOVE;
			break;

		case eOPEN:
			nrf24l01_open_pipe(BROADCAST);
			m_state = join(true);
			break;

		case eJOIN:
			m_state = join(false);
			break;

		case eJOIN_CHANNEL_BUSY:
			m_state = eCLOSE;
			eventfd_write(m_fd, 1);
			break;

		case ePRX:
			m_state = prx_service();
			break;
		}
	}
	--m_nref;
	return result;
}

static gpointer service_thread(gpointer dummy)
{
    GMainLoop *loop;
    GSource *timer;
    GMainContext *context = g_main_context_new();

    g_main_context_push_thread_default(context);
    loop = g_main_loop_new(context, FALSE);
    timer = g_timeout_source_new(TIMEOUT_SERVICE_THREAD);
    g_source_set_callback(timer, server_service, NULL, NULL);
    g_source_attach(timer, context);
    g_main_loop_run(loop);
    return NULL;
}

int nrf24l01_server_open(int socket, int channel)
{
	if (socket == SOCKET_INVALID) {
		errno = EBADF;
		return ERROR;
	}

	if (m_fd != SOCKET_INVALID) {
		errno = EMFILE;
		return ERROR;
	}

	if (nrf24l01_set_channel(channel) == ERROR) {
		errno = EINVAL;
		return ERROR;
	}

	m_join_data = build_join_msg();
	if (m_join_data == NULL) {
		errno = ENOMEM;
		return ERROR;
	}
	//TODO: debug
	dump_data("Join Data:", -1, m_join_data, m_join_data->len + sizeof(data_t));

	g_mutex_init(&G_LOCK_NAME(m_ptx_list));
	m_naccept = 0;
	m_fd = socket;

	m_gthread = g_thread_new("service_thread", service_thread, NULL);
	if (m_gthread == NULL) {
		g_free(m_join_data);
		m_join_data = NULL;
		errno = ENOMEM;
		return ERROR;
	}

	return SUCCESS;
}

int nrf24l01_server_close(int socket)
{
	if (socket == SOCKET_INVALID) {
		errno = EBADF;
		return ERROR;
	}

	if (socket == m_fd) {
		m_fd = SOCKET_INVALID;
		eventfd_write(socket, INVALID_EVENTFD);
		if (m_gthread != NULL) {
			g_thread_join(m_gthread);
			m_gthread == NULL;
		}
		g_free(m_join_data);
		m_join_data = NULL;
		g_mutex_clear(&G_LOCK_NAME(m_ptx_list));
	} else {
		close(socket);
	}
	return SUCCESS;
}

int nrf24l01_server_accept(int socket)
{
	GSList *pentry = NULL;
	int st;

	if (m_fd == SOCKET_INVALID || socket != m_fd) {
		errno = EBADF;
		return ERROR;
	}

	while (pentry == NULL) {
		if (m_naccept == 0) {
		   st = eventfd_read(socket, &m_naccept);
		   if (st < 0 && m_naccept == INVALID_EVENTFD) {
				if (m_naccept == INVALID_EVENTFD) {
					m_naccept = 0;
					errno = EBADF;
				}
				st = ERROR;
				break;
		   }
		   if (m_state == eCLOSE || m_state == eUNKNOWN) {
				m_naccept = 0;
				errno = ECONNREFUSED;
				st = ERROR;
				break;
		   }
		}

		--m_naccept;

		// find the client in list which is open state
		st = eOPEN;
		pentry = g_slist_find_custom(m_pclients, &st, state_match);
		if (pentry != NULL) {
			st = ((client_t*)pentry->data)->fds.cli;
			((client_t*)pentry->data)->state = ePRX;
		}
	}
	return st;
}

int nrf24l01_server_available(int socket)
{
	if (socket == SOCKET_INVALID) {
		errno = EBADF;
		return ERROR;
	}

	//returns: 1 for socket available,
	//				 0 for no available,
	//				 or -1 for errors.
	return poll_fd(socket, AVAILABLE_POLLTIME_NS);
}

#endif		// ARDUINO
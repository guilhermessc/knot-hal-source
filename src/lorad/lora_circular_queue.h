/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

/*
 * This library implements a circular queue
 * with a static pre-defined number of nodes.
 * 
 * The nodes that will be used for each queue
 * can be found at the respective NODE_SET.
 *
 * Each queue is related to only one NODE_SET
 * and contains only one type of node, that
 * can be: DATA_TX, DATA_RX, MGM_TX or MGM_RX.
 *
 * Each NODE_SET contains sub-sets for each
 * type of node.
 */

#include <stdint.h>

/* The size of each node type raw data */
#define LORAD_DATA_MAX_PKT_LEN 37
#define LORAD_MGM_MAX_PKT_LEN 23

/* The lorad_node_fd is the index on the array of the specifc node */
typedef int lorad_node_fd;

/* Single node structs */
struct lorad_data_node {
	uint8_t data[LORAD_DATA_MAX_PKT_LEN];
	lorad_node_fd next;
};
struct lorad_mgm_node {
	uint8_t data[LORAD_MGM_MAX_PKT_LEN];
	lorad_node_fd next;
};


struct lorad_node_set;

#define LORAD_EMPTY_QUEUE_FD -1
enum lorad_node_type { DATA_TX = 1, DATA_RX, MGM_TX, MGM_RX };

/* Single queue struct */
struct lorad_queue {
	/* 
	 * Once the queue is cricular the tail points to the head 
	 * so both head and tail can be accessed in constant time.
	 */
	lorad_node_fd tail;

	/* These fields shall be manipulated only by the init function */
	struct lorad_node_set* node_set;
	enum lorad_node_type type;
};


/*
 * The total number of packets that equals the
 * sum of every node in every thing's queue
 */
#define LORAD_DATA_TX_SET_SIZE 320
#define LORAD_DATA_RX_SET_SIZE 160
#define LORAD_MGM_TX_SET_SIZE 80
#define LORAD_MGM_RX_SET_SIZE 80

/* 
 * The node set contains:
 *
 * Sub-sets of each queue type and 
 * sentinels to manage each sub-set.
 */
struct lorad_node_set {

	struct lorad_data_node lorad_data_tx_node_set[LORAD_DATA_TX_SET_SIZE];
	struct lorad_data_node lorad_data_rx_node_set[LORAD_DATA_RX_SET_SIZE];
	struct lorad_mgm_node lorad_mgm_tx_node_set[LORAD_MGM_TX_SET_SIZE];
	struct lorad_mgm_node lorad_mgm_rx_node_set[LORAD_MGM_RX_SET_SIZE];

	/* The sentinels manages the node_set */
	struct lorad_queue sentinel_data_tx;
	struct lorad_queue sentinel_data_rx;
	struct lorad_queue sentinel_mgm_tx;
	struct lorad_queue sentinel_mgm_rx;
};


/* Initializes the sentinels must be called at the begining of the code */
int lorad_queue_begin(struct lorad_node_set *node_set);

/* Initializes a queue must be called at the declaration of a queue */
int lorad_init_queue(struct lorad_queue *queue, struct lorad_node_set *node_set,
	enum lorad_node_type type);

/*
 * Deletes all the remaining nodes at a queue to make
 * the nodes avaliable again at the NODE_SET
 */
int lorad_close_queue(struct lorad_queue *queue);


/* Queue modifying functions */
int lorad_queue_insert(struct lorad_queue *queue, uint8_t* data);
int lorad_queue_delete(struct lorad_queue *queue);
/* Returns the buffer length */
size_t lorad_queue_peek(struct lorad_queue *queue, uint8_t* peek_buffer);
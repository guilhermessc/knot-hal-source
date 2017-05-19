/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include <string.h>
#include "lora_circular_queue.h"

/* Initializes the sentinels must be called at the begining of the code */
int lorad_queue_begin(struct lorad_node_set *node_set){

	int i;

	if (node_set == NULL)
		return -1;

	/* Initializing data_tx sentinel */
	node_set->sentinel_data_tx.type = DATA_TX;
	node_set->sentinel_data_tx.node_set = node_set;
	node_set->sentinel_data_tx.tail = 0;

	/* Initinalizing  data_tx sub-node_set */
	for (i=0; i<LORAD_DATA_TX_SET_SIZE-2; ++i) {
		node_set->lorad_data_tx_node_set[i].next = i+1;
	}
	node_set->lorad_data_tx_node_set[LORAD_DATA_TX_SET_SIZE-1].next = 0;


	/* Initializing data_rx sentinel */
	node_set->sentinel_data_rx.type = DATA_RX;
	node_set->sentinel_data_rx.node_set = node_set;
	node_set->sentinel_data_rx.tail = 0;

	/* Initinalizing  data_rx sub-node_set */
	for (i=0; i<LORAD_DATA_RX_SET_SIZE-2; ++i) {
		node_set->lorad_data_rx_node_set[i].next = i+1;
	}
	node_set->lorad_data_rx_node_set[LORAD_DATA_RX_SET_SIZE-1].next = 0;


	/* Initializing mgm_tx sentinel */
	node_set->sentinel_mgm_tx.type = MGM_TX;
	node_set->sentinel_mgm_tx.node_set = node_set;
	node_set->sentinel_mgm_tx.tail = 0;

	/* Initinalizing  mgm_tx sub-node_set */
	for (i=0; i<LORAD_MGM_TX_SET_SIZE-2; ++i) {
		node_set->lorad_mgm_tx_node_set[i].next = i+1;
	}
	node_set->lorad_mgm_tx_node_set[LORAD_MGM_TX_SET_SIZE-1].next = 0;


	/* Initializing mgm_rx sentinel */
	node_set->sentinel_mgm_rx.type = MGM_RX;
	node_set->sentinel_mgm_rx.node_set = node_set;
	node_set->sentinel_mgm_rx.tail = 0;

	/* Initinalizing  mgm_rx sub-node_set */
	for (i=0; i<LORAD_MGM_RX_SET_SIZE-2; ++i) {
		node_set->lorad_mgm_rx_node_set[i].next = i+1;
	}
	node_set->lorad_mgm_rx_node_set[LORAD_MGM_RX_SET_SIZE-1].next = 0;


	return 0;
}

/* Initializes a queue must be called at the declaration of a queue */
int lorad_init_queue(struct lorad_queue *queue, struct lorad_node_set *node_set,
	enum lorad_node_type type){

	if(queue == NULL)
		return -1;
	if(node_set == NULL)
		return -1;
	if(type < DATA_TX || type > MGM_RX)
		return -1;

	queue->tail = LORAD_EMPTY_QUEUE_FD;
	queue->node_set = node_set;
	queue->type = type;

	return 0;
}

/*
 * Deletes all the remaining nodes at a queue to make
 * the nodes avaliable again at the NODE_SET
 */
int lorad_close_queue(struct lorad_queue *queue){

	int err = 0;

	/* Check if queue was properely initialized */
	if(queue->node_set == NULL)
		return -1;

	while (queue->tail != LORAD_EMPTY_QUEUE_FD && err == 0) {
		err = lorad_queue_delete(queue);
	}

	return err;
}

static struct *lorad_data_node get_queue_node_subset (struct lorad_queue *queue) {

	switch (queue->type){
	
	case DATA_TX:
		return queue->node_set->lorad_data_tx_node_set

	case DATA_RX:
		return queue->node_set->lorad_data_rx_node_set

	case MGM_TX:
		return queue->node_set->lorad_mgm_tx_node_set

	case MGM_RX:
		return queue->node_set->lorad_mgm_rx_node_set
		
	default:
		return NULL;
	}
}

static struct *lorad_queue get_queue_sentinel (struct lorad_queue *queue) {

	switch (queue->type){
	
	case DATA_TX:
		return &(queue->node_set->sentinel_data_tx);

	case DATA_RX:
		return &(queue->node_set->sentinel_data_rx);

	case MGM_TX:
		return &(queue->node_set->sentinel_mgm_tx);

	case MGM_RX:
		return &(queue->node_set->sentinel_mgm_rx);
		
	default:
		return NULL;
	}
}

static size_t get_queue_data_len (struct lorad_queue *queue) {

	switch (queue->type){
	
	case DATA_TX:
		return LORAD_DATA_MAX_PKT_LEN;

	case DATA_RX:
		return LORAD_DATA_MAX_PKT_LEN;

	case MGM_TX:
		return LORAD_MGM_MAX_PKT_LEN;

	case MGM_RX:
		return LORAD_MGM_MAX_PKT_LEN;
		
	default:
		return 0;
	}
}

static int switch_head_node_queue (struct lorad_queue *dst_queue,
	struct lorad_queue *src_queue){

	struct lorad_node_set *node_subset;
	lorad_node_fd node_to_switch;

	if (src_queue->type != dst_queue->type)
		return -1;
	if (src_queue->node_set != dst_queue->node_set)
		return -1;
	if (src_queue->tail == LORAD_EMPTY_QUEUE_FD)
		return -1;

	node_subset = get_queue_node_subset(src_queue);
	if (node_subset == NULL)
		return -1;
	/* Get the source queue's head */
	node_to_switch = node_subset[src_queue->tail].next;
	

	/* Removing the node from the source queue head */

	/* One node remaining case */
	if (src_queue->tail == node_to_switch) {
		/* Make the queue's tail points to nothing */
		src_queue->tail = LORAD_EMPTY_QUEUE_FD;
	} else {
		/* Make the queue's tail points to the new head */
		src_queue->tail = node_subset[node_to_switch].next;
	}


	/* Adding the node in the destiny queue tail */

	/* Make the new node point to the head */
	node_subset[node_to_switch].next = node_subset[dst_queue->tail].next;
	/* Make old tail node point to the new node */
	node_subset[dst_queue->tail].next = node_to_switch;
	/* Make new node the queue's tail */
	dst_queue->tail = node_to_switch;

	return 0;
}

static int head_update(struct lorad_queue *queue, uint8_t* data){

	struct lorad_node_set *node_subset;
	lorad_node_fd head_node;
	size_t max_len = get_queue_data_len(queue);

	if (len == 0)
		return -1;

	node_subset = get_queue_node_subset(queue);
	if (node_subset == NULL)
		return -1;

	/* Get the queue's head */
	head_node = node_subset[queue->tail].next;

	/* Update head data */
	memcpy(node_subset[head_node].data, data, max_len);

	return 0;
}

int lorad_queue_insert(struct lorad_queue *queue, uint8_t* data){
	
	int err = 0;
	struct lorad_queue *sentinel;

	/* Check if queue was properely initialized */
	if(queue->node_set == NULL)
		return -1;

	/* 
	 * Get the sentinel that manages the proper node
	 * subset for insertion in the queue's node set.
	 */
	sentinel = get_queue_sentinel(queue);
	if (sentinel == NULL)
		return -1;

	/* Get an avaliable node from node set through the sentinel */
	err = switch_head_node_queue (queue, sentinel);
	if (err < 0)
		return -1;

	err = head_update(queue, data);
	if (err < 0)
		return -1;

	return 0;
}

int lorad_queue_delete(struct lorad_queue *queue){
	
	int err = 0;
	struct lorad_queue *sentinel;

	/* Check if queue was properely initialized */
	if(queue->node_set == NULL)
		return -1;

	/* 
	 * Get the sentinel that manages the proper node
	 * subset for deletion in the queue's node set.
	 */
	sentinel = get_queue_sentinel(queue);
	if (sentinel == NULL)
		return -1;

	/* Make the node avaliable in the node set through the sentinel */
	err = switch_head_node_queue (sentinel, queue);
	if (err < 0)
		return -1;

	return 0;
}

int lorad_queue_peek(struct lorad_queue *queue, uint8_t* peek_buffer){
	return 0;
}
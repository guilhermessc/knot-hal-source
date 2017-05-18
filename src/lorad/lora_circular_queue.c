/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */
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
	return 0;
}

int lorad_queue_insert(struct lorad_queue *queue, uint8_t* data){
	return 0;
}
int lorad_queue_delete(struct lorad_queue *queue){
	return 0;
}
int lorad_queue_peek(struct lorad_queue *queue, uint8_t* peek_buffer){
	return 0;
}
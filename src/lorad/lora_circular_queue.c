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
	return 0;
}

/* Initializes a queue must be called at the declaration of a queue */
int lorad_init_queue(struct lorad_queue queue, struct lorad_node_set *node_set,
	enum lorad_node_type type){
	return 0;
}

/*
 * Deletes all the remaining nodes at a queue to make
 * the nodes avaliable again at the NODE_SET
 */
int lorad_close_queue(struct lorad_queue queue){
	return 0;
}

int lorad_queue_insert(struct lorad_queue queue, uint8_t* data){
	return 0;
}
int lorad_queue_delete(struct lorad_queue queue){
	return 0;
}
int lorad_queue_peek(struct lorad_queue queue, uint8_t* peek_buffer){
	return 0;
}
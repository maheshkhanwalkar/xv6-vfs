#pragma once

struct queue;
typedef struct queue* queue_t;

/**
 * Create and initialise an empty queue
 * @return the new queue opaque pointer
 */
queue_t queue_create();

/**
 * Enqueue an item onto the provided queue
 *
 * @param q - queue to modify
 * @param data - item to enqueue
 */
void queue_enq(queue_t q, void* data);

/**
 * Dequeue an item from the provided queue
 *
 * @param q - queue to modify
 * @return the item that was at the front of the queue
 */
void* queue_deq(queue_t q);

/**
 * Peek the item at the front of the queue
 *
 * @param q - the queue to peek from
 * @return the item at the front of the queue
 */
void* queue_peek(queue_t q);

/**
 * Destroy the given queue, freeing any resources that were
 * allocated (or previously in-use)
 *
 * @param q - queue to destroy (clean up)
 */
void queue_destroy(queue_t q);

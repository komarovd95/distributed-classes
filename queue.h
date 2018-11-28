#ifndef PA1_QUEUE_H
#define PA1_QUEUE_H

#define MAX_SIZE 256

#include "ipc.h"

/**
 * A critical section request.
 */
typedef struct {
    timestamp_t timestamp; ///< Timestamp when CS is requested.
    local_id process_id;   ///< Local PID of requesting process.
    int replied;           ///< TODO
} CsRequest;

/**
 * A priority queue.
 *
 * @see https://en.wikipedia.org/wiki/Priority_queue
 */
typedef struct {
    int length;                   ///< Size of queue.
    CsRequest requests[MAX_SIZE]; ///< Queue items.
} Queue;

/**
 * Adds item to priority queue.
 *
 * @param queue a priority queue
 * @param request an item to add into queue
 * @return 0 if success
 */
int enqueue(Queue *queue, const CsRequest *request);

/**
 * Removes item from priority queue.
 *
 * @param queue a priority queue.
 * @param request an item that was removed from queue. May be NULL.
 * @return 0 if success
 */
int dequeue(Queue *queue, CsRequest *request);

/**
 * Returns items from head of priority queue and does not remove it.
 *
 * @param queue a priority queue
 * @param request an item from the head of queue
 * @return 0 if success
 */
int peek(Queue *queue, CsRequest *request);

#endif //PA1_QUEUE_H

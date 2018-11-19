#ifndef PA1_QUEUE_H
#define PA1_QUEUE_H

#define MAX_SIZE 256

#include "ipc.h"

typedef struct {
    timestamp_t timestamp;
    local_id process_id;
    int replied;
} CsRequest;

typedef struct {
    int length;
    CsRequest requests[MAX_SIZE];
} Queue;

int enqueue(Queue *queue, const CsRequest *request);

int dequeue(Queue *queue, CsRequest *request);

int peek(Queue *queue, CsRequest *request);

#endif //PA1_QUEUE_H

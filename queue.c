#include <stdio.h>
#include "queue.h"
#include "core.h"

/**
 * Swaps two elements in given queue.
 *
 * @param queue a queue
 * @param a an index to swap
 * @param b an other index to swap
 */
void swap(Queue *queue, int a, int b);

int enqueue(Queue *queue, const CsRequest *request) {
    int i;

    if (queue->length + 1 > MAX_SIZE) {
        return 1;
    }

    i = queue->length + 1;
    queue->requests[i].timestamp = request->timestamp;
    queue->requests[i].process_id = request->process_id;

    while (i > 1 && cs_request_compare(&queue->requests[i / 2], &queue->requests[i]) > 0) {
        swap(queue, i, i / 2);
        i /= 2;
    }
    queue->length++;

    return 0;
}

int dequeue(Queue *queue, CsRequest *request) {
    int i, j;

    if (queue->length == 0) {
        return 1;
    }

    if (request != NULL) {
        request->timestamp = queue->requests[1].timestamp;
        request->process_id = queue->requests[1].process_id;
    }

    swap(queue, 1, queue->length);
    queue->length--;

    i = 1;
    while (i * 2 <= queue->length) {
        j = 2 * i;
        if (j < queue->length && cs_request_compare(&queue->requests[j], &queue->requests[j + 1]) > 0) {
            j++;
        }
        if (cs_request_compare(&queue->requests[i], &queue->requests[j]) <= 0) {
            break;
        }
        swap(queue, i, j);
        i = j;
    }
    return 0;
}

int peek(Queue *queue, CsRequest *request) {
    if (queue->length == 0) {
        return 1;
    }
    request->timestamp = queue->requests[1].timestamp;
    request->process_id = queue->requests[1].process_id;
    request->replied = queue->requests[1].replied;
    return 0;
}

int cs_request_compare(const CsRequest *a, const CsRequest *b) {
    if (a->timestamp == b->timestamp) {
        if (a->process_id == b->process_id) {
            return 0;
        }
        return a->process_id - b->process_id;
    }
    return a->timestamp - b->timestamp;
}

void swap(Queue *queue, int a, int b) {
    CsRequest tmp;

    tmp.timestamp = queue->requests[a].timestamp;
    tmp.process_id = queue->requests[a].process_id;

    queue->requests[a].timestamp = queue->requests[b].timestamp;
    queue->requests[a].process_id = queue->requests[b].process_id;

    queue->requests[b].timestamp = tmp.timestamp;
    queue->requests[b].process_id = tmp.process_id;
}

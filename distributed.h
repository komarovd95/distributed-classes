#ifndef PA1_DISTRIBUTED_H
#define PA1_DISTRIBUTED_H

#include "ipc.h"
#include "core.h"

void construct_message(Message *message, int message_type, size_t payload_len, const char *payload);

/**
 * Broadcasts message to all other processes.
 *
 * @param state a state of current process
 * @param message_type message type to broadcast
 * @param payload a message payload
 * @return 0 if success
 */
int broadcast_send(ProcessState *state, int message_type, const char *payload);

/**
 * Sends message to concrete receiver.
 *
 * @param state a state of current process
 * @param to a receiver of message
 * @param message_type a type of message
 * @param payload a message payload
 * @return 0 if success
 */
int send_message(ProcessState *state, local_id to, int message_type, size_t payload_len, const char *payload);

/**
 * Receives message from all other processes.
 *
 * @param state a state of current process
 * @param message_type message type to receive
 * @return 0 if success
 */
int receive_from_all(ProcessState *state, int message_type);

int receive_from_any(ProcessState *state, local_id *sender, int *message_type, size_t *payload_len, char *payload);

int receive_from(ProcessState *state, local_id sender, int *message_type, char *payload);

#endif //PA1_DISTRIBUTED_H

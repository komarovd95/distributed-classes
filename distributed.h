#ifndef PA1_DISTRIBUTED_H
#define PA1_DISTRIBUTED_H

#include "ipc.h"
#include "core.h"

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
 * Receives message from all other processes.
 *
 * @param state a state of current process
 * @param message_type message type to receive
 * @return 0 if success
 */
int receive_from_all(ProcessState *state, int message_type);

#endif //PA1_DISTRIBUTED_H

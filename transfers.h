#ifndef PA1_TRANSFERS_H
#define PA1_TRANSFERS_H

#include "banking.h"
#include "core.h"

#define BALANCE_STATE_SIZE (sizeof(balance_t) + sizeof(timestamp_t) + sizeof(balance_t))

/**
 * Serializes transfer order to byte-buffer.
 *
 * @param buffer a byte-buffer
 * @param transfer_order a transfer order to serialize
 * @return size of serialized transfer order
 */
size_t serialize_order(char *buffer, const TransferOrder *transfer_order);

/**
 * Deserialzes transfer order from byte-buffer.
 *
 * @param buffer a byte-buffer
 * @param transfer_order a transfer order
 */
void deserialize_order(const char *buffer, TransferOrder *transfer_order);

/**
 * Processes transfer as source.
 *
 * @param state a state of current process
 * @param transfer_order a transfer order to process
 * @return 0 if success
 */
int process_transfer_out(ProcessState *state, TransferOrder *transfer_order);

/**
 * Processes transfer as target.
 *
 * @param state a state of current process
 * @param transfer_order a transfer order to process
 * @return 0 if success
 */
int process_transfer_in(ProcessState *state, TransferOrder *transfer_order);

/**
 * Synchronizes balance of current process.
 *
 * @param state a state of current process
 * @param sync_time synchronization time
 * @return 0 if success
 */
int sync_balance(ProcessState *state, timestamp_t sync_time);

#endif //PA1_TRANSFERS_H

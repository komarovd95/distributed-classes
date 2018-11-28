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
 * Serializes balance history to byte-buffer.
 *
 * @param buffer a byte-buffer
 * @param history a balance history of current process to serialize
 * @return size of serialized balance history
 */
size_t serialize_history(char *buffer, const BalanceHistory *history);

/**
 * Deserializes balance history from byte-buffer.
 *
 * @param buffer a byte-buffer
 * @param history a balance history to deserialize
 */
void deserialize_history(const char *buffer, BalanceHistory *history);

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
 * @param time a timestamp when transfer has been processed by source
 * @param transfer_order a transfer order to process
 * @return 0 if success
 */
int process_transfer_in(ProcessState *state, timestamp_t time, TransferOrder *transfer_order);

/**
 * Pads balance history of current process.
 *
 * @param history a balance history
 * @param state a current balance state.
 */
void pad_history(BalanceHistory *history, const BalanceState *state);

#endif //PA1_TRANSFERS_H

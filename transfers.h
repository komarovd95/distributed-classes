#ifndef PA1_TRANSFERS_H
#define PA1_TRANSFERS_H

#include "banking.h"
#include "core.h"

#define BALANCE_STATE_SIZE (sizeof(balance_t) + sizeof(timestamp_t) + sizeof(balance_t))

size_t serialize_order(char *buffer, const TransferOrder *transfer_order);

void deserialize_order(const char *buffer, TransferOrder *transfer_order);

void serialize_history(char *buffer, const BalanceHistory *history);

void deserialize_history(const char *buffer, BalanceHistory *history);

int process_transfer_out(ProcessState *state, TransferOrder *transfer_order);

int process_transfer_in(ProcessState *state, timestamp_t time, TransferOrder *transfer_order);

void adjust_history(AllHistory *history);

#endif //PA1_TRANSFERS_H

#ifndef PA1_BANK_UTILS_H
#define PA1_BANK_UTILS_H

#include "ipc.h"
#include "banking.h"
#include "distributed.h"

void set_lamport_time(timestamp_t timestamp);

balance_t get_balance();

void set_balance(balance_t new_balance);

void serialize_order(unsigned char *buffer, TransferOrder *transfer_order);

int init_transfer(ProcessInfo *process_info, TransferOrder *transfer_order);
int receive_banking_event(ProcessInfo *process_info);

int process_transfer_in();
int process_transfer_out();


#endif //PA1_BANK_UTILS_H

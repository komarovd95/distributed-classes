/* IFMO Distributed Class, autumn 2013
 *
 * bank_robbery.c
 * Provided for testing purposes only!
 * In tests another implementation is used.
 */


#include "banking.h"

void bank_robbery(void * parent_data, local_id max_id)
{
    for (int i = 1; i < max_id; ++i) {
        transfer(parent_data, i, i + 1, i);
        if (i % 2) {
            total_sum_snapshot();
        }
    }
    if (max_id > 1) {
        transfer(parent_data, max_id, 1, 1);
        total_sum_snapshot();
    }
}

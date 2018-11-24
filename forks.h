#ifndef PA1_FORKS_H
#define PA1_FORKS_H

/**
 * A structure that represents a fork entity.
 */
typedef struct {
    int is_controlled; ///< 1 means that this fork belongs to current process.
    int is_dirty;      ///< 1 means that this fork is dirty.
    int has_request;   ///< 1 means that this fork was requested.
} Fork;

#endif //PA1_FORKS_H

#ifndef _THREAD_H_
#define _THREAD_H_

#include "constants.h"

struct thread {
    int pid;
    char name[MAX_LINE_LEN + 1];
    int alive;

    struct sample* samples;
    int prev_index;
    char prev_state;
    unsigned long long prev_timestamp;
};

struct sample {
    long long unsigned timestamp;
    char state;
};

void thread_init(struct thread* self, int pid, int max_samples);
void thread_read_sample(struct thread* self, int sample_index);
void thread_free(struct thread* self);

#endif

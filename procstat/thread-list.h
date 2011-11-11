#ifndef _THREAD_LIST_H_
#define _THREAD_LIST_H_

#include "constants.h"
#include "thread.h"

struct thread_list {
    int pid;
    struct thread* main_thread;
    struct thread* threads;
    int max_samples;
    int count;
};

struct thread_list* thread_list_init(int pid, int max_samples);
int thread_list_contains_thread(struct thread_list* self, int thread_pid);
void thread_list_flag_alive(struct thread_list* self, int thread_pid);
int thread_list_update(struct thread_list* self);
void thread_list_read_samples(struct thread_list* self, int sample_index);
void thread_list_print_samples_csv(struct thread_list* self, int collected_samples);
void thread_list_free(struct thread_list* self);

#endif

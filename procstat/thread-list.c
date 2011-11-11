#include "thread-list.h"

#include <stdio.h>
#include <stdlib.h>

extern int quiet;

static void print_csv_state_diff(struct thread* curr_t, int sample_index) {
    struct sample* curr_s = &curr_t->samples[sample_index];
    if (curr_s->state != curr_t->prev_state) {
        // Dont print zero-state time
        if (curr_t->prev_state != 0) {
            printf("%d,%s,%c,%c,%llu,%llu,%d\n",
                    curr_t->pid,
                    curr_t->name,
                    curr_t->prev_state,
                    curr_s->state,
                    curr_t->prev_timestamp,
                    curr_s->timestamp,
                    sample_index - curr_t->prev_index);
        }
        curr_t->prev_state = curr_s->state;
        curr_t->prev_timestamp = curr_s->timestamp;
        curr_t->prev_index = sample_index;
    }
}

struct thread_list* thread_list_init(int pid, int max_samples) {
    struct thread_list* self = malloc(sizeof(struct thread_list));
    self->pid = pid;
    self->count = 0;
    self->threads = NULL;
    self->main_thread = malloc(sizeof(struct thread));
    self->max_samples = max_samples;
    
    thread_init(self->main_thread, pid, max_samples);

    return self;
}

int thread_list_contains_thread(struct thread_list* self, int thread_pid) {
    int t;
    for (t = 0; t < self->count; t++) {
        if (self->threads[t].pid == thread_pid) {
            return 1;
        }
    }
    return 0;
}

void thread_list_flag_alive(struct thread_list* self, int thread_pid) {
    int t;
    for (t = 0; t < self->count; t++) {
        if (self->threads[t].pid == thread_pid) {
            self->threads[t].alive = 1;
            return;
        }
    }
}

int thread_list_update(struct thread_list* self) {
    int task_pids[MAX_CHILD_PIDS];
    int new_task_pids[MAX_CHILD_PIDS];
    int task_count, new_task_count, i, t;

    // flag everything as dead
    for (t = 0; t < self->count; t++) {
        self->threads[t].alive = 0;
    }

    task_count = find_task_pids(self->pid, task_pids);

    if (task_count == -1) {
        return -1;
    }

    new_task_count = 0;
    for (i = 0; i < task_count; i++) {
        int thread_pid = task_pids[i];
        thread_list_flag_alive(self, thread_pid);
        if (!thread_list_contains_thread(self, thread_pid)) {
            new_task_pids[new_task_count++] = thread_pid;
        }
    }
    if (!quiet && new_task_count > 0) {
        printf("%d new threads: ", new_task_count); 
        for (i = 0; i < new_task_count; i++) {
            printf("%d ", new_task_pids[i]);
        }
        printf("\n");
    }

    self->threads = realloc(self->threads, sizeof(struct thread) * (self->count + new_task_count));
    for (i = 0; i < new_task_count; i++) {
        struct thread* t = &self->threads[self->count + i];
        thread_init(t, new_task_pids[i], self->max_samples);
    }
    self->count += new_task_count;

    return 0;
}

void thread_list_read_samples(struct thread_list* self, int sample_index) {
    int t;
    unsigned long long dead_time = gettime();

    if (sample_index >= self->max_samples) {
        fprintf(stderr, "Buffer overrun, max samples %d, trying to sample index %d. Aborting.\n",
                self->max_samples,
                sample_index);
        exit(EXIT_FAILURE);
    }

    thread_read_sample(self->main_thread, sample_index);

    for (t = 0; t < self->count; t++) {
        if (self->threads[t].alive) {
            thread_read_sample(&self->threads[t], sample_index);
        } else {
            self->threads[t].samples[sample_index].timestamp = dead_time;
            self->threads[t].samples[sample_index].state = 'X';
        }
    }
}


void thread_list_print_samples_csv(struct thread_list* self, int collected_samples) {
    int i, t;
    if (!quiet) printf("Printing CSV\n");

    for (t = 0; t < self->count; t++) {
        self->threads[t].prev_index = 0;
        self->threads[t].prev_state = 0;
        self->threads[t].prev_timestamp = 0;
    }

    printf("tid,name,state,new_state,start,end,samples\n");

    for (i = 0; i < collected_samples; i++) {
        print_csv_state_diff(self->main_thread, i);
        for (t = 0; t < self->count; t++) {
            print_csv_state_diff(&(self->threads[t]), i);
        }
    }
}

void thread_list_free(struct thread_list* self) {
    int t;
    for (t = 0; t < self->count; t++) {
        thread_free(&(self->threads[t]));
    }
    free(self);
}


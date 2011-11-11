/**
  This file is part of Android Thread Profiler (ATP)
  
  Copyright (C) 2011, Aki Saarinen.
 
  ATP is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
 
  ATP is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with ATP. If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#ifndef max
    #define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif

#ifndef min
    #define min( a, b ) ( ((a) < (b)) ? (a) : (b) )
#endif

#define MAX_LINE_LEN 255
#define MAX_CHILD_PIDS 255
char buf[MAX_LINE_LEN + 1];
int quiet = 0;
int sleep_period_ms = 10;
int max_time_sec = 10;

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

inline long long unsigned gettime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((long long unsigned)tv.tv_sec * 1000000UL + (long long unsigned)tv.tv_usec) / 1000UL;
}

inline char* read_proc_line(char* filename) {
    buf[MAX_LINE_LEN] = 0;    
    FILE* stat = fopen(filename, "r");
    if (stat == NULL) {
        if (!quiet) printf("Error opening file %s\n", filename);
        return NULL;
    }
    fgets(buf, MAX_LINE_LEN - 1, stat);
    fclose(stat);
    return buf;
}

inline char* readpidstat(int pid) {
    char filename[MAX_LINE_LEN + 1];
    sprintf(filename, "/proc/%d/stat", pid);
    return read_proc_line(filename);
}

inline char* readcpustat() {
    return read_proc_line("/proc/stat");
}

inline char* readpidstatus(int pid) {
    char filename[MAX_LINE_LEN + 1];
    sprintf(filename, "/proc/%d/stat", pid);
    return read_proc_line(filename);
}

inline void getcputimes(int* usertime, int* nicetime, int* systime) {
    char* line = readcpustat();
    sscanf(line, "cpu %d %d %d", usertime, nicetime, systime);
}

int find_task_pids(int pid, int* child_pids) {
    struct dirent* dp;
    DIR* dir;
    int child_pid = 0;
    int found_index = 0;
    char* proc_line;
    char dirname[MAX_LINE_LEN + 1];

    sprintf(dirname, "/proc/%d/task", pid);

    dir = opendir(dirname);
    if (dir == NULL) {
        return -1;
    }

    while((dp = readdir(dir)) != NULL && found_index < MAX_CHILD_PIDS) {
        int child_pid = atoi(dp->d_name);
        if (child_pid > 0 && child_pid != pid) {
            child_pids[found_index++] = child_pid;
        }
    }
    closedir(dir);
    return found_index;
}


//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

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

void thread_init(struct thread* self, int pid, int max_samples) {
    self->pid = pid;
    self->alive = 1;
    self->prev_index = 0;
    self->prev_state = 0;
    self->prev_timestamp = 0;
    self->samples = calloc(max_samples, sizeof(struct sample));
}

void thread_read_sample(struct thread* self, int sample_index) {
    const char* line_start_format = "%d";
    const char* line_after_name_format = "%c";
    int read_pid;
    int name_len;
    char read_state;
    char* line;
    char* line_name_start;
    char* line_after_name;
    struct sample* s = &self->samples[sample_index];

    s->timestamp = gettime();
    line = readpidstat(self->pid);
    if (line != NULL) {
        sscanf(line, line_start_format, &read_pid);
        line_after_name = strrchr(line, ')');
        if (strcmp(self->name,"") == 0) {
            line_name_start = strchr(line, '(') + 1;
            name_len = min(MAX_LINE_LEN, line_after_name - line_name_start);
            strncpy(self->name, line_name_start, name_len);
            self->name[name_len] = 0;
        }
        line_after_name++;
        line_after_name++;
        sscanf(line_after_name, line_after_name_format, &read_state);
        s->state = read_state;
    } else {
        self->alive = 0;
        s->state = 'X';
        if (!quiet) printf("Read failed for thread %d at sample %d, marking dead\n", self->pid, sample_index);
    }
}

void thread_free(struct thread* self) {
    free(self->samples);
}

struct thread_list {
    int pid;
    struct thread* main_thread;
    struct thread* threads;
    int max_samples;
    int count;
};

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

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

int periodically_poll_thread_states(int pid) {
    int t, i, collected_samples;
    struct thread_list* tl;
    int prev_index;
    unsigned long long start_time, prev_time, curr_time;
    const unsigned long PRINT_TIME_INTERVAL = 2000;
    int max_samples;
   
   
    if (sleep_period_ms == 0) {
        // Around 3 times per ms
        max_samples = max_time_sec * 1000 * 3;
    } else {
        max_samples = (max_time_sec * 1000) / sleep_period_ms;
    }
    if (!quiet) {
        printf("Time: %d sec, Sleep: %d ms, Max samples: %d\n", 
                max_time_sec,
                sleep_period_ms,
                max_samples);
    }

    start_time = gettime();

    tl = thread_list_init(pid, max_samples);
    if (thread_list_update(tl)) {
        fprintf(stderr, "Error while updating thread list, invalid PID? (%d)\n", pid);
        return EXIT_FAILURE;
    }

    prev_index = 0;
    prev_time = gettime();
    for (i = 0; i < tl->max_samples; i++) {
        curr_time = gettime();
        thread_list_update(tl);
        thread_list_read_samples(tl, i);
        if (curr_time - prev_time >= PRINT_TIME_INTERVAL) {
            if (!quiet) {
                printf("Collected %d samples in %llu ms\n",
                    i - prev_index,
                    curr_time - prev_time);
            }
            prev_time = curr_time;
            prev_index = i;
        }
        if (sleep_period_ms != 0) {
            usleep(sleep_period_ms * 1000);
        }

        if (curr_time - start_time >= max_time_sec * 1000) {
            if (!quiet) printf("Time limit exceeded, stopping\n");
            break;
        }
    }
    collected_samples = i;
    if (!quiet) {
        printf("Thread count for PID %d: %d\n", tl->pid, tl->count);
        printf(" - Main thread name: %s\n", tl->main_thread->name);
        for (t = 0; t < tl->count; t++) {
            printf(" - Thread %d: PID %d, name '%s'\n", t, tl->threads[t].pid, tl->threads[t].name);
        }
        printf("Collected %d samples (max %d)\n",
                collected_samples,
                max_samples);
    }
    thread_list_print_samples_csv(tl, collected_samples);
    thread_list_free(tl);

    return EXIT_SUCCESS;
}

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

int arg_matches(char *arg, const char *flag)
{
	return (strncmp(arg, flag, strlen(flag)) == 0) ? 1 : 0;
}

int parse_args_and_run(int argc, char** argv) {
    int pid;
    int arg_pos;

    for (arg_pos = 1; arg_pos < argc - 1; arg_pos++) {
        if (arg_matches(argv[arg_pos], "-q")) {
            quiet = 1;
        } else if (arg_matches(argv[arg_pos], "-s")) {
            if (argc <= arg_pos + 1) {
                fprintf(stderr, "No parameter given to -s");
                return EXIT_FAILURE;
            }
            ++arg_pos;
            sleep_period_ms = atoi(argv[arg_pos]);
        } else if (arg_matches(argv[arg_pos], "-t")) {
            if (argc <= arg_pos + 1) {
                fprintf(stderr, "No parameter given to -t");
                return EXIT_FAILURE;
            }
            ++arg_pos;
            max_time_sec = atoi(argv[arg_pos]);
        } else {
            fprintf(stderr, "Unrecognized option '%s'\n",
                    argv[arg_pos]);
            return EXIT_FAILURE;
        }
    }

    if (arg_pos >= argc) {
        fprintf(stderr, "Missing argument PID\n");
        return EXIT_FAILURE;
    }

    pid = atoi(argv[arg_pos]);
    if (pid == 0) {
        fprintf(stderr, "Invalid PID '%s', converted to %d\n",
                argv[arg_pos], 
                pid);
        return EXIT_FAILURE;
    }

    return periodically_poll_thread_states(pid);
}

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "Usage: %s [-q] [-s sleep_period_ms] PID\n",
                argv[0]);
        return EXIT_FAILURE;
    } else {
        return parse_args_and_run(argc, argv);
    }
}

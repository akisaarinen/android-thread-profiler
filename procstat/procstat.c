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
#include <string.h>

#include "constants.h"
#include "thread.h"
#include "thread-list.h"
#include "read-proc.h"

int quiet = 0;
int sleep_period_ms = 10;
int max_time_sec = 10;

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

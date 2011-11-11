#include "thread.h"
#include "read-proc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int quiet;

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



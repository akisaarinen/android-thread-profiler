#include <stdio.h>
#include <sys/time.h>
#include <dirent.h>

#include "constants.h"
#include "read-proc.h"

char buf[MAX_LINE_LEN + 1];
extern int quiet;

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

char* readcpustat() {
    return read_proc_line("/proc/stat");
}

char* readpidstatus(int pid) {
    char filename[MAX_LINE_LEN + 1];
    sprintf(filename, "/proc/%d/stat", pid);
    return read_proc_line(filename);
}

char* readpidstat(int pid) {
    char filename[MAX_LINE_LEN + 1];
    sprintf(filename, "/proc/%d/stat", pid);
    return read_proc_line(filename);
}

void getcputimes(int* usertime, int* nicetime, int* systime) {
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



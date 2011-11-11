#ifndef _READ_PROC_H_
#define _READ_PROC_H_

long long unsigned gettime();
char* read_proc_line(char* filename);
char* readpidstat(int pid);
char* readcpustat();
char* readpidstatus(int pid);
void getcputimes(int* usertime, int* nicetime, int* systime);
int find_task_pids(int pid, int* child_pids);

#endif

#ifndef PRAKBS23_PROCESS_LIST_H
#define PRAKBS23_PROCESS_LIST_H

#include <stdlib.h>

void add_process(pid_t pid);

void remove_process(pid_t pid);

int terminate_all_processes();

#endif //PRAKBS23_PROCESS_LIST_H

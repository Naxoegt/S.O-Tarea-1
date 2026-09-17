#ifndef JOBS_H
#define JOBS_H

#include "defs.h"


void jobs_init(void);


void jobs_add(pid_t pid, const char *cmd, int is_bg);


void jobs_print(void);

int builtin_pmon(char **args);

void jobs_cleanup(void);

#endif
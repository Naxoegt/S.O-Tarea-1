#ifndef JOBS_H
#define JOBS_H

#include "defs.h"


void jobs_init(void);

int jobs_add(pid_t pid, const char *cmd, int is_bg);

void jobs_print(void);

Job *jobs_get_head(void);

void jobs_remove(pid_t pid);

void jobs_mark_finished(pid_t pid, int status);

void jobs_notify_and_clean(void);

int builtin_pmon(char **args);

void jobs_cleanup(void);

#endif
#ifndef SIGNALS_H
#define SIGNALS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <signal.h>

void signals_init_shell(void);

void signals_setup_child(void);

void signals_setup_sigchld(void (*handler)(int));

void sigchld_handler(int sig);

#endif
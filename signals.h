#ifndef SIGNALS_H
#define SIGNALS_H

#define _GNU_SOURCE
#include <signal.h>

void signals_init_shell(void);

void signals_setup_child(void);

void signals_setup_sigchld(void (*handler)(int));

#endif
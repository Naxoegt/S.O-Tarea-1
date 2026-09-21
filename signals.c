#include "signals.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void signals_init_shell(void) {
    struct sigaction sa_ignore;
    sa_ignore.sa_handler = SIG_IGN;
    sigemptyset(&sa_ignore.sa_mask);
    sa_ignore.sa_flags = 0;

    /* Ignorar interrupciones y suspensiones de terminal */
    if (sigaction(SIGINT, &sa_ignore, NULL) < 0) {
        perror("mishell: error configurando SIGINT");
    }
    if (sigaction(SIGQUIT, &sa_ignore, NULL) < 0) {
        perror("mishell: error configurando SIGQUIT");
    }
    if (sigaction(SIGTSTP, &sa_ignore, NULL) < 0) {
        perror("mishell: error configurando SIGTSTP");
    }
    if (sigaction(SIGTTIN, &sa_ignore, NULL) < 0) {
        perror("mishell: error configurando SIGTTIN");
    }
    if (sigaction(SIGTTOU, &sa_ignore, NULL) < 0) {
        perror("mishell: error configurando SIGTTOU");
    }
}
 
void signals_setup_child(void) {
    struct sigaction sa_default;
    sa_default.sa_handler = SIG_DFL;
    sigemptyset(&sa_default.sa_mask);
    sa_default.sa_flags = 0;

    sigaction(SIGINT, &sa_default, NULL);
    sigaction(SIGQUIT, &sa_default, NULL);
    sigaction(SIGTSTP, &sa_default, NULL);
    sigaction(SIGTTIN, &sa_default, NULL);
    sigaction(SIGTTOU, &sa_default, NULL);
}

void signals_setup_sigchld(void (*handler)(int)) {
    struct sigaction sa_chld;
    sa_chld.sa_handler = handler ? handler : SIG_DFL;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa_chld, NULL) < 0) {
        perror("mishell: error configurando SIGCHLD");
    }
}

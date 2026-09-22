#include "defs.h"
#include "parser.h"
#include "executor.h"
#include "signals.h"
#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

static bool is_blank_line(const char *str) {
    if (!str) return true;
    while (*str != '\0') {
        if (!isspace((unsigned char)*str)) {
            return false;
        }
        str++;
    }
    return true;
}

static void print_prompt(void) {
    char cwd[PROMPT_MAX_LEN];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("miShell:%s$ ", cwd);
    } else {
        printf("miShell:$ ");
    }
    fflush(stdout);
}

int main(void) {
    signals_init_shell();
    signals_setup_sigchld(sigchld_handler);
    jobs_init();
    executor_init();
    while (1) {
        jobs_notify_and_clean();
        print_prompt();
        char *line = read_line();

        if (line == NULL) {
            printf("\n");
            jobs_cleanup();
            exit(EXIT_SUCCESS);
        }

        if (is_blank_line(line)) {
            free(line);
            continue;
        }
        Pipeline *pipeline = parse_command_line(line);
        if (pipeline == NULL) {
            free(line);
            continue;
        }
        execute_pipeline(pipeline, line);
        free_pipeline(pipeline);
        free(line);
    }

    return EXIT_SUCCESS;
}

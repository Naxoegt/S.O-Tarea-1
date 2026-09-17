#include "executor.h"
#include "parser.h"
#include "signals.h"
#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

static pid_t shell_pgid;


void executor_init(void) {
    shell_pgid = getpgrp();
}


bool is_builtin(const char *cmd_name) {
    if (!cmd_name) return false;
    return (strcmp(cmd_name, "cd") == 0 ||
            strcmp(cmd_name, "exit") == 0 ||
            strcmp(cmd_name, "jobs") == 0 ||
            strcmp(cmd_name, "pmon") == 0);
}


static int apply_redirections(const SimpleCommand *cmd) {
    if (cmd->input_file != NULL) {
        int fd_in = open(cmd->input_file, O_RDONLY);
        if (fd_in < 0) {
            perror("mishell: error al abrir archivo de entrada");
            return -1;
        }
        if (dup2(fd_in, STDIN_FILENO) < 0) {
            perror("mishell: error en dup2 para entrada estándar");
            close(fd_in);
            return -1;
        }
        close(fd_in);
    }

    if (cmd->output_file != NULL) {
        int fd_out = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_out < 0) {
            perror("mishell: error al abrir archivo de salida");
            return -1;
        }
        if (dup2(fd_out, STDOUT_FILENO) < 0) {
            perror("mishell: error en dup2 para salida estándar");
            close(fd_out);
            return -1;
        }
        close(fd_out);
    }

    if (cmd->append_file != NULL) {
        int fd_app = open(cmd->append_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd_app < 0) {
            perror("mishell: error al abrir archivo para append");
            return -1;
        }
        if (dup2(fd_app, STDOUT_FILENO) < 0) {
            perror("mishell: error en dup2 para salida estándar (append)");
            close(fd_app);
            return -1;
        }
        close(fd_app);
    }

    return 0;
}

int execute_builtin(SimpleCommand *cmd, Pipeline *pipeline, char *raw_line) {
    if (!cmd || cmd->argc == 0) return 0;
    int saved_stdin = -1;
    int saved_stdout = -1;
    bool has_redirection = (cmd->input_file != NULL || cmd->output_file != NULL || cmd->append_file != NULL);

    if (has_redirection) {
        saved_stdin = dup(STDIN_FILENO);
        saved_stdout = dup(STDOUT_FILENO);
        if (apply_redirections(cmd) < 0) {
            if (saved_stdin >= 0) { dup2(saved_stdin, STDIN_FILENO); close(saved_stdin); }
            if (saved_stdout >= 0) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
            return 1;
        }
    }

    int ret_code = 0;

    if (strcmp(cmd->args[0], "cd") == 0) {
        const char *target = NULL;
        if (cmd->argc == 1 || strcmp(cmd->args[1], "~") == 0) {
            target = getenv("HOME");
            if (!target) {
                fprintf(stderr, "mishell: cd: variable HOME no definida\n");
                ret_code = 1;
            }
        } else {
            target = cmd->args[1];
        }

        if (target && chdir(target) != 0) {
            perror("mishell: cd");
            ret_code = 1;
        }
    }
    else if (strcmp(cmd->args[0], "exit") == 0) {
        int exit_val = 0;
        if (cmd->argc > 1) {
            exit_val = atoi(cmd->args[1]);
        }
        if (has_redirection) {
            if (saved_stdin >= 0) close(saved_stdin);
            if (saved_stdout >= 0) close(saved_stdout);
        }
        if (pipeline) free_pipeline(pipeline);
        if (raw_line) free(raw_line);
        jobs_cleanup();
        exit(exit_val);
    }
    else if (strcmp(cmd->args[0], "jobs") == 0) {
        jobs_print();
        ret_code = 0;
    }
    else if (strcmp(cmd->args[0], "pmon") == 0) {
        ret_code = builtin_pmon(cmd->args);
    }
    if (has_redirection) {
        if (saved_stdin >= 0) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        if (saved_stdout >= 0) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
    }

    return ret_code;
}


static int execute_single_external(Pipeline *pipeline, const char *raw_line) {
    SimpleCommand *cmd = &pipeline->commands[0];

    pid_t pid = fork();
    if (pid < 0) {
        perror("mishell: error al realizar fork");
        return -1;
    }

    if (pid == 0) {
        setpgid(0, 0);

        if (!pipeline->is_background && isatty(STDIN_FILENO)) {
            tcsetpgrp(STDIN_FILENO, getpid());
        }

        signals_setup_child();

        if (apply_redirections(cmd) < 0) {
            _exit(EXIT_FAILURE);
        }
        execvp(cmd->args[0], cmd->args);
        perror("mishell");
        _exit(127);
    }
    setpgid(pid, pid);

    if (pipeline->is_background) {
        printf("[JOB_STUB] PID: %d\n", (int)pid);
        jobs_add(pid, raw_line, 1);
        return 0;
    } else {
        if (isatty(STDIN_FILENO)) {
            tcsetpgrp(STDIN_FILENO, pid);
        }

        int status = 0;
        if (waitpid(pid, &status, WUNTRACED) < 0) {
            if (errno != ECHILD) {
                perror("mishell: error en waitpid");
            }
        }

        if (isatty(STDIN_FILENO)) {
            tcsetpgrp(STDIN_FILENO, shell_pgid);
        }

        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
}

static int execute_pipeline_arbitrary(Pipeline *pipeline, const char *raw_line) {
    int n = pipeline->cmd_count;
    int (*pipes)[2] = malloc((n - 1) * sizeof(int[2]));
    if (!pipes) {
        perror("mishell: error de memoria para pipes");
        return -1;
    }

    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("mishell: error creando pipe");
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            free(pipes);
            return -1;
        }
    }

    pid_t *pids = malloc(n * sizeof(pid_t));
    if (!pids) {
        perror("mishell: error de memoria para pids");
        for (int j = 0; j < n - 1; j++) {
            close(pipes[j][0]);
            close(pipes[j][1]);
        }
        free(pipes);
        return -1;
    }

    pid_t pgid = 0;

    for (int i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("mishell: error al crear proceso en pipeline");
            break;
        }

        if (pids[i] == 0) {

            if (i == 0) {
                pgid = getpid();
            }
            setpgid(0, pgid);

            if (!pipeline->is_background && isatty(STDIN_FILENO)) {
                tcsetpgrp(STDIN_FILENO, pgid);
            }
            signals_setup_child();
            if (i > 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                    perror("mishell: dup2 entrada pipe");
                    _exit(EXIT_FAILURE);
                }
            }

            if (i < n - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                    perror("mishell: dup2 salida pipe");
                    _exit(EXIT_FAILURE);
                }
            }

            for (int j = 0; j < n - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            if (apply_redirections(&pipeline->commands[i]) < 0) {
                _exit(EXIT_FAILURE);
            }
            if (is_builtin(pipeline->commands[i].args[0])) {
                int ret = execute_builtin(&pipeline->commands[i], pipeline, NULL);
                _exit(ret);
            }

            execvp(pipeline->commands[i].args[0], pipeline->commands[i].args);
            perror("mishell");
            _exit(127);
        }
        if (i == 0) {
            pgid = pids[0];
        }
        setpgid(pids[i], pgid);
    }

    for (int j = 0; j < n - 1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }
    free(pipes);

    if (pipeline->is_background) {
        printf("[JOB_STUB] PID: %d\n", (int)pids[0]);
        jobs_add(pids[0], raw_line, 1);
        free(pids);
        return 0;
    }

    if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, pgid);
    }
    int last_status = 0;
    for (int i = 0; i < n; i++) {
        int status = 0;
        waitpid(pids[i], &status, WUNTRACED);
        if (i == n - 1) {
            last_status = status;
        }
    }

    if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, shell_pgid);
    }

    free(pids);
    return WIFEXITED(last_status) ? WEXITSTATUS(last_status) : 1;
}


int execute_pipeline(Pipeline *pipeline, const char *raw_line) {
    if (!pipeline || pipeline->cmd_count == 0) return 0;
    if (pipeline->cmd_count == 1) {
        if (is_builtin(pipeline->commands[0].args[0])) {
            return execute_builtin(&pipeline->commands[0], pipeline, (char *)raw_line);
        }
        return execute_single_external(pipeline, raw_line);
    }

    return execute_pipeline_arbitrary(pipeline, raw_line);
}

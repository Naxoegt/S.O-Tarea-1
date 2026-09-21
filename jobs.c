#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Job *job_list_head = NULL;
static int next_job_id = 1;

void jobs_init(void) {
    job_list_head = NULL;
    next_job_id = 1;
}

void jobs_add(pid_t pid, const char *cmd, int is_bg) {
    (void)is_bg;

    Job *new_job = malloc(sizeof(Job));
    if (!new_job) {
        perror("mishell: error al asignar memoria para job");
        return;
    }

    new_job->job_id = next_job_id++;
    new_job->pid = pid;
    new_job->cmd_line = cmd ? strdup(cmd) : strdup("<desconocido>");
    new_job->state = strdup("ejecutando");
    new_job->next = NULL;

    if (job_list_head == NULL) {
        job_list_head = new_job;
    } else {
        Job *curr = job_list_head;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = new_job;
    }
}

void jobs_print(void) {
    if (job_list_head == NULL) {
        printf("mishell: no hay trabajos en segundo plano registrados.\n");
        return;
    }

    Job *curr = job_list_head;
    while (curr != NULL) {
        printf("[%d]  PID: %-6d  Estado: %-12s  Comando: %s\n",
               curr->job_id, (int)curr->pid, curr->state, curr->cmd_line);
        curr = curr->next;
    }
}

#include "pmon.h"

// retorna la cabeza de la lista de jobs
Job *jobs_get_head(void) {
    return job_list_head;
}

// elimina un proceso de la lista si ya termino
void jobs_remove(pid_t pid) {
    Job *curr = job_list_head;
    Job *prev = NULL;

    while (curr != NULL) {
        if (curr->pid == pid) {
            if (prev == NULL) {
                job_list_head = curr->next;
            } else {
                prev->next = curr->next;
            }
            if (curr->cmd_line) free(curr->cmd_line);
            if (curr->state) free(curr->state);
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

// ejecuta el comando interno pmon
int builtin_pmon(char **args) {
    return pmon_run(args);
}

void jobs_cleanup(void) {
    Job *curr = job_list_head;
    while (curr != NULL) {
        Job *temp = curr->next;
        if (curr->cmd_line) free(curr->cmd_line);
        if (curr->state) free(curr->state);
        free(curr);
        curr = temp;
    }
    job_list_head = NULL;
}

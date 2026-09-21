#ifndef DEFS_H
#define DEFS_H

#define _GNU_SOURCE
#include <sys/types.h>
#include <stdbool.h>

/* Limites para evitar realloc innecesario
MAX_ARGS: Número máximo de argumentos por comando
MAX_COMMANDS: Número máximo de comandos en un pipeline
MAX_LINE_LEN: Longitud máxima de una línea de entrada
PROMPT_MAX_LEN: Longitud máxima del prompt */
#define MAX_ARGS 128
#define MAX_COMMANDS 64
#define MAX_LINE_LEN 4096
#define PROMPT_MAX_LEN 1024

/* Representa solo un comando simple dentro de un pipeline: el argv ya
   tokenizado más las redirecciones que le aplican a ese comando
   (no a todos el pipeline). Por eso input_file/output_file/append_file  */
typedef struct SimpleCommand {
    char **args;         
    int argc;            
    char *input_file; // <
    char *output_file; // >
    char *append_file; // >>  
} SimpleCommand;

typedef struct Pipeline {
    SimpleCommand *commands; // arreglo de tamaño
    int cmd_count; // número de comandos encadenados por pipes        
    bool is_background; // si la linea terminó en & -> true, sino false
} Pipeline;

// nodo de la lista enlazada de jobs
typedef struct Job {
    int job_id;              
    pid_t pid;               
    char *cmd_line;         
    char *state;             
    struct Job *next;        
} Job;

#endif

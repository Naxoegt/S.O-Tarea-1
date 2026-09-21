#ifndef DEFS_H
#define DEFS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/types.h>
#include <stdbool.h>

#define MAX_ARGS 128
#define MAX_COMMANDS 64
#define MAX_LINE_LEN 4096
#define PROMPT_MAX_LEN 1024

typedef struct SimpleCommand {
    char **args;         
    int argc;            
    char *input_file;    
    char *output_file;   
    char *append_file;   
} SimpleCommand;


typedef struct Pipeline {
    SimpleCommand *commands; 
    int cmd_count;         
    bool is_background;      
} Pipeline;


typedef struct Job {
    int job_id;              
    pid_t pid;               
    char *cmd_line;         
    char *state;             
    struct Job *next;        
} Job;

#endif

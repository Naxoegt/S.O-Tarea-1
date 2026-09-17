#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char *read_line(void) {
    char *line = NULL;
    size_t bufsize = 0;
    ssize_t characters_read = getline(&line, &bufsize, stdin);

    if (characters_read == -1) {
        free(line);
        return NULL;
    }
    while (characters_read > 0 && 
          (line[characters_read - 1] == '\n' || line[characters_read - 1] == '\r')) {
        line[characters_read - 1] = '\0';
        characters_read--;
    }

    return line;
}

static void add_token(char ***tokens, int *count, int *capacity, const char *token_str) {
    if (*count + 1 >= *capacity) {
        *capacity = (*capacity == 0) ? 16 : (*capacity * 2);
        *tokens = realloc(*tokens, *capacity * sizeof(char *));
        if (!*tokens) {
            perror("mishell: error al asignar memoria para tokens");
            exit(EXIT_FAILURE);
        }
    }
    (*tokens)[*count] = strdup(token_str);
    (*count)++;
    (*tokens)[*count] = NULL;
}

static void free_tokens(char **tokens, int count) {
    if (!tokens) return;
    for (int i = 0; i < count; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

static char **tokenize_line(const char *raw_line, int *token_count) {
    char **tokens = NULL;
    int count = 0;
    int capacity = 0;

    const char *p = raw_line;
    char buffer[MAX_LINE_LEN];
    int buf_idx = 0;

    while (*p != '\0') {
        if (isspace((unsigned char)*p)) {
            p++;
            continue;
        }
        if (*p == '|') {
            add_token(&tokens, &count, &capacity, "|");
            p++;
            continue;
        }
        if (*p == '&') {
            add_token(&tokens, &count, &capacity, "&");
            p++;
            continue;
        }
        if (*p == '<') {
            add_token(&tokens, &count, &capacity, "<");
            p++;
            continue;
        }
        if (*p == '>') {
            if (*(p + 1) == '>') {
                add_token(&tokens, &count, &capacity, ">>");
                p += 2;
            } else {
                add_token(&tokens, &count, &capacity, ">");
                p++;
            }
            continue;
        }

        buf_idx = 0;
        while (*p != '\0' && !isspace((unsigned char)*p)) {
            if (*p == '|' || *p == '&' || *p == '<' || *p == '>') {
                break;
            }

            if (*p == '\'' || *p == '\"') {
                char quote = *p++;
                while (*p != '\0' && *p != quote) {
                    if (buf_idx < (int)sizeof(buffer) - 1) {
                        buffer[buf_idx++] = *p;
                    }
                    p++;
                }
                if (*p == quote) {
                    p++; 
                }
            } else {
                if (buf_idx < (int)sizeof(buffer) - 1) {
                    buffer[buf_idx++] = *p;
                }
                p++;
            }
        }

        buffer[buf_idx] = '\0';
        if (buf_idx > 0) {
            add_token(&tokens, &count, &capacity, buffer);
        }
    }

    *token_count = count;
    return tokens;
}

static void cleanup_simple_command(SimpleCommand *cmd) {
    if (!cmd) return;
    if (cmd->args) {
        for (int i = 0; i < cmd->argc; i++) {
            free(cmd->args[i]);
        }
        free(cmd->args);
        cmd->args = NULL;
    }
    if (cmd->input_file) {
        free(cmd->input_file);
        cmd->input_file = NULL;
    }
    if (cmd->output_file) {
        free(cmd->output_file);
        cmd->output_file = NULL;
    }
    if (cmd->append_file) {
        free(cmd->append_file);
        cmd->append_file = NULL;
    }
}

void free_pipeline(Pipeline *pipeline) {
    if (!pipeline) return;
    if (pipeline->commands) {
        for (int i = 0; i < pipeline->cmd_count; i++) {
            cleanup_simple_command(&pipeline->commands[i]);
        }
        free(pipeline->commands);
        pipeline->commands = NULL;
    }
    free(pipeline);
}

Pipeline *parse_command_line(const char *raw_line) {
    if (!raw_line) return NULL;

    int token_count = 0;
    char **tokens = tokenize_line(raw_line, &token_count);

    if (token_count == 0) {
        free_tokens(tokens, token_count);
        return NULL;
    }

    bool is_bg = false;
    if (strcmp(tokens[token_count - 1], "&") == 0) {
        is_bg = true;
        free(tokens[token_count - 1]);
        tokens[token_count - 1] = NULL;
        token_count--;
    }

    if (token_count == 0) {
        free_tokens(tokens, token_count);
        return NULL;
    }

    if (strcmp(tokens[0], "|") == 0 || strcmp(tokens[token_count - 1], "|") == 0) {
        fprintf(stderr, "mishell: error de sintaxis cerca del token no esperado '|'\n");
        free_tokens(tokens, token_count);
        return NULL;
    }

    int num_cmds = 1;
    for (int i = 0; i < token_count; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            if (i + 1 < token_count && strcmp(tokens[i + 1], "|") == 0) {
                fprintf(stderr, "mishell: error de sintaxis cerca del token no esperado '||'\n");
                free_tokens(tokens, token_count);
                return NULL;
            }
            num_cmds++;
        }
    }

    Pipeline *pipeline = malloc(sizeof(Pipeline));
    if (!pipeline) {
        perror("mishell: error de memoria al crear pipeline");
        free_tokens(tokens, token_count);
        return NULL;
    }

    pipeline->is_background = is_bg;
    pipeline->cmd_count = num_cmds;
    pipeline->commands = calloc(num_cmds, sizeof(SimpleCommand));
    if (!pipeline->commands) {
        perror("mishell: error de memoria al crear comandos del pipeline");
        free(pipeline);
        free_tokens(tokens, token_count);
        return NULL;
    }

    int cmd_idx = 0;
    int arg_cap = 16;
    pipeline->commands[cmd_idx].args = malloc(arg_cap * sizeof(char *));
    pipeline->commands[cmd_idx].argc = 0;

    for (int i = 0; i < token_count; i++) {
        char *tok = tokens[i];

        if (strcmp(tok, "|") == 0) {
            pipeline->commands[cmd_idx].args[pipeline->commands[cmd_idx].argc] = NULL;

            if (pipeline->commands[cmd_idx].argc == 0) {
                fprintf(stderr, "mishell: error de sintaxis: comando vacío en tubería\n");
                free_pipeline(pipeline);
                free_tokens(tokens, token_count);
                return NULL;
            }

            cmd_idx++;
            arg_cap = 16;
            pipeline->commands[cmd_idx].args = malloc(arg_cap * sizeof(char *));
            pipeline->commands[cmd_idx].argc = 0;
            continue;
        }

        if (strcmp(tok, "<") == 0) {
            if (i + 1 >= token_count || tokens[i + 1][0] == '|' || tokens[i + 1][0] == '<' || 
                tokens[i + 1][0] == '>') {
                fprintf(stderr, "mishell: error de sintaxis: falta archivo tras '<'\n");
                free_pipeline(pipeline);
                free_tokens(tokens, token_count);
                return NULL;
            }
            if (pipeline->commands[cmd_idx].input_file) {
                free(pipeline->commands[cmd_idx].input_file);
            }
            pipeline->commands[cmd_idx].input_file = strdup(tokens[++i]);
            continue;
        }

        if (strcmp(tok, ">") == 0) {
            if (i + 1 >= token_count || tokens[i + 1][0] == '|' || tokens[i + 1][0] == '<' || 
                tokens[i + 1][0] == '>') {
                fprintf(stderr, "mishell: error de sintaxis: falta archivo tras '>'\n");
                free_pipeline(pipeline);
                free_tokens(tokens, token_count);
                return NULL;
            }
            if (pipeline->commands[cmd_idx].output_file) {
                free(pipeline->commands[cmd_idx].output_file);
            }
            pipeline->commands[cmd_idx].output_file = strdup(tokens[++i]);
            continue;
        }

        if (strcmp(tok, ">>") == 0) {
            if (i + 1 >= token_count || tokens[i + 1][0] == '|' || tokens[i + 1][0] == '<' || 
                tokens[i + 1][0] == '>') {
                fprintf(stderr, "mishell: error de sintaxis: falta archivo tras '>>'\n");
                free_pipeline(pipeline);
                free_tokens(tokens, token_count);
                return NULL;
            }
            if (pipeline->commands[cmd_idx].append_file) {
                free(pipeline->commands[cmd_idx].append_file);
            }
            pipeline->commands[cmd_idx].append_file = strdup(tokens[++i]);
            continue;
        }

        if (pipeline->commands[cmd_idx].argc + 1 >= arg_cap) {
            arg_cap *= 2;
            pipeline->commands[cmd_idx].args = realloc(pipeline->commands[cmd_idx].args, 
                                                       arg_cap * sizeof(char *));
        }
        pipeline->commands[cmd_idx].args[pipeline->commands[cmd_idx].argc++] = strdup(tok);
    }

    pipeline->commands[cmd_idx].args[pipeline->commands[cmd_idx].argc] = NULL;
    if (pipeline->commands[cmd_idx].argc == 0) {
        fprintf(stderr, "mishell: error de sintaxis: comando vacío en tubería\n");
        free_pipeline(pipeline);
        free_tokens(tokens, token_count);
        return NULL;
    }

    free_tokens(tokens, token_count);
    return pipeline;
}

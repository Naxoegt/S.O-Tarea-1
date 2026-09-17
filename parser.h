#ifndef PARSER_H
#define PARSER_H

#include "defs.h"

char *read_line(void);

Pipeline *parse_command_line(const char *raw_line);

void free_pipeline(Pipeline *pipeline);

#endif

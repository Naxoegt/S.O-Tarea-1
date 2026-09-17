#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "defs.h"

void executor_init(void);

bool is_builtin(const char *cmd_name);


int execute_builtin(SimpleCommand *cmd, Pipeline *pipeline, char *raw_line);


int execute_pipeline(Pipeline *pipeline, const char *raw_line);

#endif 
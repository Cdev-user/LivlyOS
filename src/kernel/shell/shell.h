#ifndef SHELL_H
#define SHELL_H
#include "../kernel.h"

#define MAX_INPUT 10000

void shell_init();
void shell_run();
void shell_execute(char *input);
void shell_execute_slash(char *cmd, char *arg);


int str_equal(char *a, char *b);

#endif
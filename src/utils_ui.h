#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

void trim(char *text);
int  valid_date(char *date);
int  valid_explicit(char *explicito);
int  valid_positive_int(char *buff);
int  valid_positive_double(char *buff);
int  prompt_text(const char *msg, char *buf, size_t buf_size);

#endif
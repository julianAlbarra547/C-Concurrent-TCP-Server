#ifndef CSV_READER_H
#define CSV_READER_H
#include <stdio.h>
typedef struct row{
    int id;
    char title[512];
    short rank;
    char date[16];
    char artist[2048];
    char url[128];
    long long streams;
    char album[512];
    double duration;
    char explicito[8];
} Row;

FILE *open_csv(const char *filename);
Row *read_csv(FILE *file, long offset);
void print_row(Row *row);
void close_csv(FILE *file);
void skip_header(FILE *file);

#endif
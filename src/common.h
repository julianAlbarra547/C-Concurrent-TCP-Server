#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>

#define PORT 8080
typedef struct {
    char title[512];
    char artist[2048];
} Query;

int rcv_all(int sockfd, void *buffer, size_t length);
int send_all(int sockfd, void *buffer, size_t length);
#endif
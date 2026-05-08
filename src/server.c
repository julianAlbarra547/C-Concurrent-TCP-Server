#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include "common.h"
#include "hash.h"

#define BACKLOG 32

int main(){
    int sockfd, valid, addr_len, identify;
    struct sockaddr_in server;
    Query query;
    
    FILE *csv = fopen(CSV_FILE, "r+");
    if (csv == NULL) {
        fprintf(stderr, "Error opening CSV file: %s\n", CSV_FILE);
        return 1;
    }

    FILE *check = fopen(TABLE_IDX, "rb");
    if (check == NULL) {
        printf("Index file not found. Building index...\n");
        valid = build_index(CSV_FILE, TABLE_IDX, ENTRIES_BIN);
        if (valid == -1) {
            fprintf(stderr, "Error building index\n");
            return 1;
        }
    } else {
        printf("Index file found. Skipping index build.\n");
        fclose(check);
    }

    long table[HASH_TABLE_SIZE];
    if (load_table(TABLE_IDX, table) != 0) {
        fprintf(stderr, "Error cargando índice\n");
        return 1;
    }

    FILE *entries_file = fopen(ENTRIES_BIN, "rb+");;
    if (!entries_file) {
        fprintf(stderr, "Error abriendo archivo de entradas\n");
        return 1;
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
        perror("Socket creation failed");
        exit(-1);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server.sin_zero), 8);
    addr_len = sizeof(struct sockaddr);

    valid = bind(sockfd, (struct sockaddr *)&server, addr_len);

    if (valid < 0){
        perror("Bind failed");
        exit(-1);
    }

    valid = listen(sockfd, BACKLOG);
    if (valid < 0){
        perror("Listen Failed");
        exit(-1);
    }

    return 0;
}
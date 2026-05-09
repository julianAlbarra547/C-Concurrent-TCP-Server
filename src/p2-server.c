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
#include <fcntl.h>           
#include <sys/stat.h>        
#include <semaphore.h>
#include <pthread.h>

#define BACKLOG 32
#define MAX_CLIENTS 32
#define PATH_LOG "../data/log/log.txt"

sem_t *client_count_sem;

typedef struct query_args{
    int client_sock;
    long *table_hash;
    FILE *entries_file;
    FILE *csv;
    FILE *log_file;
} ThreadArgs;

void *process_query(void *arg){
    ThreadArgs *args =  (ThreadArgs *)arg;
    int client_sock = args->client_sock;
    long *table_hash = args->table_hash;
    FILE *entries_file = args->entries_file;
    FILE *csv = args->csv;
    FILE *log_file = args->log_file;

    free(arg);

    Query query;
    int identify;
}

int main(){
    int sockfd, valid, identify;
    struct sockaddr_in server, client;
    socklen_t addr_len_server, addr_len_client;
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
    
    FILE *log_file = fopen(PATH_LOG, "a");
    if(log_file == NULL){
        fprintf(stderr, "Error opening log file: %s\n", PATH_LOG);
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
    addr_len_server = sizeof(struct sockaddr);

    valid = bind(sockfd, (struct sockaddr *)&server, addr_len_server);

    if (valid < 0){
        perror("Bind failed");
        exit(-1);
    }

    valid = listen(sockfd, BACKLOG);
    if (valid < 0){
        perror("Listen Failed");
        exit(-1);
    }

    client_count_sem = sem_open("/client_count_sem", O_CREAT, 0644, MAX_CLIENTS);
    if(client_count_sem == SEM_FAILED){
        perror("Error creating semaphore");
        exit(-1);
    }

    addr_len_client = sizeof(struct sockaddr);

    while(1){
        int client_sock = accept(sockfd, (struct sockaddr *)&client, &addr_len_client);
        if (client_sock < 0){
            perror("Accept Failed");
            continue;
        }

     ThreadArgs *args = malloc(sizeof (ThreadArgs));
        if(args == NULL){
            fprintf(stderr, "Error allocating memory for thread arguments\n");
            close(client_sock);
            continue;
        }

        args->client_sock = client_sock;
        args->table_hash = table;
        args->entries_file = entries_file;
        args->csv = csv;
        args->log_file = log_file;  

        sem_wait(client_count_sem);
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, (void *)process_query, (void *)args);

    }

    sem_close(client_count_sem);
    sem_unlink("/client_count_sem");
    fclose(log_file);


    return 0;
}
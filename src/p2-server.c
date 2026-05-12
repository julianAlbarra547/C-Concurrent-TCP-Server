#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "hash.h"
#include <fcntl.h>           
#include <sys/stat.h>        
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include "csv_reader.h"

#define BACKLOG 32
#define MAX_CLIENTS 32
#define PATH_LOG "data/log/log.txt"

sem_t *client_count_sem;

typedef struct query_args{
    int client_sock;
    long *table_hash;
    FILE *entries_file;
    FILE *csv;
    FILE *log_file;
    pthread_mutex_t *log_mutex;
    pthread_mutex_t *hash_mutex;
    pthread_mutex_t *csv_mutex;
    char ip_client[INET_ADDRSTRLEN];
} ThreadArgs;

void format_log_search(char *buffer, char *title, char *artist, char *ip_client, size_t size_buffer){

    time_t now;
    struct tm info;
    char buff[100];

    time(&now);
    localtime_r(&now, &info);

    strftime(buff, sizeof(buff), "%Y%m%dT%H%M%S", &info);
    snprintf(buffer, size_buffer, "[Fecha %s] Cliente [%s] [búsqueda - Titulo: %s - Artista: %s]", buff, ip_client, title, artist);
}

void format_log_insert(char *buffer, char *title, char *artist, char *ip_client, size_t size_buffer){

    time_t now;
    struct tm info;
    char buff[100];

    time(&now);
    localtime_r(&now, &info);

    strftime(buff, sizeof(buff), "%Y%m%dT%H%M%S", &info);
    snprintf(buffer, size_buffer, "[Fecha %s] Cliente [%s] [inserción - Titulo: %s - Artista: %s]", buff, ip_client, title, artist);
}

void *process_query(void *arg){
    ThreadArgs *args =  (ThreadArgs *)arg;
    int client_sock = args->client_sock;
    long *table_hash = args->table_hash;
    FILE *entries_file = args->entries_file;
    FILE *csv = args->csv;
    FILE *log_file = args->log_file;
    pthread_mutex_t *log_mutex = args->log_mutex;
    pthread_mutex_t *hash_mutex = args->hash_mutex;
    pthread_mutex_t *csv_mutex = args->csv_mutex;
    char *ip_client = args->ip_client;

    Query query;
    int identify;

    if (rcv_all(client_sock, &identify, sizeof(int)) == -1){
        fprintf(stderr, "Error receiving identify from client\n");
        close(client_sock);
        sem_post(client_count_sem);
        free(arg);
        return NULL;
    }

    if (identify == 1){
        if(rcv_all(client_sock, &query, sizeof(Query)) == -1){
            fprintf(stderr, "Error receiving identify from client\n");
            close(client_sock);
            sem_post(client_count_sem);
            free(arg);
            return NULL;
        }

        char buffer[4096];
        format_log_search(buffer, query.title, query.artist, ip_client, sizeof(buffer));

        pthread_mutex_lock(log_mutex);
        fwrite(buffer, sizeof(char), strlen(buffer), log_file);
        fwrite("\n", sizeof(char), 1, log_file);
        fflush(log_file);
        pthread_mutex_unlock(log_mutex);

        if(strlen(query.title) == 0){
            fprintf(stderr, "Invalid query: Title and Artist cannot be empty.\n");
            Row empty;
            memset(&empty, 0, sizeof(Row));
            empty.id = -1;
            if(send_all(client_sock, &empty, sizeof(Row)) == -1){
                fprintf(stderr, "Error sending response to client\n");
                free(arg);
                sem_post(client_count_sem);
                close(client_sock);
                return NULL;
            }
            free(arg);
            sem_post(client_count_sem);
            close(client_sock);
            return NULL;
        }

        if(strlen(query.artist) == 0){
            Hash_node nodes[5];
            Row results[5];

            pthread_mutex_lock(hash_mutex);
            int count = search_range_node(table_hash, entries_file, query.title, nodes, 5);
            pthread_mutex_unlock(hash_mutex);

            if(count == -1){
                fprintf(stderr, "Error searching for title: %s\n", query.title);
                if(send_all(client_sock, &count, sizeof(int)) == -1){
                    fprintf(stderr, "Error sending response to client\n");
                    free(arg);
                    sem_post(client_count_sem);
                    close(client_sock);
                    return NULL;
                }
            } else if(count <= 5){
                if(send_all(client_sock, &count, sizeof(int)) == -1){
                    fprintf(stderr, "Error sending response to client\n");
                    free(arg);
                    sem_post(client_count_sem);
                    close(client_sock);
                    return NULL;
                }
            }

            pthread_mutex_lock(csv_mutex);
            for (int i = 0; i < count; i++) {
                    Row *row = read_csv(csv, nodes[i].offset);
                    if (row == NULL) {
                        fprintf(stderr, "Error reading row from CSV at offset: %ld\n", nodes[i].offset);
                        continue;
                    }
                    results[i] = *row;
                    free(row);
            }
            pthread_mutex_unlock(csv_mutex);
            
            if(send_all(client_sock, &results, sizeof(Row) * count) == -1){
                fprintf(stderr, "Error sending response to client\n");
                free(arg);
                sem_post(client_count_sem);
                close(client_sock);
                return NULL;
            }

        } else {

            pthread_mutex_lock(hash_mutex);
            long offset = search_node(table_hash, entries_file, query.title, query.artist);
            pthread_mutex_unlock(hash_mutex);
            
            if(offset == -1){
                fprintf(stderr, "No entry found for Title='%s' and Artist='%s'\n", query.title, query.artist);
                int confirm = -1;
                if(send_all(client_sock, &confirm, sizeof(int)) == -1){
                    fprintf(stderr, "Error sending response to client\n");
                    free(arg);
                    sem_post(client_count_sem);
                    close(client_sock);
                    return NULL;
                }
                free(arg);
                sem_post(client_count_sem);
                close(client_sock);
                return NULL;
            }

            int count = 1;

            if(send_all(client_sock, &count, sizeof(int)) == -1){
                fprintf(stderr, "Error sending response to client\n");
                free(arg);
                sem_post(client_count_sem);
                close(client_sock);
                return NULL;
            }
            pthread_mutex_lock(csv_mutex);
            Row *row = read_csv(csv, offset);
            pthread_mutex_unlock(csv_mutex);

            if(send_all(client_sock, row, sizeof(Row)) == -1){
                fprintf(stderr, "Error sending response row to client\n");
                if(send_all(client_sock, &count, sizeof(int)) == -1){
                    fprintf(stderr, "Error sending response to client\n");
                    free(arg);
                    free(row);
                    sem_post(client_count_sem);
                    close(client_sock);
                    return NULL;
                }
                free(arg);
                free(row);
                sem_post(client_count_sem);
                close(client_sock);
                return NULL;
            }

            free(row);
        }
    } else if (identify == 2){

        Row new_row;
        if (rcv_all(client_sock, &new_row, sizeof(Row)) == -1){
            fprintf(stderr, "Error receiving new row from client\n");
            close(client_sock);
            sem_post(client_count_sem);
            free(arg);
            return NULL;
        }

        char norm_title[512], norm_artist[2048];
        if (normalize_string(new_row.title, norm_title, sizeof(norm_title)) != 0){
            fprintf(stderr, "Error normalizing title: %s\n", new_row.title);
            int confirm = 0;
            if(send_all(client_sock, &confirm, sizeof(int)) == -1){
                fprintf(stderr, "Error sending response to client\n");
                free(arg);
                sem_post(client_count_sem);
                close(client_sock);
                return NULL;
            }
            free(arg);
            sem_post(client_count_sem);
            close(client_sock);
            return NULL;
        }

        if (normalize_string(new_row.artist, norm_artist, sizeof(norm_artist)) != 0){
            fprintf(stderr, "Error normalizing artist: %s\n", new_row.artist);
            int confirm = 0;
            if(send_all(client_sock, &confirm, sizeof(int)) == -1){
                fprintf(stderr, "Error sending response to client\n");
                free(arg);
                sem_post(client_count_sem);
                close(client_sock);
                return NULL;
            }
            free(arg);
            sem_post(client_count_sem);
            close(client_sock);
            return NULL;
        }

        pthread_mutex_lock(hash_mutex);
        if (node_exists(table_hash, entries_file, norm_title, norm_artist)) {
            fprintf(stderr, "Error: A song with Title='%s' and Artist='%s' already exists.\n", new_row.title, new_row.artist);
            int confirm = 0;
            if(send_all(client_sock, &confirm, sizeof(int)) == -1){
                fprintf(stderr, "Error sending response to client\n");
                free(arg);
                sem_post(client_count_sem);
                close(client_sock);
                return NULL;
            }
            free(arg);
            sem_post(client_count_sem);
            close(client_sock);
            return NULL;
        }
        pthread_mutex_unlock(hash_mutex);

        pthread_mutex_lock(csv_mutex);
        fseek(csv, 0, SEEK_END);
        long offset = ftell(csv);
        fprintf(csv,
                "%d,%s,%d,%s,%s,%s,%lld,%s,%f,%s\n",
                new_row.id,
                new_row.title,
                new_row.rank,
                new_row.date,
                new_row.artist,
                new_row.url,
                new_row.streams,
                new_row.album,
                new_row.duration,
                new_row.explicito);
        fflush(csv);
        pthread_mutex_unlock(csv_mutex);

        pthread_mutex_lock(hash_mutex);
        insert_node(table_hash, entries_file, norm_title, norm_artist, offset);

        FILE *idx = fopen(TABLE_IDX, "wb");
        if(idx == NULL){
            fprintf(stderr, "Error abriendo archivo de índice para actualización\n");
            pthread_mutex_unlock(hash_mutex);
            int confirm = 0;
            send_all(client_sock, &confirm, sizeof(int));
            free(arg);
            sem_post(client_count_sem);
            close(client_sock);
            return NULL;
        }
        fwrite(table_hash, sizeof(long), HASH_TABLE_SIZE, idx);
        fclose(idx);
        pthread_mutex_unlock(hash_mutex);

        int confirm = 1;
        send_all(client_sock, &confirm, sizeof(int));

        pthread_mutex_lock(log_mutex);
        char buffer[4096];
        format_log_insert(buffer, new_row.title, new_row.artist, ip_client, sizeof(buffer));
        fwrite(buffer, sizeof(char), strlen(buffer), log_file);
        fwrite("\n", sizeof(char), 1, log_file);
        fflush(log_file);
        pthread_mutex_unlock(log_mutex);
    }

    free(arg);
    sem_post(client_count_sem);
    close(client_sock);
    return NULL;
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

    pthread_mutex_t log_mutex;
    pthread_mutex_t hash_mutex;
    pthread_mutex_t csv_mutex;
    pthread_mutex_init(&log_mutex, NULL);
    pthread_mutex_init(&hash_mutex, NULL);
    pthread_mutex_init(&csv_mutex, NULL);

    while(1){
        int client_sock = accept(sockfd, (struct sockaddr *)&client, &addr_len_client);
        if (client_sock < 0){
            perror("Accept Failed");
            continue;
        }

        sem_wait(client_count_sem);

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
        args->log_mutex = &log_mutex;
        args->hash_mutex = &hash_mutex;
        args->csv_mutex = &csv_mutex;
        strncpy(args->ip_client, inet_ntoa(client.sin_addr), INET_ADDRSTRLEN);
        args->ip_client[INET_ADDRSTRLEN - 1] = '\0';

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, (void *)process_query, (void *)args);
        pthread_detach(thread_id);
    }

    sem_close(client_count_sem);
    sem_unlink("/client_count_sem");
    fclose(log_file);


    return 0;
}
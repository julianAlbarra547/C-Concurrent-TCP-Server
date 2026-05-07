#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "common.h"
#include "csv_reader.h"
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "hash.h"

int main(){
    Query query;
    int identify;
    int r, fdread, fdwrite;

    FILE *csv = fopen(CSV_FILE, "r+");
    if (csv == NULL) {
        fprintf(stderr, "Error opening CSV file: %s\n", CSV_FILE);
        return 1;
    }


    FILE *check = fopen(TABLE_IDX, "rb");
    if (check == NULL) {
        printf("Index file not found. Building index...\n");
        r = build_index(CSV_FILE, TABLE_IDX, ENTRIES_BIN);
        if (r == -1) {
            fprintf(stderr, "Error building index\n");
            return 1;
        }
    } else {
        printf("Index file found. Skipping index build.\n");
        fclose(check);
    }

    r = mkfifo(FIFO_CLIENT_PATH, 0666);
    if (r == -1 && errno != EEXIST) {
        perror("mkfifo read");
        return -1;
    }
    
    r = mkfifo(FIFO_SERVER_PATH, 0666);
    if (r == -1 && errno != EEXIST) {
        perror("mkfifo write");
        return -1;
    }

    fdread = open(FIFO_CLIENT_PATH, O_RDONLY);
    if (fdread == -1) {
        perror("Open read fifo");
        return -1;
    }

    fdwrite = open(FIFO_SERVER_PATH, O_WRONLY);
    if (fdwrite == -1) {
        perror("Open write fifo");
        return -1;
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

    while(1){

        if (read(fdread, &identify, sizeof(int)) <= 0) {
            perror("Error reading identity from fifo");
            Row empty;
            memset(&empty, 0, sizeof(Row));
            empty.id = -1; // Indicar error con un ID negativo
            write(fdwrite, &empty, sizeof(Row));
            continue;
        }

        if (identify == 1){
            if (read(fdread, &query, sizeof(Query)) <= 0) {
                perror("Error reading query from fifo");
                continue;
            }

            printf("Received query: Title='%s', Artist='%s'\n", query.title, query.artist);

            if (strlen(query.title) == 0) {
                fprintf(stderr, "Invalid query: Title and Artist cannot be empty.\n");
                Row empty;
                memset(&empty, 0, sizeof(Row));
                empty.id = -1; // Indicar error con un ID negativo
                write(fdwrite, &empty, sizeof(Row));
                continue;
            }

            if(query.artist[0] == '\0'){
                //Buscar solo por titulo
                Hash_node nodes[5];
                Row results[5];
                int count = search_range_node(table, entries_file, query.title, nodes, 5);

                if (count == -1) {
                    fprintf(stderr, "Error searching for title: %s\n", query.title);
                    write(fdwrite, &count, sizeof(int)); // Enviar -1 para indicar error
                    continue;
                } else if (count <= 5){
                    printf("It was found %d entries for title: %s\n", count, query.title);
                    write(fdwrite, &count, sizeof(int)); // Enviar el número de resultados encontrados
                }

                for (int i = 0; i < count; i++) {
                    Row *row = read_csv(csv, nodes[i].offset);
                    if (row == NULL) {
                        fprintf(stderr, "Error reading row from CSV at offset: %ld\n", nodes[i].offset);
                        continue;
                    }
                    results[i] = *row;
                    free(row);
                }

                if (write(fdwrite, &results, sizeof(Row) * count) == -1) {
                    perror("Error writing to fifo");
                    continue;
                }

            } else {
                //Buscar por titulo y artista
                long offset = search_node(table, entries_file, query.title, query.artist);
                if (offset == -1) {
                    fprintf(stderr, "No entry found for Title='%s' and Artist='%s'\n", query.title, query.artist);
                    int confirm = -1;
                    write(fdwrite, &confirm, sizeof(int));
                    continue;
                }

                int count = 1;
                write(fdwrite, &count, sizeof(int));

                Row *row;
                row = read_csv(csv, offset);

                if (write(fdwrite, row, sizeof(Row)) == -1) {
                    perror("Error writing row to fifo");
                    int confirm = -1;
                    write(fdwrite, &confirm, sizeof(int));
                    continue;
                }

                free(row);
            }

        } else if (identify == 2){
            // Agregar cancion
            Row new_row;
            if (read(fdread, &new_row, sizeof(Row)) <= 0) {
                perror("Error reading new row from fifo");
                int confirm = 0;
                write(fdwrite, &confirm, sizeof(int));
                continue;
            }

            char norm_title[512], norm_artist[2048];
            if (normalize_string(new_row.title, norm_title, sizeof(norm_title)) != 0) {
                fprintf(stderr, "Error normalizing title: %s\n", new_row.title);
                int confirm = 0;
                write(fdwrite, &confirm, sizeof(int));
                continue;
            }  
            
            if (normalize_string(new_row.artist, norm_artist, sizeof(norm_artist)) != 0) {
                fprintf(stderr, "Error normalizing artist: %s\n", new_row.artist);
                int confirm = 0;
                write(fdwrite, &confirm, sizeof(int));
                continue;
            }

            if (node_exists(table, entries_file, norm_title, norm_artist)) {
                fprintf(stderr, "Error: A song with Title='%s' and Artist='%s' already exists.\n", new_row.title, new_row.artist);
                int confirm = 0;
                write(fdwrite, &confirm, sizeof(int));
                continue;
            }

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

                // Actualizar índice
            insert_node(table, entries_file, norm_title, norm_artist, offset);


            // Guardar tabla actualizada
            FILE *idx = fopen(TABLE_IDX, "wb");
            if (!idx) {
                fprintf(stderr, "Error abriendo archivo de índice para actualización\n");
                continue;
            }
            fwrite(table, sizeof(long), HASH_TABLE_SIZE, idx);

            int confirm = 1;

            if (write(fdwrite, &confirm, sizeof(int)) == -1) {
                perror("Error writing confirmation to fifo");
                continue;
            }

            fclose(idx);

        } else {
            fprintf(stderr, "Invalid identify value: %d\n", identify);
        }

    }

    return 0;
}
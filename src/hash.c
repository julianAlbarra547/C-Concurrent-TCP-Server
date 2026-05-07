#include "hash.h"
#include "csv_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

int normalize_string(char *input, char *output, size_t size){

    if(input == NULL || output == NULL || size == 0){
        return -1;
    }

    int i;

    for(i = 0; input[i] != '\0' && i < size - 1; i++){
        output[i] = tolower(input[i]);
    }

    output[i] = '\0';
    return 0;
}

unsigned int hash(char *title){
    char normalized_title[512];

    if(normalize_string(title, normalized_title, sizeof(normalized_title)) != 0){
        fprintf(stderr, "Error normalizando el titulo: %s\n", title);
        return -1;
    }    

    unsigned int hash_value = 5381;             // Se emplea el algoritmo djb2 para generar el hash de la cadena de texto, con unsigned para no desborar el int.
    int i;                   

    for(i = 0; normalized_title[i] != '\0'; i++){
        hash_value = hash_value * 33 + normalized_title[i];
    }

    return hash_value % HASH_TABLE_SIZE;
}

int create_table(long *table){
    for(int i = 0; i < HASH_TABLE_SIZE; i++){
        table[i] = -1;
    }
    return 0;
}

int node_exists(long *table, FILE *entries, char *normalized_title, char *normalized_artist){
    unsigned int index = hash(normalized_title);
    long bucket_offset = table[index];

    while(bucket_offset != -1){
        Hash_node aux;
        fseek(entries, bucket_offset, SEEK_SET);
        fread(&aux, sizeof(Hash_node), 1, entries);

        if(strcmp(aux.title, normalized_title) == 0 && strcmp(aux.artist, normalized_artist) == 0){
            return 1;
        }

        bucket_offset = aux.next_entry;
    }

    return 0; 
}

int insert_node(long *table, FILE *entries, char *normalized_title, char *normalized_artist, long offset){
    unsigned int index = hash(normalized_title);
    
    Hash_node new_node;
    strncpy(new_node.title, normalized_title, sizeof(new_node.title));
    strncpy(new_node.artist, normalized_artist, sizeof(new_node.artist));
    new_node.offset = offset;
    new_node.next_entry = table[index];

    fseek(entries, 0, SEEK_END);
    long new_node_offset = ftell(entries);
    fwrite(&new_node, sizeof(Hash_node), 1, entries);

    table[index] = new_node_offset;
    return 0;
}

int build_index(const char *csv_path, const char *idx_path, const char *entries_path){
    FILE *csv = open_csv(csv_path);

    if(csv == NULL){
        return -1; 
    }

    FILE *entries_file = fopen(entries_path, "w+b");

    if(entries_file == NULL){
        perror("Error al crear el archivo de entradas.\n");
        fclose(csv);
        return -1;
    }

    long *table = malloc(HASH_TABLE_SIZE * sizeof(long));
    if(table == NULL){
        perror("Error al asignar memoria para la tabla hash.\n");
        fclose(csv);
        fclose(entries_file);
        return -1;
    }
    create_table(table);

    skip_header(csv);

    long offset = ftell(csv);
    Row *row = read_csv(csv, offset);

    while(row != NULL){

        static int counter = 0;
        counter++;
        if(counter % 10000 == 0){
            printf("Filas procesadas: %d\n", counter);
            fflush(stdout);
        }

        char normalized_title[512], normalized_artist[2048];
        if(normalize_string(row->title, normalized_title, sizeof(normalized_title)) != 0){
            perror("Error normalizando el titulo.\n");
            free(row);
            offset = ftell(csv);
            row = read_csv(csv, offset);
            continue;
        }
        if(normalize_string(row->artist, normalized_artist, sizeof(normalized_artist)) != 0){
            perror("Error normalizando el artista.\n");
            free(row);
            offset = ftell(csv);
            row = read_csv(csv, offset);
            continue;
        }
        
        if(!node_exists(table, entries_file, normalized_title, normalized_artist)){
            insert_node(table, entries_file, normalized_title, normalized_artist, offset);
        }

        free(row);
        offset = ftell(csv);
        row = read_csv(csv, offset);
    }

    FILE *idx_file = fopen(idx_path, "wb");
    if(idx_file == NULL){
        perror("Error al crear el archivo de indice.\n");
        free(table);
        fclose(csv);
        fclose(entries_file);
        return -1;
    }

    fwrite(table, sizeof(long), HASH_TABLE_SIZE, idx_file);
    fclose(idx_file);
    free(table);
    close_csv(csv);
    fclose(entries_file);
    return 0;
}

long search_node(long *table, FILE *entries, char *title, char *artist){
    char normalized_title[512];
    if(normalize_string(title, normalized_title, sizeof(normalized_title)) != 0){
        perror("Error normalizando el titulo\n");
        return -1;
    }

    char normalized_artist[2048];
    if(normalize_string(artist, normalized_artist, sizeof(normalized_artist)) != 0){
        perror("Error normalizando el artista\n");
        return -1;
    }

    unsigned int index = hash(normalized_title);
    long bucket_offset = table[index];

    while(bucket_offset != -1){
        Hash_node aux;
        fseek(entries, bucket_offset, SEEK_SET);
        fread(&aux, sizeof(Hash_node), 1, entries);

        if(strcmp(aux.title,  normalized_title)  == 0 &&
           strcmp(aux.artist, normalized_artist) == 0){
            return aux.offset;
        }

        bucket_offset = aux.next_entry;
    }

    return -1;
}

int search_range_node(long *table, FILE *entries, char *title, Hash_node *list, int size){
    char normalized_title[512];
    if(normalize_string(title, normalized_title, sizeof(normalized_title)) != 0){
        perror("Error normalizando el titulo\n");
        return -1;
    }

    unsigned int index = hash(normalized_title);
    long bucket_offset = table[index];
    int count = 0;

    while(bucket_offset != -1 && count < size){
        Hash_node aux;
        fseek(entries, bucket_offset, SEEK_SET);
        fread(&aux, sizeof(Hash_node), 1, entries);

        if(strcmp(aux.title, normalized_title) == 0){
            list[count] = aux;
            count++;
        }

        bucket_offset = aux.next_entry;
    }
    return count;
}

int load_table(const char *idx_path, long *table){
    FILE *idx_file = fopen(idx_path, "rb");
    if(idx_file == NULL){
        perror("Error al abrir el archivo de indice.\n");
        return -1;
    }

    fread(table, sizeof(long), HASH_TABLE_SIZE, idx_file);
    fclose(idx_file);
    return 0;
}
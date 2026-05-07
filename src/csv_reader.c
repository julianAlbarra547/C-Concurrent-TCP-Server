#include <stdio.h>
#include <stdlib.h>
#include "csv_reader.h"

#define COLUMNS_IN_ROW 10
#define MAX_VALUE_IN_ID 8
#define MAX_VALUE_IN_RANK 3
#define MAX_VALUE_IN_STREAMS 10
#define MAX_VALUE_IN_DURATION 9

FILE *open_csv(const char *filename) {
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        perror("Error opening file");
        return NULL;
    }

    return file;
}

int extract_row(char *buffer, int start, int end, char *obj){
    
    int j = start;
    int pointer = 0;

    if(buffer[start] == '"'){
        j++;
        start++;
        end--;
    }
            
    for( j, pointer ; j < end; j++, pointer++){
        obj[pointer] = buffer[j];
    }
    
    obj[pointer] = '\0';
    
    return 0;
}

Row *read_csv(FILE *file, long offset) {
    int seek_val;
    char *gets_val;
    char buff[4096]; // Cantidad obtenida de la suma de caracteres maximos que hay en cada columna.

    if(file == NULL){
        perror("Archivo no encontrado.\n");
        return NULL;
    }
    
    if(offset < 0){
        perror("Posicion del offset invalida.\n");
        return NULL;
    }

    seek_val = fseek(file, offset, SEEK_SET);
    
    if(seek_val < 0){
        perror("Registro no localizado.\n");
        return NULL;
    }
    
    Row *row = malloc(sizeof(Row));
    gets_val = fgets(buff, 4096, file);
    
    if(gets_val == NULL){
        perror("Error al obtener valores.\n");
        free(row);
        return NULL;
    }
    
    
    int start_column = 0;
    int end_column = 0;
    short in_quotes; 

    for(int i = 0; i < COLUMNS_IN_ROW; i ++){                   
        in_quotes = 0;

        for(int j = start_column; j < 4096; j ++){
            if(buff[j] == '"'){
                in_quotes = !in_quotes;
            }
            
            if(buff[j] == ',' && !in_quotes){
                end_column = j;
                break;
            }
            
            if(i == 9 && (buff[j] == '\n' || buff[j] == '\0')){
                end_column = j;
                break;
            }
        }
        
        if(i == 0){
            char box[MAX_VALUE_IN_ID];
            extract_row(buff, start_column, end_column, box);
            row->id = atoi(box);
        }
        
        if(i == 1){
            extract_row(buff, start_column, end_column, row->title);
        }
        
        if(i == 2){
            char box[MAX_VALUE_IN_RANK];
            extract_row(buff, start_column, end_column, box);
            row->rank = (short) atoi(box);
        }
        
        if(i == 3){
            extract_row(buff, start_column, end_column, row->date);
        }
        
        if(i == 4){
            extract_row(buff, start_column, end_column, row->artist);
        }
        
        if(i == 5){
            extract_row(buff, start_column, end_column, row->url);
        }
        
        if(i == 6){
            char box[MAX_VALUE_IN_STREAMS];
            extract_row(buff, start_column, end_column, box);
            row->streams = (long long) atof(box);
        }
        
        if(i == 7){
            extract_row(buff, start_column, end_column, row->album);
        }
        
        if(i == 8){
            char box[MAX_VALUE_IN_DURATION];
            extract_row(buff, start_column, end_column, box);
            row->duration = atof(box);
        }
        
        if(i == 9){
            extract_row(buff, start_column, end_column, row->explicito);
        }
        
        start_column = end_column + 1;
    }

    return row;
}

void print_row(Row *row){
    printf(
    "El registro tiene los siguientes datos: \nID: %i\nTitle: %s\nRank: %i\nDate: %s\nArtist: %s\nURL: %s\nStreams: %lli\nAlbum: %s\nDuration %lf\nExplicit: %s\n",
    row->id,
    row->title,
    row->rank,
    row->date,
    row->artist,
    row->url,
    row->streams,
    row->album,
    row->duration,
    row->explicito
    );

    printf("\n");
    
}

void close_csv(FILE *file){
    fclose(file);
}

void skip_header(FILE *file){
    char buffer[4096];
    fgets(buffer, 4096, file);
}

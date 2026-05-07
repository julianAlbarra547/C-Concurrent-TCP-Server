#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "common.h"
#include "csv_reader.h"
#include "utils.h"

/* ─── Menu ──────────────────────────────────────────────────────────────── */

void print_menu(){
    printf("\nBienvenido! Seleccione una opcion:\n");
    printf("1) Buscar cancion.\n");
    printf("2) Agregar registro.\n");
    printf("3) Salir\n");
    printf("Elegir opcion: ");
}

/* ─── Opcion 1: Buscar cancion ──────────────────────────────────────────── */

void option1(int fdwrite, int fdread){
    char title[512];
    char artist[2048];
    Query query;
    int identify = 1;
    short invalid = 1;

    while(invalid){
        printf("\nOpcion 1: Buscar cancion\n");
        printf("Nota: Ingrese 0 para regresar al menu.\n");
        printf("Titulo de la cancion: ");
        fgets(title, sizeof(title), stdin);
        trim(title);

        if(strncmp(title, "0", 1) == 0){
            printf("Regresando al menu principal...\n");
            return;
        }

        if(strlen(title) == 0){
            printf("Titulo no puede estar vacio. Intente nuevamente.\n");
            continue;
        }

        if(strlen(title) >= sizeof(query.title)){
            printf("Titulo demasiado largo. Intente nuevamente.\n");
            continue;
        }

        invalid = 0;
    }

    invalid = 1;

    while(invalid){
        printf("Artista de la cancion: ");
        fgets(artist, sizeof(artist), stdin);
        trim(artist);

        if(strlen(artist) >= sizeof(query.artist)){
            printf("Artista demasiado largo. Intente nuevamente.\n");
            continue;
        }

        invalid = 0;
    }

    strncpy(query.title,  title,  sizeof(query.title));
    strncpy(query.artist, artist, sizeof(query.artist));

    write(fdwrite, &identify, sizeof(int));
    write(fdwrite, &query,    sizeof(Query));

    int count;
    read(fdread, &count, sizeof(int));

    if(count == -1){
        printf("NA - Cancion no encontrada.\n");
        return;
    }

    Row result;

    for(int i = 0; i < count; i++){
        read(fdread, &result, sizeof(Row));
        print_row(&result);
    }
}

/* ─── Opcion 2: Agregar registro ────────────────────────────────────────── */

void option2(int fdwrite, int fdread){
    Row new_row;
    int identify = 2;
    char buff[64];
    short invalid;

    /* ID */
    invalid = 1;
    while(invalid){
        if(!prompt_text("\nID de la cancion (0 para volver): ", buff, sizeof(buff))) return;
        if(!valid_positive_int(buff)){
            printf("ID invalido. Debe ser un numero entero positivo.\n");
            continue;
        }
        new_row.id = atoi(buff);
        invalid = 0;
    }

    /* Titulo */
    invalid = 1;
    while(invalid){
        if(!prompt_text("Titulo de la cancion (0 para volver): ", new_row.title, sizeof(new_row.title))) return;
        if(strlen(new_row.title) == 0){
            printf("Titulo no puede estar vacio.\n");
            continue;
        }
        invalid = 0;
    }

    /* Rank */
    invalid = 1;
    while(invalid){
        if(!prompt_text("Rank de la cancion (0 para volver): ", buff, sizeof(buff))) return;
        if(!valid_positive_int(buff)){
            printf("Rank invalido. Debe ser un numero entero positivo.\n");
            continue;
        }
        new_row.rank = (short) atoi(buff);
        invalid = 0;
    }

    /* Fecha */
    invalid = 1;
    while(invalid){
        if(!prompt_text("Fecha de lanzamiento YYYY-MM-DD (0 para volver): ", new_row.date, sizeof(new_row.date))) return;
        if(!valid_date(new_row.date)){
            printf("Fecha invalida. Formato requerido: YYYY-MM-DD.\n");
            continue;
        }
        invalid = 0;
    }

    /* Artista */
    invalid = 1;
    while(invalid){
        if(!prompt_text("Artista de la cancion (0 para volver): ", new_row.artist, sizeof(new_row.artist))) return;
        if(strlen(new_row.artist) == 0){
            printf("Artista no puede estar vacio.\n");
            continue;
        }
        invalid = 0;
    }

    /* URL */
    invalid = 1;
    while(invalid){
        if(!prompt_text("URL de la cancion (0 para volver): ", new_row.url, sizeof(new_row.url))) return;
        if(strlen(new_row.url) == 0){
            printf("URL no puede estar vacia.\n");
            continue;
        }
        invalid = 0;
    }

    /* Streams */
    invalid = 1;
    while(invalid){
        if(!prompt_text("Numero de streams (0 para volver): ", buff, sizeof(buff))) return;
        if(!valid_positive_int(buff)){
            printf("Streams invalido. Debe ser un numero entero positivo.\n");
            continue;
        }
        new_row.streams = atoll(buff);
        invalid = 0;
    }

    /* Album */
    invalid = 1;
    while(invalid){
        if(!prompt_text("Album de la cancion (0 para volver): ", new_row.album, sizeof(new_row.album))) return;
        if(strlen(new_row.album) == 0){
            printf("Album no puede estar vacio.\n");
            continue;
        }
        invalid = 0;
    }

    /* Duracion */
    invalid = 1;
    while(invalid){
        if(!prompt_text("Duracion en segundos (0 para volver): ", buff, sizeof(buff))) return;
        if(!valid_positive_double(buff)){
            printf("Duracion invalida. Debe ser un numero positivo.\n");
            continue;
        }
        new_row.duration = atof(buff) * 1000.0;
        invalid = 0;
    }

    /* Explicito */
    invalid = 1;
    while(invalid){
        if(!prompt_text("Es explicita? True/False (0 para volver): ", new_row.explicito, sizeof(new_row.explicito))) return;
        if(!valid_explicit(new_row.explicito)){
            printf("Valor invalido. Ingrese True o False.\n");
            continue;
        }
        invalid = 0;
    }

    write(fdwrite, &identify, sizeof(int));
    write(fdwrite, &new_row,  sizeof(Row));

    int confirm;
    read(fdread, &confirm, sizeof(int));

    if(confirm == 1){
        printf("Registro agregado exitosamente.\n");
    } else {
        printf("Error al agregar el registro.\n");
    }
}

/* ─── Main ──────────────────────────────────────────────────────────────── */

int main(){
    int fdwrite, fdread;
    short option;
    short start = 1;
    char buff[8];

    fdwrite = open(FIFO_CLIENT_PATH, O_WRONLY);
    if(fdwrite == -1){
        perror("Open write fifo");
        close(fdwrite);
        return -1;
    }

    fdread = open(FIFO_SERVER_PATH, O_RDONLY);
    if(fdread == -1){
        perror("Open read fifo");
        close(fdread);
        return -1;
    }

    while(start){
        print_menu();
        fgets(buff, sizeof(buff), stdin);
        trim(buff);
        option = (short) atoi(buff);

        switch(option){
            case 1:
                option1(fdwrite, fdread);
                break;
            case 2:
                option2(fdwrite, fdread);
                break;
            case 3:
                printf("Saliendo del programa...\n");
                start = 0;
                break;
            default:
                printf("Opcion no valida. Intente nuevamente.\n");
        }
    }

    close(fdwrite);
    close(fdread);
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"
#include "csv_reader.h"
#include "utils_ui.h"

/* ─── Menu ──────────────────────────────────────────────────────────────── */

void print_menu(){
    printf("\nBienvenido! Seleccione una opcion:\n");
    printf("1) Buscar cancion.\n");
    printf("2) Agregar registro.\n");
    printf("3) Salir\n");
    printf("Elegir opcion: ");
}

/* ─── Opcion 1: Buscar cancion ──────────────────────────────────────────── */

void option1(int sockfd){
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

    if(send_all(sockfd, &identify, sizeof(int)) == -1 || send_all(sockfd, &query, sizeof(Query)) == -1){
        perror("Error sending search request");
        return;
    }

    int count;
    if(rcv_all(sockfd, &count, sizeof(int)) == -1){
        perror("Error receiving search response count");
        return;
    }

    if(count == -1){
        printf("NA - Cancion no encontrada.\n");
        return;
    }

    Row result;

    for(int i = 0; i < count; i++){
        if(rcv_all(sockfd, &result, sizeof(Row)) == -1){
            perror("Error receiving search result row");
            return;
        }
        print_row(&result);
    }
}

/* ─── Opcion 2: Agregar registro ────────────────────────────────────────── */

void option2(int sockfd){
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

    if(send_all(sockfd, &identify, sizeof(int)) == -1 || send_all(sockfd, &new_row, sizeof(Row)) == -1){
        perror("Error sending insert request");
        return;
    }

    int confirm;
    if(rcv_all(sockfd, &confirm, sizeof(int)) == -1){
        perror("Error receiving insert confirmation");
        return;
    }

    if(confirm == 1){
        printf("Registro agregado exitosamente.\n");
    } else {
        printf("Error al agregar el registro.\n");
    }
}

/* ─── Main ──────────────────────────────────────────────────────────────── */

int main(int argc, char **argv){
    int sockfd;
    short option;
    short start = 1;
    char buff[8];

    const char *server_ip = "127.0.0.1";
    const char *env_server_ip = getenv("P2_SERVER_IP");
    if(env_server_ip != NULL && env_server_ip[0] != '\0'){
        server_ip = env_server_ip;
    }
    if(argc >= 2 && argv[1] != NULL && argv[1][0] != '\0'){
        server_ip = argv[1];
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if(inet_pton(AF_INET, server_ip, &server_addr.sin_addr) != 1){
        fprintf(stderr, "Invalid server IP: %s\n", server_ip);
        close(sockfd);
        return -1;
    }

    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("Connect failed");
        close(sockfd);
        return -1;
    }

    char local_ip[INET_ADDRSTRLEN] = {0};
    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    if(getsockname(sockfd, (struct sockaddr *)&local_addr, &local_len) == 0){
        inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, sizeof(local_ip));
    } else {
        strncpy(local_ip, "unknown", sizeof(local_ip) - 1);
    }
    printf("Conectado al servidor %s:%d (IP local: %s)\n", server_ip, PORT, local_ip);

    while(start){
        print_menu();
        fgets(buff, sizeof(buff), stdin);
        trim(buff);
        option = (short) atoi(buff);

        switch(option){
            case 1:
                option1(sockfd);
                break;
            case 2:
                option2(sockfd);
                break;
            case 3:
                printf("Saliendo del programa...\n");
                start = 0;
                break;
            default:
                printf("Opcion no valida. Intente nuevamente.\n");
        }
    }

    close(sockfd);
    return 0;
}

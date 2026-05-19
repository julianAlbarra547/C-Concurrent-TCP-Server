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
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <linux/input.h>
#include <dirent.h>

#define KEYBOARD_DEVICE "event2" //Cambiar en base a la entrada de teclado. Revisar en /dev/input/by-id

/* ─── Menu ──────────────────────────────────────────────────────────────── */
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define PURPLE  "\033[35m"
#define CYAN    "\033[36m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define RED     "\033[31m"
#define WHITE   "\033[97m"
#define BG_DARK "\033[48;5;17m"

void print_menu() {
    printf("\n");
    printf(PURPLE "  ╔══════════════════════════════════════╗\n" RESET);
    printf(PURPLE "  ║" BOLD WHITE "       ♪  CORDILLERAS SOUND  ♪       " RESET PURPLE " ║\n" RESET);
    printf(PURPLE "  ║" CYAN "     Sistema de gestión musical      " RESET PURPLE " ║\n" RESET);
    printf(PURPLE "  ╠══════════════════════════════════════╣\n" RESET);
    printf(PURPLE "  ║" RESET "                                      " PURPLE "║\n" RESET);
    printf(PURPLE "  ║  " YELLOW "Seleccione una opción:" RESET "              " PURPLE "║\n" RESET);
    printf(PURPLE "  ║" RESET "                                      " PURPLE "║\n" RESET);
    printf(PURPLE "  ║  " CYAN "[ 1 ]" WHITE "  Buscar canción              " RESET PURPLE " ║\n" RESET);
    printf(PURPLE "  ║  " GREEN "[ 2 ]" WHITE "  Agregar canción            " RESET PURPLE "  ║\n" RESET);
    printf(PURPLE "  ║  " RED   "[ 3 ]" WHITE "  Salir                      " RESET PURPLE "  ║\n" RESET);
    printf(PURPLE "  ║" RESET "                                      " PURPLE "║\n" RESET);
    printf(PURPLE "  ╚══════════════════════════════════════╝\n" RESET);
    printf(YELLOW "  Elige una opción: " RESET);
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
        printf("La cancion no fue encontrada.\n");
        return;
    }

    Row result;
    if (count == 1){
        rcv_all(sockfd, &result, sizeof(Row));
        print_row(&result);
    } else{
        printf("\nSe encontraron %d resultados:\n\n", count);
        for(int i = 0; i < count; i++){
            if(rcv_all(sockfd, &result, sizeof(Row)) == -1){
                perror("Error receiving search result row");
                return;
            }
            print_row(&result);
        }
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

//------------ Se accede a la capa 2: se accede a eventos generados por el kernel y para ello se lee un archivo
void waitKey(){
    printf(RESET CYAN "Presiona CUALQUIER tecla para continuar\n" RESET);
    fflush(stdout);
    char path[80] = "/dev/input/";
    strcat(path, KEYBOARD_DEVICE);
    int fd = open(path, O_RDONLY);
    if(fd < 0){
        // fallback a termios si no se puede abrir el dispositivo
        struct termios old, raw;
        tcgetattr(STDIN_FILENO, &old);  // guarda config actual
        raw = old;
        raw.c_lflag &= ~(ICANON | ECHO); // desactiva buffer de línea y eco
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        getchar();                      // lee un solo caracter sin Enter
        tcsetattr(STDIN_FILENO, TCSANOW, &old); // restaura config original
        return;
    }

    struct input_event ev;

    // espera hasta que se PRESIONE cualquier tecla (value == 1)
    while(read(fd, &ev, sizeof(ev)) > 0){
        if(ev.type == EV_KEY && ev.value == 1){
            break;
        }
    }

    close(fd);
    tcflush(STDIN_FILENO, TCIFLUSH);
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
        fflush(stdout); 
        print_menu();

        fgets(buff, sizeof(buff), stdin);
        trim(buff);
        option = (short) atoi(buff);

        fflush(stdout);
        printf(RESET RED "Está seguro de continuar con la opción %hd? Presione Enter para continuar, otra tecla para regresar:" RESET, option);
        fgets(buff, sizeof(buff), stdin);
        if (buff[0] != '\n'){ continue;}

        switch(option){
            case 1:
                option1(sockfd);
                waitKey();
                break;
            case 2:
                option2(sockfd);
                waitKey();
                break;
            case 3:
                printf("Saliendo del programa...\n");
                int bye = 0;
                send_all(sockfd, &bye, sizeof(int));
                start = 0;
                break;
            default:
                printf("Opcion no valida. Intente nuevamente.\n");
        }
    }

    close(sockfd);
    return 0;
}

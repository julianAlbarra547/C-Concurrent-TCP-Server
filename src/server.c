#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>

#define PORT 8080
#define BACKLOG 32

int main(){
    int sockfd, valid, addr_len;
    struct sockaddr_in server;

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

    return 0;
}
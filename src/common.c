# include "common.h"

int rcv_all(int sockfd, void *buffer, size_t length){

    size_t total_received = 0;
    while(total_received < length){
        int received = recv(sockfd, buffer + total_received, length - total_received, 0);
        if(received <= 0){
            return -1;
        }
        total_received += received;
    }
    return 0;
}

int send_all(int sockfd, void *buffer, size_t length){

    size_t total_sent = 0;
    while(total_sent < length){
        int sent = send(sockfd, buffer + total_sent, length - total_sent, 0);
        if(sent <= 0){
            return -1;
        }
        total_sent += sent;
    }
    return 0;
}

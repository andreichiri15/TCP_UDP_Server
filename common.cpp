#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

int recv_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_received = 0;
    char *buff = (char *)buffer;

    while (bytes_received < len) {
        ssize_t result = recv(sockfd, buff + bytes_received, len - bytes_received, 0);
        if (result < 0) {
            return -1;
        } else if (result == 0) {
            break;
        }
        bytes_received += result;
    }

    return bytes_received;
}

int send_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_sent = 0;
    char *buff = (char *)buffer;

    while (bytes_sent < len) {
        ssize_t result = send(sockfd, buff + bytes_sent, len - bytes_sent, 0);
        if (result < 0) {
            return -1;
        } else if (result == 0) {
            break;
        }
        bytes_sent += result;
    }

    return bytes_sent;
}

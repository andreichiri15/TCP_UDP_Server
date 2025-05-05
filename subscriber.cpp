#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <iostream>
#include <math.h>

#include "common.h"
#include "helpers.h"

void interpret_message_from_server(tcp_message_from_server *msg, char *content) {
    std::cout << msg->msg.topic << " - ";

    if (msg->msg.type == 0) {
        uint8_t sign = content[0];
        char number[4];

        memcpy(number, content + 1, 4);

        int value = ntohl(*((int *)(number)));

        if (sign == 1) {
            value = -value;
        }

        std::cout << "INT - " << value << "\n";
    } else if (msg->msg.type == 1) {
        float value = ntohs(*((u_int16_t *)(content))) / 100.0;

        printf("SHORT_REAL - %.2f\n", value);
    } else if (msg->msg.type == 2) {
        uint8_t sign = content[0];
        char number[4];

        mempcpy(number, content + 1, 4);

        char exponent = content[5];
    
        u_int32_t new_nr = ntohl(*((u_int32_t *)(number)));
        float value = new_nr * std::pow(10, -exponent);

        if (sign == 1) {
            value = -value;
        }

        printf("FLOAT - %.4f\n", value);
    } else {
        std::cout << "STRING - " << content << "\n";
    }

    
}

void run_client(int sockfd) {
    int rc;
    char buffer[2048];

    while (1) {
        memset(buffer, 0, 2048);
        
        std::vector<struct pollfd> fds(2);
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[1].fd = sockfd;
        fds[1].events = POLLIN;

        rc = poll(fds.data(), fds.size(), -1);
        DIE(rc < 0, "poll");

        for (int i = 0; i < fds.size(); i++) {
            memset(buffer, 0, 2048);

            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == STDIN_FILENO) {
                    std::string command, topic;
                    std::cin >> command;

                    tcp_message msg = {};

                    if (strcmp(command.c_str(), "subscribe") == 0) {
                        msg.type = 0;
                        std::cout << "Subscribed to topic ";

                    } else if (strcmp(command.c_str(), "unsubscribe") == 0) {
                        msg.type = 1;
                        std::cout << "Unsubscribed from topic ";
                    } else if (strcmp(command.c_str(), "exit") == 0) {
                        msg.type = 2;
                    }

                    if (msg.type != 2) {
                        std::cin >> topic;
                        std::cout << topic << "\n";
                        strcpy(msg.topic, topic.c_str());
                    }

                    if (msg.type == 2) {
                        return;
                    }

                    DIE(send_all(sockfd, &msg, sizeof(tcp_message)) < 0, "send");

                    
                } else if (fds[i].fd == sockfd) {
                    tcp_message_from_server *msg;

                    rc = recv_all(sockfd, buffer, sizeof(tcp_message_from_server));
                    DIE(rc < 0, "recv");

                    if (rc == 0) {
                        return;
                    }

                    msg = (tcp_message_from_server *)buffer;

                    if (msg->msg.type == 5) {
                        return;
                    }

                    char *content = (char *)malloc(msg->size);
                    DIE(content == NULL, "malloc");

                    rc = recv_all(sockfd, content, msg->size);
                    DIE(rc < 0, "recv");

                    interpret_message_from_server(msg, content);

                    free(content);
                }
            }
        
        }
    }
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    
    if (argc != 4) {
        return -1;
    }

    DIE(strlen(argv[1]) > 10, "ID too long");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    int rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");

    DIE(send_all(sockfd, argv[1], 11) < 0, "send");

    int enable = 1;
    rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));

    run_client(sockfd);

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    return 0;   
}
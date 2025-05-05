#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <iostream>
#include <sstream>

#include "common.h"
#include "helpers.h"

std::unordered_map<int, std::string> online_clients;
std::unordered_map<std::string, std::unordered_set<std::string>> subscribed_topics;

std::vector<std::string> split_topic(const std::string& topic) {
    std::vector<std::string> parts;
    std::istringstream iss(topic);
    std::string part;

    while (std::getline(iss, part, '/')) {
        parts.push_back(part);
    }
    return parts;
}

bool matches(const std::string& sub_topic, const std::string& msg_topic) {
    std::vector<std::string> sub_parts = split_topic(sub_topic);
    std::vector<std::string> msg_parts = split_topic(msg_topic);

    size_t i = 0, j = 0;
    while (i < sub_parts.size() && j < msg_parts.size()) {
        if (sub_parts[i] == "+") {
            i++; j++;
        } else if (sub_parts[i] == "*") {
            if (i == sub_parts.size() - 1) return true;
            i++;
            while (j < msg_parts.size() && sub_parts[i] != msg_parts[j]) j++;
        } else if (sub_parts[i] != msg_parts[j]) {
            return false;
        } else {
            i++; j++;
        }
    }

    return i == sub_parts.size() && j == msg_parts.size();
}

void run_server(int udp_sockfd, int tcp_sockfd, uint16_t port) {
    int rc;
    char buffer[2048];

    std::vector<struct pollfd> clients(3);

    clients[0].fd = STDIN_FILENO;
    clients[0].events = POLLIN;

    clients[1].fd = tcp_sockfd;
    clients[1].events = POLLIN;

    clients[2].fd = udp_sockfd;
    clients[2].events = POLLIN;

    while(1) {
        rc = poll(clients.data(), clients.size(), -1);
        DIE(rc < 0, "poll");
        memset(buffer, 0, 2048);

        for (int i = 0; i < clients.size(); i++) {
            if (clients[i].revents & POLLIN) {
                if (clients[i].fd == STDIN_FILENO) {
                    fgets(buffer, 2048, stdin);

                    if (strncmp(buffer, "exit", 4) == 0) {
                        for (int i = 1; i < clients.size(); i++) {
                            close(clients[i].fd);
                        }
                        return;
                    }

                } else if (clients[i].fd == tcp_sockfd) {
                    struct sockaddr_in cli_addr;
                    socklen_t clilen = sizeof(cli_addr);

                    const int newsockfd = accept(tcp_sockfd, (struct sockaddr *)&cli_addr, &clilen);
                    DIE(newsockfd < 0, "accept");

                    int enable = 1;
                    int result = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&enable, sizeof(int));

                    rc = recv(newsockfd, buffer, 11, 0);
                    DIE(rc < 0, "recv");

                    std::string id(buffer);

                    bool found = false;
                    for (int i = 3; i < clients.size(); i++) {
                        if (online_clients[clients[i].fd] == id) {
                            std::cout << "Client " << id << " already connected.\n";
                            found = true;
                            break;
                        }
                    }

                    if (found) {
                        tcp_message_from_server msg;
                        msg.msg.type = 5;

                        rc = send_all(newsockfd, &msg, sizeof(tcp_message_from_server));

                        close(newsockfd);
                        continue;
                    }

                    struct pollfd new_client;
                    new_client.fd = newsockfd;
                    new_client.events = POLLIN;
                    clients.push_back(new_client);

                    std::cout << "New client " << buffer << " connected from " << inet_ntoa(cli_addr.sin_addr) << ":" << ntohs(cli_addr.sin_port) << "\n";

                    online_clients[newsockfd] = id;

                } else if(clients[i].fd == udp_sockfd) {
                    struct sockaddr_in udp_addr;
                    socklen_t udp_len = sizeof(udp_addr);
                    rc = recvfrom(udp_sockfd, buffer, 2048, 0, (sockaddr *)&udp_addr, &udp_len);
                    DIE(rc < 0, "recvfrom");

                    struct udp_message *udp_msg = (struct udp_message *)buffer;

                    struct tcp_message_from_server tcp_msg;
                    char content[MSG_MAXSIZE];

                    tcp_msg.ip = udp_addr.sin_addr.s_addr;
                    tcp_msg.port = udp_addr.sin_port;
                    tcp_msg.msg.type = udp_msg->type;
                    strcpy(tcp_msg.msg.topic, udp_msg->topic);
                    memcpy(content, udp_msg->content, MSG_MAXSIZE);

                    if (udp_msg->type == 0) {
                        tcp_msg.size = sizeof(uint32_t) + 1;
                    } else if (udp_msg->type == 1) {
                        tcp_msg.size = sizeof(uint16_t);
                    } else if (udp_msg->type == 2) {
                        tcp_msg.size = sizeof(uint32_t) + sizeof(uint8_t) + 1;
                    } else if (udp_msg->type == 3) {
                        tcp_msg.size = strlen(udp_msg->content) + 1;
                    }

                    for (int i = 3; i < clients.size(); i++) {
                        if (online_clients.find(clients[i].fd) != online_clients.end()) {
                            std::string id = online_clients[clients[i].fd];

                            std::unordered_set<std::string>::const_iterator it;
                            for (it = subscribed_topics[id].begin(); it != subscribed_topics[id].end(); ++it) {
                                const std::string& topic = *it;
                                if (matches(topic, udp_msg->topic)) {
                                    rc = send_all(clients[i].fd, &tcp_msg, sizeof(tcp_message_from_server));
                                    DIE(rc < 0, "send");

                                    rc = send_all(clients[i].fd, content, tcp_msg.size);
                                    DIE(rc < 0, "send");
                                    break;
                                }
                            }
                        }
                    }

                } else {
                    rc = recv_all(clients[i].fd, buffer, sizeof(tcp_message));
                    DIE(rc < 0, "recv");

                    if (rc == 0) {
                        std::cout << "Client " << online_clients[clients[i].fd] << " disconnected.\n";

                        online_clients.erase(clients[i].fd);
                        close(clients[i].fd);
                        clients.erase(clients.begin() + i);
                        continue;
                    }

                    tcp_message *msg = (tcp_message *)buffer;

                    if (msg->type == 0) {
                        subscribed_topics[online_clients[clients[i].fd]].insert(msg->topic);
                    } else if (msg->type == 1) {
                        subscribed_topics[online_clients[clients[i].fd]].erase(msg->topic);
                    } else if (msg->type == 2) {
                        std::cout << "Client " << online_clients[clients[i].fd] << " disconnected.\n";

                        online_clients.erase(clients[i].fd);
                        close(clients[i].fd);
                        clients.erase(clients.begin() + i);
                    }
                
                }
            }
        }
    }

    for (int i = 1; i < clients.size(); i++) {
        close(clients[i].fd);
    }
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    
    if (argc != 2) {
        return -1;
    }

    int tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sockfd < 0, "socket");

    int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_sockfd < 0, "socket");

    uint16_t port = atoi(argv[1]);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    const int enable = 1;
    if (setsockopt(tcp_sockfd, SOL_SOCKET , TCP_NODELAY, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    if (setsockopt(udp_sockfd, SOL_SOCKET, TCP_NODELAY, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");


    int rc = bind(tcp_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    DIE(rc < 0, "bind1");

    rc = bind(udp_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    DIE(rc < 0, "bind2");

    rc = listen(tcp_sockfd, INT32_MAX);
    DIE(rc < 0, "listen");

    run_server(udp_sockfd, tcp_sockfd, port);

    return 0;
}
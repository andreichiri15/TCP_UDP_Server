#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

/* Dimensiunea maxima a mesajului */
#define MSG_MAXSIZE 1500

struct chat_packet {
  uint16_t len;
  char message[MSG_MAXSIZE];
};

struct udp_message {
  char topic[50];
  uint8_t type;
  char content[MSG_MAXSIZE];
};

struct tcp_message {
    uint8_t type;
    char topic[50];
};

struct tcp_message_from_server {
    uint32_t ip;
    uint16_t port;
    tcp_message msg;
    size_t size;
};

#endif

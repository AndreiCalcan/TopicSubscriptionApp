#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

#define MSG_MAXSIZE 1024

#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                                \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

struct chat_packet {
  uint16_t len;
  uint32_t destination;
  char message[MSG_MAXSIZE + 1];
};

struct client {
    char ID[10];
};

struct UDPpacket {
    char topic[50];
    uint8_t type;
    char content[1500];
};

struct message {
    struct in_addr addr;
    uint16_t port;
    struct UDPpacket packet;
};
#endif
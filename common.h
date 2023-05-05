#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>

#include "vector.h"

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

#define MSG_MAXSIZE 100

struct chat_packet {
  uint16_t len;
  uint32_t destination;
  char message[MSG_MAXSIZE + 1];
};

struct client {
  char ID[10];
  int fd;
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

struct TCPreq {
  uint8_t type;
  char topic[50];
};

struct topic {
  char topic[50];
  struct vector *messages;
  struct vector *subscribers;
};

struct subscriber {
  struct client *client;
  int last_sent;
};
#endif
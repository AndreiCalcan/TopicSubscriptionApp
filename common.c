#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

int recv_all(int sockfd, void *buffer, size_t len) {

  size_t bytes_recv = 0;

      while(len - bytes_recv){
          ssize_t rc = recv(sockfd, buffer + bytes_recv, len -  bytes_recv, 0);
          if(rc <= 0)
            return rc;
          bytes_recv += rc;
      }

  return bytes_recv;
}

int send_all(int sockfd, void *buffer, size_t len) {
   size_t bytes_sent = 0;

      while(len - bytes_sent){
          ssize_t rc = send(sockfd, buffer + bytes_sent, len -  bytes_sent, 0);
          if(rc <= 0)
            return rc;
          bytes_sent += rc;
      }

  return bytes_sent;
}
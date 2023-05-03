#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include "common.h"

void run_client(int sockfd)
{
    char buf[MSG_MAXSIZE];
    memset(buf, 0, MSG_MAXSIZE);

    struct chat_packet sent_packet;
    struct chat_packet recv_packet;
    struct pollfd pollfds[2];

    pollfds[0].fd = 0;
    pollfds[0].events = POLLIN;
    pollfds[1].fd = sockfd;
    pollfds[1].events = POLLIN;
    /* TODO 2.2: Multiplexeaza intre citirea de la tastatura si primirea unui
     mesaj, ca sa nu mai fie impusa ordinea.
  */
    while (1)
    {
        int rc = poll(pollfds, 2, 0);
        for (int i = 0; i < 2; i++)
        {
            if (pollfds[i].revents & POLLIN)
            {
                if (pollfds[i].fd == sockfd)
                {
                    rc = recv_all(sockfd, &recv_packet, sizeof(recv_packet));
                    if (rc == 0)
                    {
                        return;
                    }
                    printf("%s", recv_packet.message);
                }
                else
                {
                    fgets(buf, sizeof(buf), stdin);
                    sent_packet.len = strlen(buf) + 1;
                    strcpy(sent_packet.message, buf);

                    // Use send_all function to send the pachet to the server.
                    send_all(sockfd, &sent_packet, sizeof(sent_packet));
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd = -1;

    if (argc != 4)
    {
        printf("\n Usage: %s <id> <ip> <port>\n", argv[0]);
        return 1;
    }

    // Parsam port-ul ca un numar
    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // Obtinem un socket TCP pentru conectarea la server
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    // Completăm in serv_addr adresa serverului, familia de adrese si portul
    // pentru conectare
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    int TCP_enable = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &TCP_enable, sizeof(int)) < 0)
        perror("setsockopt(TCP_NODELAY) failed");
    
    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");
    printf("%d\n", serv_addr.sin_addr.s_addr);

    // Ne conectăm la server
    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");

    char ID[10];
    memset(ID, 0, 10);
    strcpy(ID, argv[1]);
    send_all(sockfd, ID, 10);
    run_client(sockfd);

    // Inchidem conexiunea si socketul creat
    close(sockfd);

    return 0;
}

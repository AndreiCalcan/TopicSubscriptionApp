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
FILE *fptr;

void printMessage(struct message msg){
    char buf[100];
    int length = 0;
    memset(buf, 0, 100);
    length += sprintf(buf, "%s:%hu - %s", inet_ntoa(msg.addr), ntohs(msg.port), msg.packet.topic);

    switch (msg.packet.type) {
        case 0:
        {
            u_int8_t sign;
            u_int32_t number;
            memcpy(&sign, msg.packet.content, 1);
            memcpy(&number, msg.packet.content + 1, 4);
            number = ntohl(number);
            if(sign == 1){
                number *= -1;
            }
            printf("%s - INT - %d\n", buf, number);
            fprintf(fptr, "%s - INT - %d\n", buf, number);
            break;
        }
        case 1:
        {
            uint16_t number;
            memcpy(&number, msg.packet.content, 2);
            number = ntohs(number);
            double result = (double)number / 100;
            printf("%s - SHORT_REAL - %.2lf\n", buf, result);
            fprintf(fptr, "%s - SHORT_REAL - %.2lf\n", buf, result);
            break;
        }

         case 2:
        {
            u_int8_t sign;
            u_int32_t abs;
            u_int8_t pow;
            memcpy(&sign, msg.packet.content, 1);
            memcpy(&abs, msg.packet.content + 1, 4);
            memcpy(&pow, msg.packet.content + 5, 1);
            abs = ntohl(abs);
            double db = (double) abs;
            for(int i = pow; i != 0; i--){
                db /= 10;
            }

            if(sign == 1){
                db *= -1;
            }
            printf("%s - FLOAT - %.15g\n", buf, db);
            fprintf(fptr, "%s - FLOAT - %.15g\n", buf, db);
            break;
        }

        case 3:
        {
            printf("%s - STRING - %s\n", buf, msg.packet.content);
            fprintf(fptr, "%s - STRING - %s\n", buf, msg.packet.content);
            break;
        }
        
        default:
        {
            printf("%s WIP\n", buf);
            fprintf(fptr, "%s WIP\n", buf);
            break;
        }
    }
}

void run_client(int sockfd)
{
    char buf[MSG_MAXSIZE];
    memset(buf, 0, MSG_MAXSIZE);

    struct TCPreq sent_packet;
    struct message recv_packet;
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
                    int len;
                    rc = recv_all(sockfd, &len, sizeof(int));
                    if (rc == 0)
                    {
                        return;
                    }
                    rc = recv_all(sockfd, &recv_packet, len);
                    if (rc == 0)
                    {
                        return;
                    }
                    // printf("recved smth\n");
                    printMessage(recv_packet);
                }
                else
                {
                    fgets(buf, sizeof(buf), stdin);
                    const char delim[] = " \n";
                    memset(&sent_packet, 0, sizeof(struct TCPreq));
                    char *token = strtok(buf, delim);
                    if(token != NULL && strcmp(token, "subscribe") == 0) {
                        token = strtok(NULL, delim);
                        if(token == NULL){
                            continue;
                        }

                        strcpy(sent_packet.topic, token);
                        token = strtok(NULL, delim);
                        if(token == NULL){
                            continue;
                        }
                        if(strcmp(token, "0") == 0) {
                            sent_packet.type = 1;
                        }
                        else if(strcmp(token, "1") == 0) {
                            sent_packet.type = 2;
                        }
                        else {
                            continue;
                        }
                        send_all(sockfd, &sent_packet, sizeof(struct TCPreq));
                        printf("Subscribed to topic.\n");
                        fprintf(fptr, "Subscribed to topic.\n");
                    } 
                    else if (token != NULL && strcmp(token, "unsubscribe") == 0) {
                        token = strtok(NULL, delim);
                        if(token == NULL){
                            continue;
                        }
                        strcpy(sent_packet.topic, token);
                        sent_packet.type = 0;
                        send_all(sockfd, &sent_packet, sizeof(struct TCPreq));
                        printf("Unsubscribed from topic.\n");
                        fprintf(fptr, "Unsubscribed from topic.\n");
                    } 
                    else if (token != NULL && strcmp(token, "exit") == 0){
                        return;
                    }
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
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &TCP_enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");
    
    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    // Ne conectăm la server
    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");

    char ID[10];
    memset(ID, 0, 10);
    strcpy(ID, argv[1]);
    if(strcmp(ID, "C1") == 0){
        fptr = fopen("clientC1.out", "a");
    } else {
        fptr = fopen("clientC2.out", "a");
    }
    send_all(sockfd, ID, 10);
    run_client(sockfd);

    // Inchidem conexiunea si socketul creat
    close(sockfd);
    fclose(fptr);

    return 0;
}

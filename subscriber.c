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

// Functie utilizata pentru afisarea unui mesaj primit de la server
void printMessage(struct message msg){
    char buf[100];
    int length = 0;
    memset(buf, 0, 100);
    // Formarea headerului comun tuturor tipurilor
    length += sprintf(buf, "%s:%hu - %s", inet_ntoa(msg.addr), ntohs(msg.port), msg.packet.topic);

    // In functie de tip se interpreteaza si afiseaza diferit payloadul
    switch (msg.packet.type) {

        // Pentru INT se proceseaza primul byte ca semn
        // si urmatorii 4 ca modulul in network byte order
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
            break;
        }

        // Pentru SHORT_REAL se proceseaza primii doi bytes
        // ca modulul in network byte order inmultit cu 100
        case 1:
        {
            uint16_t number;
            memcpy(&number, msg.packet.content, 2);
            number = ntohs(number);
            double result = (double)number / 100;
            printf("%s - SHORT_REAL - %.2lf\n", buf, result);
            break;
        }

        // Pentru FLOAT se proceseaza primul byte ca semn, 
        // urmatorii 4 ca baza in network byte order inmultit
        // cu 10 la puterea celui de-al saselea byte
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
            break;
        }

        // Pentru STRING se afieseaza efectiv stringul
        case 3:
        {
            printf("%s - STRING - %s\n", buf, msg.packet.content);
            break;
        }
        
        // Pentru tip invalid se afiseaza mesaj de eroare
        default:
        {
            printf("ERROR: %s - INVALID TYPE\n", buf);
            break;
        }
    }
}

// Functia principala de rulare a clientului TCP (subscriber)
void run_client(int sockfd)
{
    char buf[MSG_MAXSIZE];
    memset(buf, 0, MSG_MAXSIZE);

    // Initializarea structurilor interne, in principal vectorul de pollfds
    struct TCPreq sent_packet;
    struct message recv_packet;
    struct pollfd pollfds[2];
    pollfds[0].fd = 0;
    pollfds[0].events = POLLIN;
    pollfds[1].fd = sockfd;
    pollfds[1].events = POLLIN;

    // Loopul principal in care ruleaza clientul
    while (1)
    {
        // Se apeleaza functia prin care se efectueaza multiplexarea
        int rc = poll(pollfds, 2, 0);
        DIE(rc < 0, "poll");

        // Se parcurg constant cei doi fds (stdin si socket)
        for (int i = 0; i < 2; i++)
        {
            // Cand se primeste un eventiment de tip POLLIN
            if (pollfds[i].revents & POLLIN)
            {
                // Daca este de pe socket
                if (pollfds[i].fd == sockfd)
                {
                    // Se primeste lungimea mesajului
                    int len;
                    rc = recv_all(sockfd, &len, sizeof(int));
                    if (rc == 0)
                    {
                        return;
                    }
                    // Se realizeaza incadrarea
                    rc = recv_all(sockfd, &recv_packet, len);
                    if (rc == 0)
                    {
                        return;
                    }

                    // Se afiseaza mesajul
                    printMessage(recv_packet);
                }
                // Daca este un eveniment de pe stdin
                else
                {
                    // Se citeste o linie
                    fgets(buf, sizeof(buf) - 1, stdin);

                    // Se tokenizeaza linia pentru procesare
                    const char delim[] = " \n";
                    memset(&sent_packet, 0, sizeof(struct TCPreq));
                    char *token = strtok(buf, delim);

                    // Daca primul token este subscribe se proceseaza restul si se arunca comanda daca este invalida
                    if(token != NULL && strcmp(token, "subscribe") == 0) {
                        token = strtok(NULL, delim);
                        if(token == NULL){
                            continue;
                        }

                        // Se copiaza topicul in pachetul care trebuie trimis
                        strcpy(sent_packet.topic, token);
                        token = strtok(NULL, delim);
                        if(token == NULL){
                            continue;
                        }

                        // Se seteaza tipul in functie de modul de abonare (stor-and-forward ON sau OFF)
                        if(strcmp(token, "0") == 0) {
                            sent_packet.type = 1;
                        }
                        else if(strcmp(token, "1") == 0) {
                            sent_packet.type = 2;
                        }
                        // Pentru tip invalid se arunca mesajul
                        else {
                            continue;
                        }

                        // Dupa procesare se trimite mesajul incadrat si se confirma pe iesire (stdout)
                        send_all(sockfd, &sent_packet, sizeof(struct TCPreq));
                        printf("Subscribed to topic.\n");
                    } 
                    // Pentru unsubscribe doar se copiaza topicul
                    else if (token != NULL && strcmp(token, "unsubscribe") == 0) 
                    {
                        token = strtok(NULL, delim);
                        if(token == NULL){
                            continue;
                        }
                        strcpy(sent_packet.topic, token);
                        sent_packet.type = 0;
                        
                        // Se trimite si confirma
                        send_all(sockfd, &sent_packet, sizeof(struct TCPreq));
                        printf("Unsubscribed from topic.\n");
                    } 

                    // Pentru exit functia principala de rulare se intoarce in main
                    else if (token != NULL && strcmp(token, "exit") == 0){
                        return;
                    }
                }
            }
        }
    }
}

// Functia apelata la rularea executabilului
int main(int argc, char *argv[])
{
    // Se dezactiveaza bufferingul la afisare
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
    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    // Dezactivam algoritmul lui Nagle
    int TCP_enable = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &TCP_enable, sizeof(int)) < 0)
        perror("setsockopt(TCP_NODELAY) failed");
    
    // Facem adresa socket-ului TCP reutilizabila, ca sa nu primim eroare in caz ca
    // rulam de 2 ori rapid
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &TCP_enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");
    
    // Ne conectăm la server
    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");

    // Trimitem IPul catre server
    char ID[10];
    memset(ID, 0, 10);
    strcpy(ID, argv[1]);
    send_all(sockfd, ID, 10);

    // Incepem rularea clientului
    run_client(sockfd);

    // Inchidem conexiunea si socketul creat
    close(sockfd);

    return 0;
}

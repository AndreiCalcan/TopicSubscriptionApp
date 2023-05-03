#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include "common.h"
#include "vector.h"

#define MAX_CLIENTS 1000
#define MAX_CONNECTIONS 1000

void printMessage(struct message msg){
    char buf[100];
    int length = 0;
    memset(buf, 0, 100);
    length += sprintf(buf, "%s:%hu - %s - %hhu -", inet_ntoa(msg.addr), ntohs(msg.port), msg.packet.topic, msg.packet.type);
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
            printf("%s %d\n", buf, number);
            break;
        }
        case 1:
        {
            uint16_t number;
            memcpy(&number, msg.packet.content, 2);
            number = ntohs(number);
            double result = (double)number / 100;
            printf("%s %.2lf\n", buf, result);
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
            printf("%s %.15g\n", buf, db);
            break;
        }

        case 3:
        {
            printf("%s %s\n", buf, msg.packet.content);
            break;
        }
        
        default:
        {
            printf("%s WIP\n", buf);
            break;
        }
    }
}

void run_chat_multi_server(int TCP_listenfd, int UDP_listenfd)
{
    Vector *pollfds = init_vector(sizeof(struct pollfd));
    int num_clients = 2;
    int rc;

    struct chat_packet received_packet;

    // Setam socket-ul listenfd pentru ascultare
    rc = listen(TCP_listenfd, MAX_CLIENTS);
    DIE(rc < 0, "listen");

    // se adauga noul file descriptor (socketul pe care se asculta conexiuni) in
    // multimea read_fds
    struct pollfd aux;
    aux.fd = TCP_listenfd;
    aux.events = POLLIN;

    add_elem_vector(pollfds, &aux);

    aux.fd = UDP_listenfd;
    aux.events = POLLIN;

    add_elem_vector(pollfds, &aux);

    Vector *clients = init_vector(sizeof(struct client));

    while (1)
    {
        struct pollfd *poll_fds = (struct pollfd *) pollfds->vector;
        num_clients = pollfds->length;
        rc = poll(poll_fds, num_clients, 0);
        DIE(rc < 0, "poll");

        for (int i = 0; i < num_clients; i++)
        {
            if (poll_fds[i].revents & POLLIN)
            {
                printf("sper aici\n");
                if (poll_fds[i].fd == TCP_listenfd)
                {
                    printf("aici\n");
                    // a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
                    // pe care serverul o accepta
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    int newsockfd = accept(TCP_listenfd, (struct sockaddr *)&cli_addr, &cli_len);
                    DIE(newsockfd < 0, "accept");

                    // se adauga noul socket intors de accept() la multimea descriptorilor
                    // de citire
                    aux.fd = newsockfd;
                    aux.events = POLLIN;
                    add_elem_vector(pollfds, &aux);
                    struct pollfd *poll_fds = (struct pollfd *) pollfds->vector;

                    struct client new_client;
                    recv_all(newsockfd, new_client.ID, 10);

                    int found = 0;
                    struct client *crt_clients = (struct client *) clients->vector;
                    for(int i = 0; i < clients->length && found == 0; i++){
                        if(strcmp(new_client.ID, crt_clients[i].ID) == 0){
                            found = 1;
                            printf("Already existing client %s connected from %s:%d, socket client %d\n",
                           new_client.ID, inet_ntoa(cli_addr.sin_addr), 
                           ntohs(cli_addr.sin_port), newsockfd);
                        }
                    }

                    if(found == 0)
                    { 
                        add_elem_vector(clients, &new_client);
                        printf("New client %s connected from %s:%d, socket client %d\n",
                           new_client.ID, inet_ntoa(cli_addr.sin_addr), 
                           ntohs(cli_addr.sin_port), newsockfd);
                    }
                }
                else if(poll_fds[i].fd == UDP_listenfd) {
                    struct message packet;
                    struct sockaddr_in client_addr;
                    socklen_t clen = sizeof(client_addr);
                    int rc = recvfrom(UDP_listenfd, &(packet.packet), sizeof(struct UDPpacket), 0, (struct sockaddr *)&client_addr, &clen);
                    DIE(rc < 0, "recv");
                    int ack = 0;
                    rc = sendto(UDP_listenfd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, clen);
                    DIE(rc < 0, "send");

                    packet.port = client_addr.sin_port;
                    packet.addr = client_addr.sin_addr;                    
                    printMessage(packet);
                }
                else
                {
                    printf("nu aici\n");
                    // s-au primit date pe unul din socketii de client,
                    // asa ca serverul trebuie sa le receptioneze
                    int rc = recv_all(poll_fds[i].fd, &received_packet,
                                      sizeof(received_packet));
                    DIE(rc < 0, "recv");

                    if (rc == 0)
                    {
                        // conexiunea s-a inchis
                        printf("Socket-ul client %d a inchis conexiunea\n", i);
                        close(poll_fds[i].fd);

                        // se scoate din multimea de citire socketul inchis
                        for (int j = i; j < pollfds->length - 1; j++)
                        {
                            poll_fds[j] = poll_fds[j + 1];
                        }

                        num_clients--;
                        (pollfds->length)--;
                    }
                    else
                    {
                        printf("S-a primit de la clientul de pe socketul %d mesajul: %s\n",
                               poll_fds[i].fd, received_packet.message);
                        /* TODO 2.1: Trimite mesajul catre toti ceilalti clienti */
                        // for (int j = 1; j < num_clients; j++)
                        // {
                        //     if (i != j)
                        //         send_all(poll_fds[j].fd, &received_packet, sizeof(struct chat_packet));
                        // }
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    if (argc != 2)
    {
        printf("\n Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // Parsam port-ul ca un numar
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // Obtinem un socket TCP pentru receptionarea conexiunilor
    int TCP_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(TCP_listenfd < 0, "socket TCP");

    int UDP_listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(UDP_listenfd < 0, "socket UDP");

    // Completăm in serv_addr adresa serverului, familia de adrese si portul
    // pentru conectare
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    // Facem adresa socket-ului reutilizabila, ca sa nu primim eroare in caz ca
    // rulam de 2 ori rapid
    int UDP_enable = 1;
    if (setsockopt(UDP_listenfd, SOL_SOCKET, SO_REUSEADDR, &UDP_enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    int TCP_enable = 1;
    if (setsockopt(TCP_listenfd, IPPROTO_TCP, TCP_NODELAY, &TCP_enable, sizeof(int)) < 0)
        perror("setsockopt(TCP_NODELAY) failed");

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("%d\n", serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    // Asociem adresa serverului cu socketul creat folosind bind
    rc = bind(TCP_listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind");
    rc = bind(UDP_listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind");

    // run_chat_server(listenfd);
    run_chat_multi_server(TCP_listenfd, UDP_listenfd);

    // Ichidem listenfd
    close(TCP_listenfd);
    close(UDP_listenfd);

    return 0;
}

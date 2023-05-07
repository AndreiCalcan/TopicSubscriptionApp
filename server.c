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

#include "common.h"

FILE *fptr;
void send_variable(int type, int sockfd, void *data)
{
    switch (type)
    {
    case 0:
    {
        int len = 65;
        send_all(sockfd, &len, sizeof(int));
        send_all(sockfd, data, len);
        break;
    }
    case 1:
    {
        int len = 62;
        send_all(sockfd, &len, sizeof(int));
        send_all(sockfd, data, len);
        break;
    }
    case 2:
    {
        int len = 66;
        send_all(sockfd, &len, sizeof(int));
        send_all(sockfd, data, len);
        break;
    }
    case 3:
    {
        struct message *payload = data;
        char aux = payload->packet.content[1499];
        payload->packet.content[1499] = '\0';
        int len = 60 + strlen(payload->packet.content) + 1;
        payload->packet.content[1499] = aux;
        send_all(sockfd, &len, sizeof(int));
        send_all(sockfd, data, len);
        break;
    }
    }
}

void run_server(int TCP_listenfd, int UDP_listenfd)
{
    fptr = fopen("server.out", "w");
    Vector *pollfds = init_vector(sizeof(struct pollfd));
    int num_clients = 3;
    int rc;

    // Setam socket-ul listenfd pentru ascultare
    rc = listen(TCP_listenfd, SOMAXCONN);
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

    aux.fd = 0;
    aux.events = POLLIN;

    add_elem_vector(pollfds, &aux);

    Vector *clients = init_vector(sizeof(struct client));
    Vector *topics = init_vector(sizeof(struct topic));
    char buf[MSG_MAXSIZE];
    memset(buf, 0, MSG_MAXSIZE);

    while (1)
    {
        struct pollfd *poll_fds = (struct pollfd *)pollfds->vector;
        num_clients = pollfds->length;
        rc = poll(poll_fds, num_clients, 0);
        DIE(rc < 0, "poll");

        for (int i = 0; i < num_clients; i++)
        {
            if (poll_fds[i].revents & POLLIN)
            {
                // printf("sper aici\n");
                if (poll_fds[i].fd == 0)
                {
                    // printf("sper aici\n");
                    fgets(buf, sizeof(buf), stdin);
                    if (strcmp(buf, "exit\n") == 0)
                    {
                        struct topic *crt_topics = (struct topic *)topics->vector;
                        for(int j = 0; j < topics->length; j++){
                            free_vector(crt_topics[j].messages);
                            free_vector(crt_topics[j].subscribers);
                        }
                        free_vector(topics);
                        free_vector(clients);
                        free_vector(pollfds);
                        return;
                    }
                }

                else if (poll_fds[i].fd == TCP_listenfd)
                {
                    // printf("aici\n");
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
                    poll_fds = (struct pollfd *)pollfds->vector;

                    struct client new_client;
                    recv_all(newsockfd, new_client.ID, 10);

                    int found = 0;
                    struct client *crt_clients = (struct client *)clients->vector;
                    for (int j = 0; j < clients->length && found == 0; j++)
                    {
                        if (strcmp(new_client.ID, crt_clients[j].ID) == 0)
                        {
                            found = 1;
                            if (crt_clients[j].fd != -1)
                            {
                                printf("Client %s already connected.\n", new_client.ID);
                                fprintf(fptr, "Client %s already connected.\n", new_client.ID);
                                close(newsockfd);
                                (pollfds->length)--;
                            }
                            else
                            {
                                crt_clients[j].fd = newsockfd;
                                struct topic *crt_topics = (struct topic *)topics->vector;
                                for (int k = 0; k < topics->length; k++)
                                {
                                    struct subscriber *subscribers = crt_topics[k].subscribers->vector;
                                    struct message *crt_msgs = crt_topics[k].messages->vector;
                                    int min = INT32_MAX;
                                    for (int l = 0; l < crt_topics[k].subscribers->length; l++)
                                    {
                                        if (subscribers[l].client->fd == newsockfd && subscribers[l].last_sent > -2)
                                        {
                                            for (int m = subscribers[l].last_sent + 1; m < crt_topics[k].messages->length; m++)
                                            {
                                                fprintf(fptr, "Sending in bulk msgs\n");       
                                                // printf("trying to send message on topic %s\n", crt_msgs[m].packet.content);
                                                send_variable(crt_msgs[m].packet.type, newsockfd, &crt_msgs[m]);
                                                
                                            }
                                            subscribers[l].last_sent = crt_topics[k].messages->length - 1;
                                        }

                                        if(subscribers[l].last_sent < min && subscribers[l].last_sent > -2) {
                                            min = subscribers[l].last_sent;
                                        }
                                    }
                                    if(min >= 0 && min != INT32_MAX) {
                                        fprintf(fptr, "Deleting %d pointless messages from topic %s\n", min + 1, crt_topics[k].topic);

                                        for (int l = 0; l < crt_topics[k].subscribers->length; l++){
                                            if(subscribers[l].last_sent > -2) {
                                                subscribers[l].last_sent = subscribers[l].last_sent - min - 1;
                                            }
                                        }

                                        while(min >= 0) {
                                            for (int l = 0; l < crt_topics[k].messages->length - 1; l++)
                                            {
                                                crt_msgs[l] = crt_msgs[l + 1];
                                            }
                                            (crt_topics[k].messages->length)--;
                                            min--;
                                        }
                                    }
                                }
                                printf("New client %s connected from %s:%hu.\n",
                                       new_client.ID, inet_ntoa(cli_addr.sin_addr),
                                       ntohs(cli_addr.sin_port));
                                fprintf(fptr, "New client %s connected from %s:%hu.\n",
                                        new_client.ID, inet_ntoa(cli_addr.sin_addr),
                                        ntohs(cli_addr.sin_port));
                                // printf("Already existing client %s connected from %s:%d, socket client %d\n",
                                // new_client.ID, inet_ntoa(cli_addr.sin_addr),
                                // ntohs(cli_addr.sin_port), newsockfd);
                            }
                            break;
                        }
                    }

                    if (found == 0)
                    {
                        new_client.fd = newsockfd;
                        add_elem_vector(clients, &new_client);
                        printf("New client %s connected from %s:%hu.\n",
                               new_client.ID, inet_ntoa(cli_addr.sin_addr),
                               ntohs(cli_addr.sin_port));
                        fprintf(fptr, "New client %s connected from %s:%hu.\n",
                                new_client.ID, inet_ntoa(cli_addr.sin_addr),
                                ntohs(cli_addr.sin_port));
                    }
                }
                else if (poll_fds[i].fd == UDP_listenfd)
                {
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
                    int found = 0;
                    struct topic *crt_topics = (struct topic *)topics->vector;
                    for (int j = 0; j < topics->length; j++)
                    {
                        if (strncmp(packet.packet.topic, crt_topics[j].topic, 50) == 0)
                        {
                            found = 1;
                            uint8_t need_storage = 0;
                            struct subscriber *subscribers = crt_topics[j].subscribers->vector;
                            for (int k = 0; k < crt_topics[j].subscribers->length; k++)
                            {
                                if (subscribers[k].client->fd != -1)
                                {
                                    // printf("trying to send message to %s on topic %s\n", subscribers[k].client->ID, packet.packet.topic);
                                    // send_all(subscribers[k].client->fd, &packet, sizeof(struct message));
                                    send_variable(packet.packet.type, subscribers[k].client->fd, &packet);
                                    // printf("sent message to %s on topic %s\n", subscribers[k].client->ID, packet.packet.topic);
                                }
                                else
                                {
                                    if (subscribers[k].last_sent != -2)
                                    {
                                        need_storage = 1;
                                    }
                                }
                            }
                            if (need_storage == 1)
                            {
                                for (int k = 0; k < crt_topics[j].subscribers->length; k++)
                                {
                                    if (subscribers[k].client->fd != -1 && subscribers[k].last_sent != -2)
                                    {
                                        (subscribers[k].last_sent)++;
                                    }
                                }
                                // printf("storing message on topic %s\n", packet.packet.topic);
                                // printMessage(packet);
                                add_elem_vector(crt_topics[j].messages, &packet);

                                // struct message *msg = crt_topics[j].messages->vector;
                                // printf("stored message number %d on topic %s\n",crt_topics[j].messages->length, msg[crt_topics[j].messages->length - 1].packet.topic);
                                // printMessage(msg[crt_topics[j].messages->length - 1]);
                            }
                        }
                    }
                    if (found == 0)
                    {
                        struct topic new_topic;
                        new_topic.messages = init_vector(sizeof(struct message));
                        new_topic.subscribers = init_vector(sizeof(struct subscriber));
                        memcpy(new_topic.topic, packet.packet.topic, 50);
                        add_elem_vector(topics, &new_topic);
                        // struct topic *top = topics->vector;
                        // printf("Added topic number %d - %s\n",topics->length - 1, top[topics->length - 1].topic);
                    }
                    // printMessage(packet);
                }
                else
                {
                    struct TCPreq received_packet;
                    // printf("nu aici\n");
                    int client_index = -1;
                    struct client *clients_arr = (struct client *)clients->vector;
                    for (int j = 0; j < clients->length; j++)
                    {
                        if (clients_arr[j].fd == poll_fds[i].fd)
                        {
                            client_index = j;
                            break;
                        }
                    }
                    DIE(client_index == -1, "client not found");
                    // s-au primit date pe unul din socketii de client,
                    // asa ca serverul trebuie sa le receptioneze
                    int rc = recv_all(poll_fds[i].fd, &received_packet,
                                      sizeof(received_packet));
                    DIE(rc < 0, "recv");

                    if (rc == 0)
                    {
                        // conexiunea s-a inchis
                        printf("Client %s disconnected.\n", clients_arr[client_index].ID);
                        fprintf(fptr, "Client %s disconnected.\n", clients_arr[client_index].ID);
                        close(poll_fds[i].fd);
                        clients_arr[client_index].fd = -1;
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
                        // printf("Command from %s - type %hhd - topic %s\n",
                        //        clients_arr[client_index].ID, received_packet.type, received_packet.topic);
                        switch (received_packet.type)
                        {
                        case 0:
                        {
                            struct topic *crt_topics = (struct topic *)topics->vector;
                            for (int j = 0; j < topics->length; j++)
                            {
                                if (strncmp(received_packet.topic, crt_topics[j].topic, 50) == 0)
                                {
                                    fprintf(fptr, "Trying to unsub from %s\n", received_packet.topic);
                                    struct subscriber *subscribers = crt_topics[j].subscribers->vector;
                                    for (int k = 0; k < crt_topics[j].subscribers->length; k++)
                                    {
                                        fprintf(fptr, "Checking subscriber %s, on fd %d compared to %d\n", subscribers[k].client->ID, subscribers[k].client->fd, poll_fds[i].fd);
                                        if (subscribers[k].client->fd == poll_fds[i].fd)
                                        {
                                            for (int l = k; l < crt_topics[j].subscribers->length - 1; l++)
                                            {
                                                subscribers[l] = subscribers[l + 1];
                                            }
                                            (crt_topics[j].subscribers->length)--;
                                            fprintf(fptr, "Unsubscribed client from topic %s, %d subs remain\n", received_packet.topic, crt_topics[j].subscribers->length);
                                        }
                                    }
                                }
                            }
                            break;
                        }
                        case 1:
                        {
                            uint8_t found = 0;
                            struct topic *crt_topics = (struct topic *)topics->vector;
                            for (int j = 0; j < topics->length; j++)
                            {
                                if (strncmp(received_packet.topic, crt_topics[j].topic, 50) == 0)
                                {
                                    found = 1;
                                    int already_subscribed = 0;
                                    struct subscriber* subs = crt_topics[j].subscribers->vector;
                                    for(int k = 0; k < crt_topics[j].subscribers->length; k++){
                                        if(subs[k].client->fd == poll_fds[i].fd) {
                                            already_subscribed = 1;
                                            subs[k].last_sent = -2;
                                        }
                                        break;
                                    }
                                    if (already_subscribed == 0) {
                                        struct subscriber new_subscriber;
                                        new_subscriber.client = &clients_arr[client_index];
                                        new_subscriber.last_sent = -2;
                                        add_elem_vector(crt_topics[j].subscribers, &new_subscriber);
                                    }
                                    break;
                                }
                            }

                            if (found == 0)
                            {
                                struct topic new_topic;
                                new_topic.messages = init_vector(sizeof(struct message));
                                new_topic.subscribers = init_vector(sizeof(struct subscriber));
                                memcpy(new_topic.topic, received_packet.topic, 50);
                                struct subscriber new_subscriber;
                                new_subscriber.client = &clients_arr[client_index];
                                new_subscriber.last_sent = -2;
                                add_elem_vector(new_topic.subscribers, &new_subscriber);
                                add_elem_vector(topics, &new_topic);
                            }
                            break;
                        }
                        case 2:
                        {
                            uint8_t found = 0;
                            struct topic *crt_topics = (struct topic *)topics->vector;
                            for (int j = 0; j < topics->length; j++)
                            {
                                if (strncmp(received_packet.topic, crt_topics[j].topic, 50) == 0)
                                {
                                    found = 1;
                                    int already_subscribed = 0;
                                    struct subscriber* subs = crt_topics[j].subscribers->vector;
                                    for(int k = 0; k < crt_topics[j].subscribers->length; k++){
                                        if(subs[k].client->fd == poll_fds[i].fd) {
                                            already_subscribed = 1;
                                            subs[k].last_sent = crt_topics[j].messages->length - 1;
                                        }
                                        break;
                                    }
                                    if(already_subscribed == 0) {
                                        struct subscriber new_subscriber;
                                        new_subscriber.client = &clients_arr[client_index];
                                        new_subscriber.last_sent = crt_topics[j].messages->length - 1;
                                        add_elem_vector(crt_topics[j].subscribers, &new_subscriber);
                                    }
                                    break;
                                }
                            }

                            if (found == 0)
                            {
                                struct topic new_topic;
                                new_topic.messages = init_vector(sizeof(struct message));
                                new_topic.subscribers = init_vector(sizeof(struct subscriber));
                                memcpy(new_topic.topic, received_packet.topic, 50);
                                struct subscriber new_subscriber;
                                new_subscriber.client = &clients_arr[client_index];
                                new_subscriber.last_sent = -1;
                                add_elem_vector(new_topic.subscribers, &new_subscriber);
                                add_elem_vector(topics, &new_topic);
                            }
                            break;
                        }

                        default:
                            break;
                        }
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

    if (setsockopt(TCP_listenfd, SOL_SOCKET, SO_REUSEADDR, &TCP_enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    DIE(rc <= 0, "inet_pton");

    // Asociem adresa serverului cu socketul creat folosind bind
    rc = bind(TCP_listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind");
    rc = bind(UDP_listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind");

    // run_chat_server(listenfd);
    run_server(TCP_listenfd, UDP_listenfd);

    // Ichidem listenfd
    close(TCP_listenfd);
    close(UDP_listenfd);
    fclose(fptr);

    return 0;
}

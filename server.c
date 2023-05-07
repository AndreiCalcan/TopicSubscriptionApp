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

// Functie de trimitere de pachete cu lungime variabila 
// printr-un protocol de nivel aplicatie
void send_variable(int type, int sockfd, void *data)
{
    // Lungimea se determina in functie de tipul de date trimis
    switch (type)
    {

    // Pentru INT lungimea payloadului este 5
    case 0:
    {
        int len = 65;
        send_all(sockfd, &len, sizeof(int));
        send_all(sockfd, data, len);
        break;
    }

    // Pentru SHORT_REAL lungimea payloadului este 2;
    case 1:
    {
        int len = 62;
        send_all(sockfd, &len, sizeof(int));
        send_all(sockfd, data, len);
        break;
    }

    // Pentru FLOAT lungimea payloadului este 6
    case 2:
    {
        int len = 66;
        send_all(sockfd, &len, sizeof(int));
        send_all(sockfd, data, len);
        break;
    }

    // Pentru STRING lunigmea payloadului este lungimea stringului
    // limitata superior de 1500 de caractere prin fortarea unui
    // caracter null la final
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

    // Pentru tip invalid nu se trimite nimic
    default:
        break;
    }
}


// Functia principala de rulare a serverului primeste ca parametrii file descriptorii
// pe care se accepta conexiuni pentru TCP si pe care se trimit pachete pentru UDP
void run_server(int TCP_listenfd, int UDP_listenfd)
{
    // Se initializeaza vectorul folosit pentru poll_fds
    Vector *pollfds = init_vector(sizeof(struct pollfd));
    int num_clients = 3;
    int rc;

    // Setam socket-ul de TCP pentru ascultare
    rc = listen(TCP_listenfd, SOMAXCONN);
    DIE(rc < 0, "listen");

    // Se adauga file descriptorii initiali
    struct pollfd aux;

    // Fd pentru socketul de conexiune TCP
    aux.fd = TCP_listenfd;
    aux.events = POLLIN;
    add_elem_vector(pollfds, &aux);

    // Fd pentru primire pachete UDP
    aux.fd = UDP_listenfd;
    aux.events = POLLIN;
    add_elem_vector(pollfds, &aux);

    // Fd pentru intrarile de la tastatura (stdin)
    aux.fd = 0;
    aux.events = POLLIN;
    add_elem_vector(pollfds, &aux);

    // Se initializeaza structurile interne pentru memorizare de clienti si mesaje sortate pe topice
    Vector *clients = init_vector(sizeof(struct client));
    Vector *topics = init_vector(sizeof(struct topic));
    char buf[MSG_MAXSIZE];
    memset(buf, 0, MSG_MAXSIZE);

    // Loopul principal pe care ruleaza serverul pana la oprire
    while (1)
    {
        // Se actualizeaza pointerii cu ce se afla in memorie
        struct pollfd *poll_fds = (struct pollfd *)pollfds->vector;
        num_clients = pollfds->length;

        // Se efectueaza multiplexarea prin poll pe fd din poll_fds
        rc = poll(poll_fds, num_clients, 0);
        DIE(rc < 0, "poll");

        // Parcurgere prin toti descriptorii multiplexati
        for (int i = 0; i < num_clients; i++)
        {
            // Unde se primeste un event de tipul POLLIN
            if (poll_fds[i].revents & POLLIN)
            {
                // Pentru intrari de la stdin
                if (poll_fds[i].fd == 0)
                {
                    // Se citeste o linie cu lungimea maxima 100
                    fgets(buf, sizeof(buf) - 1, stdin);

                    // Pentru comanda exit se elibereaza memoria alocata pe heap si returneaza apelul
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

                // Pentru intrari pe socketul de ascultare de conexiuni TCP
                else if (poll_fds[i].fd == TCP_listenfd)
                {
                    // Serverul accepta conexiunea 
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    int newsockfd = accept(TCP_listenfd, (struct sockaddr *)&cli_addr, &cli_len);
                    DIE(newsockfd < 0, "accept");

                    // Adauga in vectorul de poll_fds noul socket deschis
                    aux.fd = newsockfd;
                    aux.events = POLLIN;
                    add_elem_vector(pollfds, &aux);
                    poll_fds = (struct pollfd *)pollfds->vector;

                    // Asteapta pe acest socket sa primeasca de la client ID-ul sau
                    struct client new_client;
                    recv_all(newsockfd, new_client.ID, 10);

                    // Cauta in memorie daca IDul primit a mai fost conectat precedent
                    int found = 0;
                    struct client *crt_clients = (struct client *)clients->vector;
                    for (int j = 0; j < clients->length && found == 0; j++)
                    {
                        // Daca il gaseste deja in memorie si are deja un client conectat sub acel ID inchide conexiunea
                        if (strcmp(new_client.ID, crt_clients[j].ID) == 0)
                        {
                            found = 1;
                            if (crt_clients[j].fd != -1)
                            {
                                printf("Client %s already connected.\n", new_client.ID);
                                close(newsockfd);
                                (pollfds->length)--;
                            }

                            // Pentru ID cunoscut cauta in memorie la fiecare topic daca exista un subscription
                            // cu store-and-forward setat si trimite mesajele primite de la deconectarea utilizatorului
                            // Aditional, se sterg toate mesajele inutile care au fost trimise deja tuturor abonatilor
                            else
                            {
                                crt_clients[j].fd = newsockfd;
                                struct topic *crt_topics = (struct topic *)topics->vector;
                                for (int k = 0; k < topics->length; k++)
                                {
                                    struct subscriber *subscribers = crt_topics[k].subscribers->vector;
                                    struct message *crt_msgs = crt_topics[k].messages->vector;
                                    int min = INT32_MAX;

                                    // Prin parcurgerea subscriberilor se determina cel mai vechi mesaje netrimis (minimul dintre campurile last_sent)
                                    for (int l = 0; l < crt_topics[k].subscribers->length; l++)
                                    {
                                        // Pentru un subscription de la clientul nou conectat se trimit mesajele restante
                                        if (subscribers[l].client->fd == newsockfd && subscribers[l].last_sent > -2)
                                        {
                                            for (int m = subscribers[l].last_sent + 1; m < crt_topics[k].messages->length; m++)
                                            {   
                                                send_variable(crt_msgs[m].packet.type, newsockfd, &crt_msgs[m]);
                                            }
                                            subscribers[l].last_sent = crt_topics[k].messages->length - 1;
                                        }
                                        
                                        // Se proceseaza minimul ignorand abonarile facute fara SF
                                        if(subscribers[l].last_sent < min && subscribers[l].last_sent > -2) {
                                            min = subscribers[l].last_sent;
                                        }
                                    }

                                    // Daca exista mesaje inutile
                                    if (min >= 0 && min != INT32_MAX) {

                                        // Actualizeaza informatia legata de ultimul mesaj trimis pentru fiecare subscriber
                                        for (int l = 0; l < crt_topics[k].subscribers->length; l++){
                                            if(subscribers[l].last_sent > -2) {
                                                subscribers[l].last_sent = subscribers[l].last_sent - min - 1;
                                            }
                                        }

                                        // Se sterge cel mai vechi mesaj pana se ajunge la unul netrimis
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
                                // Serverul anunta noua conexiune si se opreste cautarea prin clientii din memorie
                                printf("New client %s connected from %s:%hu.\n",
                                       new_client.ID, inet_ntoa(cli_addr.sin_addr),
                                       ntohs(cli_addr.sin_port));
                            }
                            break;
                        }
                    }

                    // Daca clientul nu a fost gasit se introduce in memorie si se afiseaza conexiunea la stdout
                    if (found == 0)
                    {
                        new_client.fd = newsockfd;
                        add_elem_vector(clients, &new_client);
                        printf("New client %s connected from %s:%hu.\n",
                               new_client.ID, inet_ntoa(cli_addr.sin_addr),
                               ntohs(cli_addr.sin_port));
                    }
                }

                // Pentru pachete primite de la un client UDP
                else if (poll_fds[i].fd == UDP_listenfd)
                {
                    // Se primeste si mesajul si e trimite un ack
                    struct message packet;
                    struct sockaddr_in client_addr;
                    socklen_t clen = sizeof(client_addr);
                    int rc = recvfrom(UDP_listenfd, &(packet.packet), sizeof(struct UDPpacket), 0, (struct sockaddr *)&client_addr, &clen);
                    DIE(rc < 0, "recv");
                    int ack = 0;
                    rc = sendto(UDP_listenfd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, clen);
                    DIE(rc < 0, "send");

                    // Se completeaza portul si adresa de pe care au venit mesajele
                    packet.port = client_addr.sin_port;
                    packet.addr = client_addr.sin_addr;

                    // Se efectueaza o cautare in memorie dupa topicul mesajului primit
                    int found = 0;
                    struct topic *crt_topics = (struct topic *)topics->vector;
                    for (int j = 0; j < topics->length; j++)
                    {
                        // Daca topicul este gasit se parcurg abonatii
                        if (strncmp(packet.packet.topic, crt_topics[j].topic, 50) == 0)
                        {
                            found = 1;
                            uint8_t need_storage = 0;
                            struct subscriber *subscribers = crt_topics[j].subscribers->vector;
                            for (int k = 0; k < crt_topics[j].subscribers->length; k++)
                            {
                                // Pentru abonatii conectati se trimite direct mesajul
                                if (subscribers[k].client->fd != -1)
                                {
                                    send_variable(packet.packet.type, subscribers[k].client->fd, &packet);
                                }
                                else
                                {
                                    // Pentru cei deconectati care au store-and-forward enabled se sesizeaza nevoia de a memoriza mesajul
                                    if (subscribers[k].last_sent != -2)
                                    {
                                        need_storage = 1;
                                    }
                                }
                            }

                            // Pentru cand mesajul trebuie memorizat se actualizeaza ultimul mesaj trimis celor conectati si abonati cu SF
                            if (need_storage == 1)
                            {
                                for (int k = 0; k < crt_topics[j].subscribers->length; k++)
                                {
                                    if (subscribers[k].client->fd != -1 && subscribers[k].last_sent != -2)
                                    {
                                        (subscribers[k].last_sent)++;
                                    }
                                }
                                // Se adauga mesajul in memorie
                                add_elem_vector(crt_topics[j].messages, &packet);
                            }
                        }
                    }
                    // Cand topicul nu a fost gasit se initializeaza si adauga in memorie
                    if (found == 0)
                    {
                        struct topic new_topic;
                        new_topic.messages = init_vector(sizeof(struct message));
                        new_topic.subscribers = init_vector(sizeof(struct subscriber));
                        memcpy(new_topic.topic, packet.packet.topic, 50);
                        add_elem_vector(topics, &new_topic);
                    }
                }

                // Pentru mesajele primite de la un client TCP
                else
                {
                    // Se cauta clientul in memorie si se determina indexul lui in lista de clienti
                    struct TCPreq received_packet;
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

                    // Se receptioneaza mesajul de pe socket
                    int rc = recv_all(poll_fds[i].fd, &received_packet,
                                      sizeof(received_packet));
                    DIE(rc < 0, "recv");

                    // Pentru cand se primeste un mesaj de lungime 0
                    if (rc == 0)
                    {
                        // Se inchide conexiunea si se anunta la stdout
                        close(poll_fds[i].fd);
                        printf("Client %s disconnected.\n", clients_arr[client_index].ID);
                        clients_arr[client_index].fd = -1;

                        // Se scoate din multimea de citire socketul inchis
                        for (int j = i; j < pollfds->length - 1; j++)
                        {
                            poll_fds[j] = poll_fds[j + 1];
                        }

                        num_clients--;
                        (pollfds->length)--;
                    }
                    // Pentru pachete cu lungimea > 0
                    else
                    {
                        // Se actioneaza in functie de tipul comenzii
                        switch (received_packet.type)
                        {

                        // Pentru comenzi de dezabonare (unsubscribe)
                        case 0:
                        {
                            // Se cauta in memorie topicul si se sterge orice abonament pe Idul clientului la acel topic
                            struct topic *crt_topics = (struct topic *)topics->vector;
                            for (int j = 0; j < topics->length; j++)
                            {
                                if (strncmp(received_packet.topic, crt_topics[j].topic, 50) == 0)
                                {
                                    struct subscriber *subscribers = crt_topics[j].subscribers->vector;
                                    for (int k = 0; k < crt_topics[j].subscribers->length; k++)
                                    {
                                        if (subscribers[k].client->fd == poll_fds[i].fd)
                                        {
                                            for (int l = k; l < crt_topics[j].subscribers->length - 1; l++)
                                            {
                                                subscribers[l] = subscribers[l + 1];
                                            }
                                            (crt_topics[j].subscribers->length)--;
                                        }
                                    }
                                }
                            }
                            break;
                        }

                        // Pentru comenzi de abonare fara SF (subscribe topic 0)
                        case 1:
                        {
                            // Se cauta topicul
                            uint8_t found = 0;
                            struct topic *crt_topics = (struct topic *)topics->vector;
                            for (int j = 0; j < topics->length; j++)
                            {
                                // Daca este gasit topicul
                                if (strncmp(received_packet.topic, crt_topics[j].topic, 50) == 0)
                                {
                                    // Se cauta printre abonamente daca exista deja unul facut pe IDul clientului
                                    found = 1;
                                    int already_subscribed = 0;
                                    struct subscriber* subs = crt_topics[j].subscribers->vector;
                                    for(int k = 0; k < crt_topics[j].subscribers->length; k++)
                                    {
                                        // Daca exista deja unul actualizeaza tipul abonamentului
                                        if(subs[k].client->fd == poll_fds[i].fd) 
                                        {
                                            already_subscribed = 1;
                                            subs[k].last_sent = -2;
                                        }
                                        break;
                                    }

                                    // Daca nu este gasit un abonament creeaza unul nou si il adauga in memorie
                                    if (already_subscribed == 0) 
                                    {
                                        struct subscriber new_subscriber;
                                        new_subscriber.client = &clients_arr[client_index];
                                        new_subscriber.last_sent = -2;
                                        add_elem_vector(crt_topics[j].subscribers, &new_subscriber);
                                    }
                                    break;
                                }
                            }

                            // Daca topicul nu este gasit, se creeaza si se adauga noul subscriber
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

                        // Pentru comenzi de abonare cu SF (subscribe topic 1)
                        case 2:
                        {
                            // Se cauta topicul
                            uint8_t found = 0;
                            struct topic *crt_topics = (struct topic *)topics->vector;
                            for (int j = 0; j < topics->length; j++)
                            {
                                // Daca este gasit topicul
                                if (strncmp(received_packet.topic, crt_topics[j].topic, 50) == 0)
                                {
                                    // Se cauta printre abonamente daca exista deja unul facut pe IDul clientului
                                    found = 1;
                                    int already_subscribed = 0;
                                    struct subscriber* subs = crt_topics[j].subscribers->vector;
                                    for(int k = 0; k < crt_topics[j].subscribers->length; k++)
                                    {
                                        // Daca exista deja unul actualizeaza tipul abonamentului
                                        if(subs[k].client->fd == poll_fds[i].fd) 
                                        {
                                            already_subscribed = 1;
                                            subs[k].last_sent = crt_topics[j].messages->length - 1;
                                        }
                                        break;
                                    }

                                    // Daca nu este gasit un abonament creeaza unul nou si il adauga in memorie
                                    if(already_subscribed == 0) 
                                    {
                                        struct subscriber new_subscriber;
                                        new_subscriber.client = &clients_arr[client_index];
                                        new_subscriber.last_sent = crt_topics[j].messages->length - 1;
                                        add_elem_vector(crt_topics[j].subscribers, &new_subscriber);
                                    }
                                    break;
                                }
                            }

                            // Daca topicul nu este gasit, se creeaza si se adauga noul subscriber
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

                        // Pentru un tip invalid se ignora pachetul primit
                        default:
                            break;
                        }
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
    int rc = setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    DIE(rc != 0, "setvbuf");

    // Se verifica ca numarul de argumente primite in linia de comanda este corect
    if (argc != 2)
    {
        printf("\n Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // Parsam port-ul ca un numar
    uint16_t port;
    rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // Obtinem un socket TCP pentru receptionarea conexiunilor
    int TCP_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(TCP_listenfd < 0, "socket TCP");

    // Obtinem un socket UDP pentru receptionarea de mesaje de la astfel de clienti
    int UDP_listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(UDP_listenfd < 0, "socket UDP");

    // Completăm in serv_addr adresa serverului, familia de adrese si portul
    // pentru conectare
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);
    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    DIE(rc <= 0, "inet_pton");

    // Facem adresa socket-ului UDP reutilizabila, ca sa nu primim eroare in caz ca
    // rulam de 2 ori rapid
    int UDP_enable = 1;
    if (setsockopt(UDP_listenfd, SOL_SOCKET, SO_REUSEADDR, &UDP_enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    // Dezactivam algoritmul lui Nagle
    int TCP_enable = 1;
    if (setsockopt(TCP_listenfd, IPPROTO_TCP, TCP_NODELAY, &TCP_enable, sizeof(int)) < 0)
        perror("setsockopt(TCP_NODELAY) failed");

    // Facem adresa socket-ului TCP reutilizabila, ca sa nu primim eroare in caz ca
    // rulam de 2 ori rapid
    if (setsockopt(TCP_listenfd, SOL_SOCKET, SO_REUSEADDR, &TCP_enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    // Asociem adresa serverului cu socketii creati folosind bind
    rc = bind(TCP_listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind");
    rc = bind(UDP_listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind");

    // Incepem rularea
    run_server(TCP_listenfd, UDP_listenfd);

    // Inchidem socketii
    close(TCP_listenfd);
    close(UDP_listenfd);

    return 0;
}

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/timerfd.h>

#include "common.h"
#include "helpers.h"

#define BUFLEN 1600
#define MAX_CONNECTIONS 32

void convert_udp_message(uint8_t type, char *content, char *buffer)
{

    char str[33];
    if (type == 0)
    {
        uint32_t n = ntohl(*(uint32_t *)(content + 1));
        if (content[0] == 0)
        {
            snprintf(str, sizeof(str), "%d", n);
            strcat(buffer, " - INT - ");
            strcat(buffer, str);
        }
        else
        {
            n = n * (-1);
            snprintf(str, sizeof(str), "%d", n);
            strcat(buffer, " - INT - ");
            strcat(buffer, str);
        }
    }
    if (type == 1)
    {
        double n = abs(ntohs(*(uint16_t *)(content)));
        n /= 100;
        strcat(buffer, " - SHORT_REAL - ");
        snprintf(str, sizeof(str), "%.2f", n);
        strcat(buffer, str);
    }
    if (type == 2)
    {
        strcat(buffer, " - FLOAT - ");

        float n = ntohl(*(uint32_t *)(content + 1));
        uint8_t power = content[5];
        while (power)
        {
            n /= 10;
            power--;
        }
        if (content[0] == 0)
        {
            sprintf(str, "%.*lf", content[5], n);
            strcat(buffer, str);
        }
        else
        {
            sprintf(str, "%.*lf", content[5], (n * (-1)));
            strcat(buffer, str);
        }
    }
    if (type == 3)
    {
        strcat(buffer, " - STRING - ");
        strcat(buffer, content);
    }
}

void run_chat_multi_server(int listenfd_tcp, int listenfd_udp)
{

    struct pollfd poll_fds[MAX_CONNECTIONS];
    int num_clients = 3;
    int rc;

    struct chat_packet received_packet;

    // set listening socket
    rc = listen(listenfd_tcp, MAX_CONNECTIONS);
    DIE(rc < 0, "listen");

    // add new file descriptor

    poll_fds[0].fd = listenfd_tcp;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = listenfd_udp;
    poll_fds[1].events = POLLIN;

    poll_fds[2].fd = STDIN_FILENO;
    poll_fds[2].events = POLLIN;

    struct connection_data online_clients[500];
    int clients_len = 0;
    struct topic_data TOPICS[100];
    int topics_len = 0;

    while (1)
    {
        rc = poll(poll_fds, num_clients, -1);
        DIE(rc < 0, "poll");

        for (int i = 0; i < num_clients; i++)
        {
            if (poll_fds[i].revents & POLLIN)
            {

                if (poll_fds[i].fd == listenfd_tcp)
                {
                    // new connection request
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    int newsockfd =
                        accept(listenfd_tcp, (struct sockaddr *)&cli_addr, &cli_len);
                    DIE(newsockfd < 0, "accept");

                    struct chat_packet buffer;
                    memset(buffer.id_client, 0, 100);
                    rc = recv(newsockfd, &buffer, sizeof(struct chat_packet), 0);
                    DIE(rc < 0, "recv id client");

                    int found = 0;
                    for (int c = 0; c < clients_len; c++)
                    {
                        if (strcmp(buffer.id_client, online_clients[c].id_client) == 0)
                        {
                            // if client already connected
                            found = 1;
                            printf("Client %s already connected.\n", buffer.id_client);
                            close(newsockfd);
                        }
                    }
                    if (found == 0)
                    {
                        strcpy(online_clients[clients_len].id_client, buffer.id_client);
                        online_clients[clients_len].socket = poll_fds[i].fd;
                        clients_len++;

                        poll_fds[num_clients].fd = newsockfd;
                        poll_fds[num_clients].events = POLLIN;
                        num_clients++;

                        printf("New client %s connected from %s:%d.\n", buffer.id_client,
                               inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                        break;
                    }
                }
                else if (poll_fds[i].fd == listenfd_udp)
                {

                    struct udp_packet udp_pack;
                    struct sockaddr_in udp_addr;
                    socklen_t udp_len = sizeof(udp_addr);
                    memset(udp_pack.content, 0, sizeof(udp_pack.content));
                    memset(udp_pack.topic, 0, sizeof(udp_pack.topic));
                    int rc = recvfrom(poll_fds[i].fd, &udp_pack,
                                      sizeof(struct udp_packet), 0, (struct sockaddr *)&udp_addr, &udp_len);
                    DIE(rc < 0, "recv udp");
         
                    // check if i have a new topic
                    int found = 0;
                    int tp = -1;
                    for (int t = 0; t < topics_len; t++)
                    {
                        if (strncmp(TOPICS[t].udp_info.topic, udp_pack.topic, strlen(udp_pack.topic) - 1) == 0)
                        {
                            found = 1;
                            tp = t;
                        }
                    }
                    if (found == 0)
                    {
                        // add new topic
                        TOPICS[topics_len].udp_info = udp_pack;
                        TOPICS[topics_len].sin_addr = udp_addr.sin_addr;
                        TOPICS[topics_len].sin_port = udp_addr.sin_port;
                        TOPICS[topics_len].len_clients = 0;
                        // send packet to all clients
                        char udp_buffer[2000];
                        memset(udp_buffer, 0, 2000);
                        strcpy(udp_buffer, inet_ntoa(TOPICS[topics_len].sin_addr));
                        strcat(udp_buffer, ":");
                        char str[20];
                        snprintf(str, sizeof(str), "%d", ntohs(TOPICS[topics_len].sin_port));
                        strcat(udp_buffer, str);
                        strcat(udp_buffer, " - ");
                        strcat(udp_buffer, TOPICS[topics_len].udp_info.topic);
                        // convert from char* to int/float/short_real/string and save topic 
                        convert_udp_message(udp_pack.data_type, udp_pack.content, udp_buffer);
                        strcpy(TOPICS[topics_len].udp_info.content, udp_buffer);
                 
                        topics_len++;
                    }
                    if (tp != -1)
                    {
                        TOPICS[tp].udp_info = udp_pack;
                        // send packet to all clients
                        char udp_buffer[2000];
                        memset(udp_buffer, 0, 2000);
                        strcpy(udp_buffer, inet_ntoa(TOPICS[tp].sin_addr));
                        strcat(udp_buffer, ":");
                        char str[20];
                        snprintf(str, sizeof(str), "%d", ntohs(TOPICS[tp].sin_port));
                        strcat(udp_buffer, str);
                        strcat(udp_buffer, " - ");
                        strcat(udp_buffer, TOPICS[tp].udp_info.topic);
                        // convert from char* to int/float/short_real/stringg
                        convert_udp_message(udp_pack.data_type, udp_pack.content, udp_buffer);
                        strcpy(TOPICS[tp].udp_info.content, udp_buffer);
                        
                        for (int x = 0; x < TOPICS[tp].len_clients; x++)
                        {
                            //send message from topic to all clients
                            struct chat_packet chat;
                            memset(chat.message, 0, MAX_UDP_CONTENT + 500);
                            strcpy(chat.message, udp_buffer);
                            send(TOPICS[tp].clients[x].socket, &chat, sizeof(struct chat_packet), 0);
                        }
                    }
                }
                else if ((poll_fds[i].fd == STDIN_FILENO) & POLLIN)
                {
                    char exit_buff[100];
                    memset(exit_buff, 0, 100);
                    fgets(exit_buff, 100, stdin);
                    if (strcmp(exit_buff, "exit\n") == 0)
                    {
                        for (int s = 0; s < clients_len; s++)
                        {
                            close(online_clients[s].socket);
                        }
                        exit(0);
                    }
                }
                else
                {
                    // got data from one of the clients' sockets
                    struct chat_packet recv_pack;
                    int rc = recv(poll_fds[i].fd, &recv_pack,
                                  sizeof(struct chat_packet), 0);
                    DIE(rc < 0, "recv tcp");

                    char id_client[100];
                    memset(id_client, 0, 100);
                    strcpy(id_client, recv_pack.id_client);
                 
                    // parse package to see the chosen subscription
                    char line[MAX_UDP_CONTENT + 1];
                    strcpy(line, recv_pack.message);
                    char *word;
                    char *d = " ";
                    int sub = 0;
                    int unsub = 0;
                    int cont = 0;
                    char sub_topic[50];
                    char unsub_topic[50];
                    word = strtok(line, d);
                    while (word != NULL)
                    {

                        if (strcmp(word, "subscribe") != 0 && strcmp(word, "unsubscribe") != 0 && cont == 0)
                        {
                            break;
                        }

                        if (strcmp(word, "subscribe") == 0)
                        {
                            sub = 1;
                        }
                        if (strcmp(word, "unsubscribe") == 0)
                        {
                            unsub = 1;
                        }
                        if (cont == 1 && sub == 1)
                        {
                            strcpy(sub_topic, word);
                            memset(recv_pack.message, 0, MSG_MAXSIZE + 1);
                            char text[100] = "Subscribed to topic.";
                
                            memcpy(recv_pack.message, text, strlen(text) + 1);

                            // search for the topic to get the client subscribed
                            for (int t = 0; t < topics_len; t++)
                            {
                                if (strncmp(TOPICS[t].udp_info.topic, sub_topic, strlen(sub_topic) - 1) == 0)
                                {
                                    int x = TOPICS[t].len_clients;
                                    memset(TOPICS[t].clients[x].id_client, 0, 100);
                                    strcpy(TOPICS[t].clients[x].id_client, id_client);
                                    TOPICS[t].clients[x].socket = poll_fds[i].fd;
                                    TOPICS[t].len_clients++;
                                    //send message to the client
                                    send(poll_fds[i].fd, &recv_pack, sizeof(struct chat_packet), 0);
                                    //reset package
                                    memset(recv_pack.message, 0, 2000);
                                    strcpy(recv_pack.message, TOPICS[t].udp_info.content);
  
                                    break;
                                }
                            }
                            break;
                        }
                        if (cont == 1 && unsub == 1)
                        {
                            strcpy(unsub_topic, word);
                            memset(recv_pack.message, 0, MSG_MAXSIZE + 1);
                            char text[100] = "Unsubscribed from topic.";
                          
                            memcpy(recv_pack.message, text, strlen(text) + 1);
                            // search for the topic the client wants an unsubscription
                            for (int t = 0; t < topics_len; t++)
                            {
                                if (strncmp(TOPICS[t].udp_info.topic, unsub_topic, strlen(unsub_topic) - 1) == 0)
                                {
                                    // find client's position
                                    for (int p = 0; p < TOPICS[t].len_clients; p++)
                                    {
                                        if (strcmp(TOPICS[t].clients[p].id_client, recv_pack.id_client) == 0)
                                        {  
                                            // remove client from topic
                                            memcpy(&TOPICS[t].clients[p], &TOPICS[t].clients[p + 1],
                                                   (TOPICS[t].len_clients - p - 1) * sizeof(struct clients_data));
                                            TOPICS[t].len_clients--;
                                        }
                                    }
                                }
                            }
                            //send unsubscription message
                            send(poll_fds[i].fd, &recv_pack, sizeof(struct chat_packet), 0);
                            break;
                        }
                        if (cont >= 3)
                        {
                            break;
                        }
                        cont++;
                        word = strtok(NULL, d);
                    }

                    if (strcmp(recv_pack.message, "exit\n") == 0)
                    {
                        //connexion closed from client
                        printf("Client %s disconnected.\n", id_client);
                        close(poll_fds[i].fd);
                    
                        if (clients_len == 1)
                        {
                            if (strcmp(id_client, online_clients[0].id_client) == 0)
                            {
                                // remove from list
                                memset(online_clients[0].id_client, 0, sizeof(online_clients[0].id_client));
                                online_clients[0].socket = -1;
                                clients_len--;
                            }
                        }
                        else
                        {
                            for (int c = 0; c < clients_len; c++)
                            {
                                if (strcmp(id_client, online_clients[c].id_client) == 0)
                                {
                                    // remove from list
                                    memset(online_clients[c].id_client, 0, 100);
                                    online_clients[c].socket = -1;
                                    memcpy(&online_clients[c], &online_clients[c + 1], (clients_len - c - 1) * sizeof(struct connection_data));
                                    clients_len--;
                                }
                            }
                        }
                        //remove from the list of sockets
                        for (int j = i; j < num_clients - 1; j++)
                        {
                            poll_fds[j] = poll_fds[j + 1];
                        }

                        num_clients--;
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

    //parse port as a number
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // get TCP socket for further connections
    int listenfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    DIE(listenfd_tcp < 0, "socket tcp");

    // UDP socket
    int listenfd_udp = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(listenfd_udp < 0, "socket udp");

    struct sockaddr_in serv_addr_tcp;
    socklen_t socket_len_tcp = sizeof(struct sockaddr_in);

    struct sockaddr_in serv_addr_udp;
    socklen_t socket_len_udp = sizeof(struct sockaddr_in);

    // Facem adresa socket-ului reutilizabila, ca sa nu primim eroare in caz ca
    // disable Nagle's alogithm
    int enable = 1;
    if (setsockopt(listenfd_tcp, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    if (setsockopt(listenfd_udp, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    memset(&serv_addr_tcp, 0, socket_len_tcp);
    serv_addr_tcp.sin_family = AF_INET;
    serv_addr_tcp.sin_port = htons(port);
    serv_addr_tcp.sin_addr.s_addr = INADDR_ANY;

    memset(&serv_addr_udp, 0, socket_len_udp);
    serv_addr_udp.sin_family = AF_INET;
    serv_addr_udp.sin_port = htons(port);
    serv_addr_udp.sin_addr.s_addr = INADDR_ANY;

    rc = bind(listenfd_tcp, (const struct sockaddr *)&serv_addr_tcp, sizeof(serv_addr_tcp));
    DIE(rc < 0, "bind tcp");

    rc = bind(listenfd_udp, (const struct sockaddr *)&serv_addr_udp, sizeof(serv_addr_udp));
    DIE(rc < 0, "bind udp");

    run_chat_multi_server(listenfd_tcp, listenfd_udp);

    //close listening sockets
    close(listenfd_tcp);
    close(listenfd_udp);

    return 0;
}

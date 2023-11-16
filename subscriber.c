#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>

#include "common.h"
#include "helpers.h"

void run_client(int sockfd, char *id_client)
{
  char buf[MSG_MAXSIZE + 1];
  memset(buf, 0, MSG_MAXSIZE + 1);

  struct chat_packet sent_packet;
  struct chat_packet recv_packet;
  struct pollfd fds[2];

  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;

  fds[1].fd = sockfd;
  fds[1].events = POLLIN;

  while (1)
  {
    int rc = poll(fds, 2, -1);
    DIE(rc < 0, "poll fail");

    if (fds[0].revents & POLLIN)
    {
      fgets(buf, sizeof(buf), stdin);

      strcpy(sent_packet.id_client, id_client);
      sent_packet.len = strlen(buf) + 1;
      strcpy(sent_packet.message, buf);
      sent_packet.client_socket = sockfd;

      char line[1000];
      strcpy(line, buf);
      char *word;
      char *d = " ";
      word = strtok(line, d);
      if (strcmp(word, "exit\n") == 0)
      {
        memset(sent_packet.id_client, 0, sizeof(sent_packet.id_client));
        strcpy(sent_packet.id_client, id_client);
        memset(sent_packet.message, 0, 2000);
        strcpy(sent_packet.message, "exit\n");
        //send exit message to server
        rc = send(sockfd, &sent_packet, sizeof(sent_packet), 0);
        DIE(rc < 0, "send exit from client");
        int rec = shutdown(sockfd, SHUT_RDWR);
        DIE(rec < 0, "shutdown client");

        break;
      }

      // Use send function to send the package to the server.
      send(sockfd, &sent_packet, sizeof(sent_packet), 0);
    }

    if (fds[1].revents & POLLIN)
    {
      // Receive a message and show it's content
      rc = recv(sockfd, &recv_packet, sizeof(recv_packet), 0);
      if (rc <= 0)
      {
        break;
      }

      printf("%s\n", recv_packet.message);
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

  // parse port as a number
  uint16_t port;
  int rc = sscanf(argv[3], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // get TCP socket for serve connection
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(sockfd < 0, "socket");

  // disable Nagle's algorithm
  short enable = 1;
  int res = setsockopt(sockfd, 0, TCP_NODELAY, &enable, sizeof(short));
  DIE(res < 0, "setsockopt");

  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
  DIE(rc <= 0, "inet_pton");

  // connect to server
  rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "connect");

  struct chat_packet id;
  memset(id.id_client, 0, 100);
  strcpy(id.id_client, argv[1]);

  rc = send(sockfd, &id, sizeof(struct chat_packet), 0);
  DIE(rc < 0, "rc id client");

  run_client(sockfd, argv[1]);

  // close connection & socket
  close(sockfd);

  return 0;
}
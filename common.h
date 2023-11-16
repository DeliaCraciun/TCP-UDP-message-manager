#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>

/* Dimensiunea maxima a mesajului */
#define MSG_MAXSIZE 1024
#define MAX_UDP_CONTENT 1500

struct udp_packet
{
  char topic[50];
  uint8_t data_type;
  char content[MAX_UDP_CONTENT + 500];
};
struct clients_data
{
  char id_client[100];
  int socket;
};
struct topic_data
{
  struct udp_packet udp_info;
  int len_clients;
  struct clients_data clients[100];
  unsigned short sin_port; // e.g. htons(3490)
  struct in_addr sin_addr;
};
struct connection_data
{
  char id_client[100];
  int online;
  int socket;
};

struct chat_packet
{
  char id_client[100];
  int client_socket;
  uint16_t len;
  char message[MAX_UDP_CONTENT + 500];
};

#endif

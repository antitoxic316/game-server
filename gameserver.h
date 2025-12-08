#ifndef _GAMESERVER_H_
#define _GAMESERVER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>



#define BACKLOG 10	 // how many pending connections queue will hold

#define PLAYERS_PER_SESSION 2

#define PORT "4210"  // the port users will be connecting to

struct obj_data {
  char *name;
  char **interpol_fields;
  int *interpol_values;
  size_t nfields;
};

struct client {
	int tcp_sock;
	int udp_sock;
	struct obj_data **objs;
	int objs_n;
  bool ready;
};

int server_socket_init();

void pfds_add_client(struct pollfd *pfds[], int *size, int ncli, int sockfd, short events);

void pfds_remove_client(struct pollfd *pfds[], int *size, int ncli, int i);

struct client *accept_client(struct pollfd *waiting_clients[], int *queue_size, int ncli, int serv_sock);

void client_handle_packet(struct client*, char *buff, size_t nbuff, int *echo_packet);
#endif
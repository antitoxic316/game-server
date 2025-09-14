#ifndef _TRANSMITTER_H_
#define _TRANSMITTER_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define PORT "3490"  // the port users will be connecting to

struct tm {
  int *tcp_sockfd;
  int *udp_sockfd;
};

void tm_init_udp_socket(int *sockfd, struct addrinfo **listener_addr);
void tm_send_packet_to(int udp_sockfd, struct addrinfo *addr);

#endif
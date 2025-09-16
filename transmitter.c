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

#include "transmitter.h"

int tm_init_udp_socket(){
	int sockfd;
	int broadcast = 1;

	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("socket");
		exit(1);
	}
}

void tm_send_packet_to(int sockfd, struct addrinfo *addr){

}


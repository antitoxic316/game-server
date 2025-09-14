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

struct connection {
	//addrinfo
	//listetn udp
	int recvfrom_sockfd;
	//talk udp
	int sendto_sockfd;
	//register objects tcp
	int obj_h_sockfd;
};


void tm_init_udp_socket(int *sockfd, struct addrinfo **listener_addr){
	struct addrinfo hints, *p;
	int yes=1;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = INADDR_ANY; 

	if ((rv = getaddrinfo("192.168.1.26", PORT, &hints, listener_addr)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	// loop through all the results and bind to the first we can
	for(p = (struct addrinfo*) *listener_addr; p != NULL; p = p->ai_next) {
		if ((*sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		*listener_addr = p;

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}
}


void tm_send_packet_to(int sockfd, struct addrinfo *addr){

}


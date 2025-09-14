#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>

#include "gameserver.h"

#define PLAYERS_PER_SESSION 2

struct obj_data{
	char **field_names;
	void **field_val;
	size_t *field_size;
	int field_n;
};

struct client_info {
	int tcp_sock;
	int udp_sock;
	struct obj_data *objs;
	int objs_n;
};

void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

int main(void)
{
	struct sigaction sa;
	int pid;
	int serv_sock;

	int *waiting_clients;
	int waiting_clients_count = 0;

	serv_sock = server_socket_init();

	for(;;){
		int cl_sock;
		struct sockaddr_storage their_addr; // connector's address information
		socklen_t sin_size;
		
		cl_sock = accept(serv_sock, (struct sockaddr *)&their_addr,
					&sin_size);

		if(cl_sock < 0){
			perror("accept");
			continue;
		}
		waiting_clients = malloc(sizeof(int) * ++waiting_clients_count);
		waiting_clients[waiting_clients_count-1] = cl_sock;

		//recieve how many players needed for session?

		if(waiting_clients_count % PLAYERS_PER_SESSION != 0){
			//serialized more verbose message?
			//how many players are in the queue etc;
			int r = send(cl_sock, "WAITING_FOR_GAME", 16+1, 0);
			continue;
		}

		int r = send(cl_sock, "GAME_STARTED", 16+1, 0);	
		//recv game data and obj data
		//create structs for sockfd and obj that he needs
		//create polls with handlers

		if((pid = fork()) < 0){
			perror("fork");
			exit(1);
		}

		if(pid == 0){ //game sync session
			struct client_info game_players[PLAYERS_PER_SESSION];
			game_players[0].tcp_sock = waiting_clients[-1];
			tm_init_udp_socket()
			game_players[1].tcp_sock = waiting_clients[-2];

		}

	}

	init_sendto_socket(&sockfd, &their_addr);

	printf("sending data...\n");

/*
	sendto(sockfd, "\n\r\r", 3, 0, their_addr->ai_addr, their_addr->ai_addrlen);
	sendto(sockfd, "test\n\n\n", 7, 0, their_addr->ai_addr, their_addr->ai_addrlen);
*/

	close(sockfd);
	exit(0);


	return 0;

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
}

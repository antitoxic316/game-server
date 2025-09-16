#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <poll.h>
#include <stdbool.h>

#include "gameserver.h"

#define PLAYERS_PER_SESSION 2

#define PLAYER_CONNECTION_TIMEOUT 10000 //ms

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

//returns number of clients
int pfds_add_client(struct pollfd *pfds[], int *size, int ncli, int sockfd, short events){
	if(ncli == *size){
		*size *= 2;
		*pfds = realloc(*pfds, sizeof(*pfds) * *size);
	}

	(*pfds)[ncli].fd = sockfd;
	(*pfds)[ncli].events = events;
	(*pfds)[ncli].revents = 0;

	return ++ncli;
}

//returns number of clients
int pfds_remove_client(struct pollfd *pfds[], int *size, int ncli, int i){
	if(*size/2 > ncli && *size > 8){
		*pfds = realloc(*pfds, sizeof(*pfds) * *size/2);
		*size = *size/2;
	}
	
	if(ncli == 1) return 0;

	if(i >= ncli){
		return ncli;
	}

	if(i == ncli-1){
		return --ncli;
	}
	//memory past ncli-1 is unitialized
	while(i < ncli-1){
		// the current one is the one for deletion;
		(*pfds)[i] = (*pfds)[i+1];
		i++;
	}
	return --ncli;
}

int main(void)
{
	struct sigaction sa;
	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
	int pid;
	int serv_sock;

	serv_sock = server_socket_init();


	int queue_size = 8;
	int ncli = 0;
	struct pollfd *waiting_clients = malloc(sizeof(*waiting_clients) * queue_size);

	for(;;){
		int cl_sock;
		struct sockaddr_storage their_addr; // connector's address information
		socklen_t sin_size;
		
		printf("waiting for connections\n");
		cl_sock = accept(serv_sock, (struct sockaddr *)&their_addr, &sin_size);

		if(cl_sock < 0){
			perror("accept");
			continue;
		}
		ncli = pfds_add_client(&waiting_clients, &queue_size, ncli, cl_sock, POLLOUT);
		printf("added connection %d, number of conns: %d\n", cl_sock, ncli);

		//recieve how many players needed for session?

		if(ncli % PLAYERS_PER_SESSION != 0){
			int poll_n = poll(waiting_clients, ncli, -1);

			if(poll_n == -1){
				perror("poll");
				exit(1);
			}

			for(int i = 0; i < ncli; i++){
				int r = send(waiting_clients[i].fd, "WAITING_FOR_GAME\r", 17+1, 0);
			}
			continue;
		}

		struct pollfd *pfds = malloc(sizeof(*pfds) * PLAYERS_PER_SESSION);
		int players_n = PLAYERS_PER_SESSION;
		int game_clients = 2;
		for(int i = 0; i < players_n; i++){
			//always zero because remove_client function shifts all members
			pfds[i] = waiting_clients[0];
			ncli = pfds_remove_client(&waiting_clients, &queue_size, ncli, 0);
		}

		while(true){ // GAME SYNC LOOP
			int events_n = poll(pfds, players_n, PLAYER_CONNECTION_TIMEOUT);

			if(events_n == -1){
				perror("poll");
				exit(1);
			}

			if(events_n < players_n){
				//reset queue
			}

			for(int i = 0; i < players_n; i++){
				if(pfds[i].revents & POLLOUT){
					int r = send(pfds[i].fd, "GAME_STARTED\r", 13+1, 0);	
				}
			}
			break;
		}

		sleep(10);

		//recv game data and obj data
		//create structs for sockfd and obj that he needs
		//create polls with handlers

		/*
		if((pid = fork()) < 0){
			perror("fork");
			exit(1);
		}

		if(pid == 0){ //game sync session
			while(true){

			}
		}
		*/
		break;
	}

	close(serv_sock);
	return 0;
}

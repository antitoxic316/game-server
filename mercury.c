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

#define PLAYER_CONNECTION_TIMEOUT 10000 //ms

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
	struct pollfd *queue_pfds = malloc(sizeof(*queue_pfds) * queue_size);
	bool bad_session = false;

	int ncli = 0;
	struct client **clients = malloc(sizeof(struct client*) * PLAYERS_PER_SESSION);

	for(;;){
		do {
			struct client *cli = accept_client(&queue_pfds, &queue_size, ncli, serv_sock);
			clients[ncli++] = cli;
		} while (PLAYERS_PER_SESSION - ncli);

		for(int i = 0; i < PLAYERS_PER_SESSION; i++){
			queue_pfds[i].events = POLLOUT | POLLHUP;
		}

		while(true){ // QUEUE LOOP
			int events_n = poll(queue_pfds, PLAYERS_PER_SESSION, PLAYER_CONNECTION_TIMEOUT);

			if(events_n == -1){
				perror("poll");
				exit(1);
			}

			if(events_n < PLAYERS_PER_SESSION){
				continue;
			}

			for(int i = 0; i < PLAYERS_PER_SESSION; i++){
				if(queue_pfds[i].revents & POLLHUP){ 
					bad_session = true;
					break;
				}
				if(queue_pfds[i].revents & POLLOUT){
					int r = send(queue_pfds[i].fd, "INIT_STARTED\r", 13+1, 0);	
				}
			}
			break;
		} // end QUEUE LOOP

		if(bad_session){
			for(int i = 0; i < PLAYERS_PER_SESSION; i++){
				if(queue_pfds[i].revents & POLLHUP){
					continue;
				}
				int r = send(queue_pfds[i].fd, "SESSION_CANCELED\r", 17+1, 0);	
			}
			continue; // restart the session
		}

		// FORK HERE IN THEORY


		for(int i = 0; i < PLAYERS_PER_SESSION; i++){
			queue_pfds[i].events = POLLIN | POLLHUP;
		}

		bool initialized = false;
		while(!initialized){ // GAME SESSION INIT LOOP
			int events_n = poll(queue_pfds, PLAYERS_PER_SESSION,100);
		
			if(events_n == -1){
				perror("poll");
				exit(1);
			}

			for(int i = 0; i < PLAYERS_PER_SESSION; i++){
				if(queue_pfds[i].revents & POLLHUP){
					printf("client broke the connection\n");
					exit(-1);
				}
				if(queue_pfds[i].revents & POLLIN){
					char buff[256] = {'\0',};
					int r = recv(queue_pfds[i].fd, buff, 256-1, 0);	

					struct client* cli = NULL;
					for(int i = 0; i < ncli; i++){
						if(clients[i]->tcp_sock == queue_pfds[i].fd){
							cli = clients[i];
							break;
						}
					}

					if(!cli){
						printf("client not found");
						exit(-1);
					}

					printf("%s\n", buff);

					if(!strcmp(buff, "READY")){
						cli->ready = true;
						continue;
					}

					int echo_packet = 0;
					client_handle_packet(cli, buff, 256-1, &echo_packet);
					
					if(!echo_packet){
						continue;
					}

					for(int j = 0; j < PLAYERS_PER_SESSION; j++){
						if(j == i){
							continue;
						}
						send(queue_pfds[j].fd, buff, r, 0);
					}
				}
			}

			initialized = true;
			for(int i = 0; i < ncli; i++){
				if(!clients[i]->ready){
					initialized = false;
					break;
				}
			}
		} // end GAME SESSION INIT LOOP
		
		//init upd socket

		while(true){ // GAME SESSION LOOP
			break;
		} // end GAME SESSION LOOP
		break;
	}

	close(serv_sock);
	return 0;
}
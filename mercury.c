#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <poll.h>
#include <stdbool.h>
#include <time.h>
#include <fcntl.h>

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
	int serv_sock;
	serv_sock = server_socket_init();

	int tcp_pfds_size = 8;
	struct pollfd *tcp_pfds = malloc(sizeof(*tcp_pfds) * tcp_pfds_size);


	struct pollfd *session_pfds = NULL;

	for(;;){
		int ncli = 0;
		struct client **clients = malloc(sizeof(struct client*) * PLAYERS_PER_SESSION);
		do {
			struct client *cli = accept_client(&tcp_pfds, &tcp_pfds_size, ncli, serv_sock);
			clients[ncli++] = cli;
		} while (PLAYERS_PER_SESSION - ncli); // queue loop

		for(int i = 0; i < PLAYERS_PER_SESSION; i++){
			tcp_pfds[i].events = POLLOUT | POLLHUP;
		}

		while(true){ // QUEUE LOOP
			int events_n = poll(tcp_pfds, PLAYERS_PER_SESSION, PLAYER_CONNECTION_TIMEOUT);

			if(events_n == -1){
				perror("poll");
				goto SESSION_CANCELATION;
			}

			if(events_n < PLAYERS_PER_SESSION){
				continue;
			}

			for(int i = 0; i < PLAYERS_PER_SESSION; i++){
				if(tcp_pfds[i].revents & POLLHUP){ 
					goto SESSION_CANCELATION;
				}
				if(tcp_pfds[i].revents & POLLOUT){
					int r = send(tcp_pfds[i].fd, "INIT_STARTED\r", 13+1, 0);	
					if(r == -1){
						perror("send");
						goto SESSION_CANCELATION;
					}
				}
			}
			break;
		} // end QUEUE LOOP

		for(int i = 0; i < PLAYERS_PER_SESSION; i++){
			tcp_pfds[i].events = POLLIN | POLLHUP;
		}

		bool initialized = false;
		while(!initialized){ // GAME SESSION INIT LOOP
			int events_n = poll(tcp_pfds, PLAYERS_PER_SESSION,100);
		
			if(events_n == -1){
				perror("poll");
				goto SESSION_CANCELATION;
			}

			for(int i = 0; i < PLAYERS_PER_SESSION; i++){
				if(tcp_pfds[i].revents & POLLHUP){
					printf("client broke the connection\n");
					goto SESSION_CANCELATION;
				}
				if(tcp_pfds[i].revents & POLLIN){
					char buff[256] = {'\0',};
					int r = recv(tcp_pfds[i].fd, buff, 256-1, 0);	
					if(r == 0){
						perror("recv");
						goto SESSION_CANCELATION;
					}

					struct client* cli = NULL;
					for(int j = 0; j < ncli; j++){
						if(clients[j]->tcp_sock == tcp_pfds[i].fd){
							cli = clients[j];
							break;
						}
					}

					if(!cli){
						printf("client not found\n");
						goto SESSION_CANCELATION;
					}

					printf("tcp: %s\n", buff);
					for (int i = 0; i < 256; i++)
    				printf("%02X ", buff[i]);
					printf("\n");
					if(strstr(buff, "READY")){
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
						send(tcp_pfds[j].fd, buff, r, 0);
						if(r == -1){
							perror("send");
							goto SESSION_CANCELATION;
						}
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

		// session tcp pfds init
		session_pfds = malloc(sizeof(*session_pfds) * ncli);

		int udp_sockfd;

		struct pollfd *session_pfds = malloc(sizeof(*session_pfds) * ncli);
		client_init_udp_socket(&udp_sockfd);
		if(udp_sockfd <= 0){
			printf("failed to initialize udp socket\n");
			goto SESSION_CANCELATION;
		}
		for(int i = 0; i < ncli; i++){ // UDP address check
			clients[i]->udp_sock = udp_sockfd;
			session_pfds[i].fd = udp_sockfd;
			session_pfds[i].events = POLLIN;
		
			char buff[8] = {'\0',};
			char s[INET6_ADDRSTRLEN];
			struct sockaddr_storage tmp_addr;
			socklen_t tmp_addr_len = sizeof tmp_addr;

			bool is_unique = false;
			do {
				printf("checking for client\n");
				recvfrom(session_pfds[i].fd, buff, 7, 0, 
								 (struct sockaddr*) &tmp_addr, 
								 &tmp_addr_len);	
				
				is_unique = true;
				for(int j = 0; j < ncli; j++){
					if(!memcmp(&clients[j]->addr, &tmp_addr, sizeof(tmp_addr))){
						is_unique = false;
						break;
					}
				}
			} while(!is_unique);
			clients[i]->addr = tmp_addr;
			clients[i]->addr_len = tmp_addr_len;

			printf("listener: got %s from %s\n", buff,
					inet_ntop(clients[i]->addr.ss_family,
						get_in_addr((struct sockaddr *)&(clients[i]->addr)),
							s, sizeof s));
		} // end UDP address check
		

		for(int i = 0; i < ncli; i++){
			int r = send(clients[i]->tcp_sock, "GAME_STARTED\r\n", 13, 0);
			if(r == -1 || r == 0){
				perror("send");
				goto SESSION_CANCELATION;
			}
		}

		//TODO
		//setting tcp sockets to non blocking mode
		//later try to implement reading from them on other thread instead
		for(int i = 0; i < ncli; i++){
			int flags = fcntl(clients[i]->tcp_sock, F_GETFL, 0);
			int rt = fcntl(clients[i]->tcp_sock, F_SETFL, flags|O_NONBLOCK);
			if(rt == -1){
				perror("fcntl");
				exit(-1);
			}
		}

		while(true){ // GAME SESSION LOOP
			int events_n = poll(session_pfds, ncli, 100);
		
			if(events_n == -1){
				perror("poll");
				goto SESSION_CANCELATION;
			}

			for(int i = 0; i < PLAYERS_PER_SESSION; i++){ // UDP polling
				if(session_pfds[i].revents & POLLIN){
					char buff[256] = {'\0',};

					struct sockaddr_storage cli_addr;
					socklen_t addr_len = sizeof(cli_addr);

					int r = recvfrom(session_pfds[i].fd, buff, 256-1, 0, 
													 (struct sockaddr*) &cli_addr, &addr_len);	
					if(r == -1){
						perror("recvfrom");
						goto SESSION_CANCELATION;
					}

					int curr_client_i = -1;
					struct client* cli = NULL;
					for(int j = 0; j < ncli; j++){
						if(!memcmp(&clients[j]->addr, &cli_addr, sizeof(cli_addr))){
							cli = clients[j];
							curr_client_i = j;
							break;
						}
					}
					if(!cli){
						printf("client not found\n");
						continue;
					}

					if(cli->udp_packet_time != 0){
						if(time(NULL) - cli->udp_packet_time > 3){
							cli->timed_out = true;
							printf("client got timed out\n");
							goto SESSION_CANCELATION;
						} else {
							cli->udp_packet_time = time(NULL);
						}
					} else {
						cli->udp_packet_time = time(NULL);
					}

					printf("udp: %s\n", buff);

					int echo_packet = 0;
					client_handle_packet(cli, buff, 256-1, &echo_packet);
					
					if(!echo_packet){
						continue;
					}

					for(int j = 0; j < PLAYERS_PER_SESSION; j++){
						if(j == curr_client_i){
							continue;
						}
						r = sendto(session_pfds[j].fd, buff, r, 0,
									 (struct sockaddr *)&clients[j]->addr, clients[j]->addr_len);
						if(r == -1){
							perror("sendto");
							goto SESSION_CANCELATION;
						}
					}
				}
			} // end UDP polling 

			for(int i = 0; i < ncli; i++){ // TCP polling
				if(tcp_pfds[i].revents & POLLHUP){
					printf("client broke the connection\n");
					goto SESSION_CANCELATION;
				}
				if(tcp_pfds[i].revents & POLLIN){
					char buff[256] = {'\0',};
					int r = recv(tcp_pfds[i].fd, buff, 256-1, 0);	
					if(r <= 0){
						if(errno != EAGAIN && errno != EWOULDBLOCK){
							perror("recvfrom");
							goto SESSION_CANCELATION;
						}
					}

					struct client* cli = NULL;
					for(int j = 0; j < ncli; j++){
						if(clients[j]->tcp_sock == tcp_pfds[i].fd){
							cli = clients[j];
							break;
						}
					}

					if(!cli){
						printf("client not found\n");
						goto SESSION_CANCELATION;
					}

					printf("tcp: %s; r: %d\n", buff, r);
					for (int i = 0; i < 256; i++)
    				printf("%02X ", buff[i]);
					printf("\n");
					int echo_packet = 0;
					client_handle_packet(cli, buff, 256-1, &echo_packet);
					
					if(!echo_packet){
						continue;
					}

					for(int j = 0; j < ncli; j++){
						if(j == i){
							continue;
						}
						send(tcp_pfds[j].fd, buff, r, 0);
						if(r == -1){
							perror("send");
							goto SESSION_CANCELATION;
						}
					}
				}
			} // end TCP polling
		} // end GAME SESSION LOOP

SESSION_CANCELATION: //restart the session
		if(session_pfds) free(session_pfds);

		for(int i = 0; i < ncli; i++){
			if(clients[i]->timed_out){
				continue;
			}
			send(tcp_pfds[i].fd, "SESSION_CANCELED\r", 17+1, 0);	
		}
	
		close(clients[0]->udp_sock);
		for(int i = 0; i < ncli; i++){
			close(clients[i]->tcp_sock);
			free(clients[i]);
		}
		free(clients);
	}

	close(serv_sock);	
	return 0;
}
#ifndef _GAMESERVER_H_
#define _GAMESERVER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "transmitter.h"

#define BACKLOG 10	 // how many pending connections queue will hold

int server_socket_init();

#endif
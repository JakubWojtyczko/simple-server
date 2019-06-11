#ifndef __SERVER_H
#define __SERVER_H

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "messages.h"

#define ADDRESS "127.0.0.1"
#define PORT "50000"

#define MAX_CLIENTS_NUMBER 30

struct client_info {
	struct sockaddr_in address;
	long client_id;
	int addrlen;
};

int prepare_socket();

void * send_message(void * socket_fd);

void add_new_client(struct sockaddr * address, int addrlen, long cliet);

void make_decision(struct message msg, int fd, int queue);

#endif // __SERVER_H

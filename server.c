/*
 * Simple server
 * Jakub Wojtyczko 2019
 */

#include "server.h"
#include "messages.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/msg.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h> 
#include <errno.h>
#include <pthread.h>

// store informations about all connected clients
struct client_info clients[MAX_CLIENTS_NUMBER];
int thread_counter;

// to lock memory shared by threads
pthread_mutex_t locker;

int main() {
	/*
	 * Server IP - 172.0.0.1:50000
	 * Client IP - 172.0.0.1:500xx (where x = client id)
	 */
	int socket_id = prepare_socket();
	if (socket_id < 0) {
		fprintf(stderr, "Cannot prepare socket\n");
		exit(-1);
	}
	// Depend on it that client with ID=0 is unassigned
	// it should be 0, but double protection
	bzero(clients, sizeof(clients));

	struct message msg;
	// threads send messages to clients.
	// if one of them is busy then the next thread 
	// will take over the task
	pthread_t thr_id[4];
	for (int i=0; i<4; ++i) {
		pthread_create(thr_id + i, NULL, send_message, (void *)&socket_id);
	}
	// mutex to avoid reading the same resources
	if (pthread_mutex_init(&locker, NULL) != 0) {
		fprintf(stderr, "Mutex init failed\n");
		exit(-2);
	}
	// shared memory for requests queueing because all 
	// threads are using one memory queue
	int queue = msgget(1, IPC_CREAT | 0666);
	if (queue < 0) {
		fprintf(stderr, "Cannot create memory queue\n");
		exit(-1);
	} else {
		fprintf(stdout, "Memory queue created\n");
	}

	while (1) {
		// store information about the last request owner 
		struct sockaddr client_i;
		unsigned client_addrlen = sizeof(client_i);

		// it is important to clear that message - it can
		// be an initial message
		bzero(&msg, sizeof(msg));

		// receive data here. We have to decide if that message is an
		// 'initial req' or a message for to another client.
		int status = recvfrom(socket_id, &msg, sizeof(msg), MSG_WAITALL, 
				&client_i, &client_addrlen);
		if (status < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
		}
		
		// if that message has no content, it means that it's a new
		// client and he must be added 
		if(*msg.content == 0) {
			add_new_client(&client_i, client_addrlen, msg.type);
			// we don't need to "forward" an 
			// initial user request
			continue;
		}
		
		// 'forward' message to another client
		make_decision(msg, socket_id, queue);
		
	}
	close(socket_id);
}

void make_decision(struct message msg, int fd, int queue) {
	// add to queue and continue the loop
	// because we can have a next request 
	msgsnd(queue, &msg, sizeof(msg), 0);		
}

int prepare_socket() {
	// struct storing connection inforations
	struct addrinfo addr_setup;
	memset(&addr_setup, 0, sizeof(addr_setup));
	addr_setup.ai_flags = AI_PASSIVE;
	addr_setup.ai_family = AF_INET;
	addr_setup.ai_socktype = SOCK_DGRAM;

	// for informations about the server
	struct addrinfo * server_info;

	// localhost, port 50000
	int status = getaddrinfo(0, PORT, &addr_setup, &server_info);
	if (status != 0) {
		fprintf(stderr, "Cannot get info about IP address\n");
		return -1;
	}
	
	// server socket
	int socket_fd = socket(server_info->ai_family, 
			       server_info->ai_socktype,
			       server_info->ai_protocol
			);
	if (socket_fd < 0) {
		fprintf(stderr, "Socket cannot be created\n");
		return -2;
	}
	fprintf(stdout, "Opening socket...\n");
	// bind it
	if (bind(socket_fd, server_info->ai_addr, server_info->ai_addrlen) == -1) {
		fprintf(stderr, "Binding...\n");
		return -3;
	}
	fprintf(stdout, "Done!\n");
	return socket_fd;
}

/*
 * This function must work at an another thread.
 */
void * send_message(void * fd) {
	// request messages are stored here
	int queue = msgget(1, 0);
	struct message msg;
	// thread id
	int thread_id = ++thread_counter;
	while (1) {
		pthread_mutex_lock(&locker);
			// read from shared memory queue
			// must be temprarily locked
			// only one thread can read the data now
			int status = msgrcv(queue, &msg, 
					    sizeof(struct message), 0, 0
					);
		pthread_mutex_unlock(&locker);

		if (status < 0) {
			// There is no message in queue yet.
			continue;
		}
		
		// addresser id 
		int client_no = ntohl(msg.type);
		if (client_no == 0)
			// client id can't be 0 
			continue;
		for(int i=0; i<MAX_CLIENTS_NUMBER; ++i) {
			if (client_no != ntohl(clients[i].client_id)) {
				// we are looking for an another
				// client 
				continue;
			}
			// log destination
			fprintf(stdout, "Message --> %d PORT: %d\n", 
				ntohl(clients[i].client_id), 
				ntohs(clients[i].address.sin_port));
			fprintf(stdout, "Handled by thread %d\n\n", thread_id);

			// send to the suitable client
			status = sendto(*((int *)fd), &msg, sizeof(msg), 0, 
				(struct sockaddr *)&((clients+i)->address),
				clients[i].addrlen);
			if (status < 0) {
				fprintf(stderr, "%s\n", strerror(errno));
			}
		}
	}	
}


void add_new_client(struct sockaddr * address, int addrlen, long client) {

	for (int i=0; i < MAX_CLIENTS_NUMBER; ++i) {
		if ((clients + i)->client_id == client) {
			// this client is already connected
			return;
		}
		
		if ((clients + i)->addrlen == 0) {
			memcpy(&((clients + i)->address), address, addrlen);
			(clients + i)->address.sin_port = htons(atoi(PORT) + ntohl(client));
			(clients + i)->addrlen = addrlen;
			(clients + i)->client_id = client;
			fprintf(stdout, "New client connected. (id=%d) on port: %d\n", 
					ntohl(client), ntohs(clients[i].address.sin_port));
			return;
		}
	}
	// Max clients number has been exeeded
	fprintf(stdout, "Unable to add a new client\n");
}


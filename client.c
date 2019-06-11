/*
 * Simple client using UDP
 * Jakub Wojtyczko 2019
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>

#include "server.h"
#include "messages.h"

int clio;

void prepare_socket_c(int id, int * socket_c, int * socket_s, 
		struct addrinfo ** s, struct addrinfo ** c);


// it must work at an another thread
void * read(void * serv) {
	struct sockaddr * serv_addr = (struct sockaddr *) serv;
	unsigned len = sizeof(*serv_addr);

	struct message msg;
	while (1) {
		// we don't need to filter messages because this
		// port belongs to that client.
		int bytes = recvfrom(clio, &msg, sizeof(msg), MSG_WAITALL, 
				serv_addr, &len); 
		if (bytes < 0) {
			// something went wrong but don't care
		} else {
			// display received message
			printf("%s\n", msg.content);
		}
	}
}

int main(int argc, char * argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Your id is required\n");
		exit(-1);
	} else if (atoi(argv[1]) < 1) {
		fprintf(stderr, "Your id must be > 0\n");
		exit(-2);
	}
	// client unique id
	int id = atoi(argv[1]);
	// file descriptors for server and client
	int cli, serv;
	// info about server and client addresses
	struct addrinfo * s_info, * c_info;

	prepare_socket_c(id, &cli, &serv, &s_info, &c_info);

	struct message msg;
	
	struct sockaddr serv_addr;
	msg.type = htonl(id);
	msg.content[0] = '\0';
	// first message must have no content. then the server
	// will save information about that client.
	int status = sendto(serv, &msg, sizeof(msg), 0, 
				     (struct sockaddr *) s_info->ai_addr, 
				     s_info->ai_addrlen);
	if (status < 0) {
		printf("*%s\n", strerror(errno));
		fprintf(stderr, "=> Unable to send an initial data\n");
		exit(-3);
	}
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if (setsockopt(serv, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		fprintf(stderr, "Server unavailable\nAborting...\n");
		exit(-1);
	}

	// thread which listen to responses in the background
	pthread_t thread_id;
	clio = cli;
	pthread_create(&thread_id, NULL, read, (void *)&serv_addr);
	fprintf(stdout, "Press [id] [message] to send your message\n");
	while (1) {
		// user writes destination id and message to send here
		scanf("%d", &msg.type);
		msg.type = htonl(msg.type);
		char * line = NULL;
		size_t size = 256;
		// read message from stdin
		getline(&line, &size, stdin);
		size = 0;
		// terminate line by \0 when it is longer that max
		// massage length
		for ( ; line[size] != '\n' && size < 256; ++size) {
			msg.content[size] = line[size];
		}
		msg.content[size] = '\0';
		free(line);
		// flush rest input data from stdin
		fflush(stdin);
		// send to the server inserted by user data 
		int status = sendto(serv, &msg, sizeof(msg), 0,
				     (struct sockaddr *) s_info->ai_addr, 
				     s_info->ai_addrlen);
		if (status < 0) {
			printf("=> %s\n", strerror(errno));
		}
	}
}


void prepare_socket_c(int id, int * socket_c, int * socket_s,
	struct addrinfo ** servinfo, struct addrinfo ** client_info) {

	// information about address
	struct addrinfo addr_setup;
	memset(&addr_setup, 0, sizeof(addr_setup));
	addr_setup.ai_flags = AI_PASSIVE;
	addr_setup.ai_family = AF_INET;
	addr_setup.ai_socktype = SOCK_DGRAM;
	
	// server info (172.0.0.1:50000)
	int status = getaddrinfo("127.0.0.1", PORT, &addr_setup, servinfo);
	if (status < 0) {
		fprintf(stderr, "Cannoct create server socket\n");
	}
	// client info (127.0.0.1:[50000 + id])
	char port[10];
	sprintf(port, "%d", atoi(PORT) + id);
	status = getaddrinfo("127.0.0.1", port, &addr_setup, client_info);

	// server socket
	int server_socket = socket(
			(*servinfo)->ai_family, 
			(*servinfo)->ai_socktype, 
			(*servinfo)->ai_protocol
		);

	// client socket
	int client_socket = socket(
			(*client_info)->ai_family,
			(*client_info)->ai_socktype,
 			(*client_info)->ai_protocol
		);
	fprintf(stdout, "The socket is open\n");

	// Bind it
	status = bind(client_socket, (*client_info)->ai_addr, (*client_info)->ai_addrlen);
	if (status < 0) {
		fprintf(stderr, "Binding error occurred.\n");
		return;
	}
	*socket_c = client_socket;
	*socket_s = server_socket;
	struct timeval r;
	r.tv_sec = 1;
	r.tv_usec = 0;
	setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &r, sizeof(struct timeval));
	
				
}


all: server client

server: server.c
	gcc server.c -o server -lpthread -g -Wall

client: client.c
	gcc client.c -o client -lpthread -g -Wall


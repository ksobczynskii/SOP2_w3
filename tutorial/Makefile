CC=gcc
CFLAGS= -std=gnu99 -Wall

server: server.c
	${CC} ${CFLAGS} server.c -o server

local_client: local_client.c
	${CC} ${CFLAGS} local_client.c -o local_client

tcp_client: tcp_client.c
	${CC} ${CFLAGS} tcp_client.c -o tcp_client

client: client.c
	${CC} ${CFLAGS} client.c -o client
clean:
	rm -rf tcp_client local_client server client

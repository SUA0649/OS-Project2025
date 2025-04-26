CC=gcc
CFLAGS=-Wall -pthread
LIBS=-lncurses

all: server client client_p2p

server: server.c server.h Utils/cJSON.c
	$(CC) $(CFLAGS) -o server server.c Utils/cJSON.c

client: client.c client.h Utils/cJSON.c
	$(CC) $(CFLAGS) -o client client.c Utils/cJSON.c $(LIBS)

client_p2p: client_p2p.c client.h Utils/cJSON.c
	$(CC) $(CFLAGS) -o client_p2p client_p2p.c Utils/cJSON.c $(LIBS)

clean:
	rm -f server client client_p2p *.o
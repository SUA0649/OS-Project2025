CFLAGS= -Wall

all: server client client_p2p

server: server.c server.h
	gcc server.c -o server.exe -lpthread $(CFLAGS)

client: client.c client.h Utils/cJSON.c client_p2p
	gcc client.c Utils/cJSON.c -o client.exe -lncurses -lpthread $(CFLAGS)

client_p2p: client_p2p.c client.h Utils/cJSON.c
	gcc client_p2p.c Utils/cJSON.c -o client_p2p.exe -lncurses -lpthread $(CFLAGS)

clean:
	rm -f server.exe client.exe client_p2p.exe
	rm -f Logs/*.json
all: server client

server: server.c
	gcc server.c -o server -lpthread

client: client.c
	gcc client.c -o client -lncurses -lpthread

clean:
	rm -f server client
all: server client

server: server.c
	gcc server.c -o server.exe -lpthread

client: client.c
	gcc client.c -o client.exe -lncurses -lpthread

clean:
	rm -f server.exe client.exe
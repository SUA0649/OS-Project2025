all: server client

server: server.c server.h
	gcc server.c -o server.exe -lpthread

client: client.c client.h Utils/cJSON.c
	gcc client.c Utils/cJSON.c -o client.exe -lncurses -lpthread

clean:
	rm -f server.exe client.exe
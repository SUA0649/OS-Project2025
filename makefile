all: server client client_p2p

server: server.c server.h
	gcc server.c -o server.exe -lpthread

client: client.c client.h
	gcc client.c -o client.exe -lncurses -lpthread

client_p2p: client_p2p.c client.h
	gcc client_p2p.c -o client_p2p.exe -lncurses -lpthread

clean:
	rm -f server.exe client.exe
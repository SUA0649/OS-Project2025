server: server.c ui.o chat.o
	gcc server.c ui.o chat.o -o server -lncurses

client: client.o ui.o chat.o
	gcc client.o ui.o chat.o -o client -lncurses

client.o: client.c ui.h
	gcc -c client.c -o client.o -lncurses

chat.o: chat.c chat.h ui.h
	gcc -c chat.c -o chat.o -lncurses

ui.o: ui.c ui.h
	gcc -c ui.c -o ui.o -lncurses

clean:
	rm -f server client *.o
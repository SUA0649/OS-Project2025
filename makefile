server:
	gcc server.c -o server.exe
	./server.exe

client:
	gcc client.c -o client.exe
	./client.exe

clean:
	rm -f client.exe server.exe
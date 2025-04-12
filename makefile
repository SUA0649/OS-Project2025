
all: server
	gcc Backend_side/server.c -lncurses -lpthread -o main
	./main


server: Backend_side/server.c
	@echo "Server file exists."


gtk:
	gcc `pkg-config --cflags gtk4` More_test/gtk_test.c -o gtk_test `pkg-config --libs gtk4`
	GSK_RENDERER=cairo LIBGL_ALWAYS_SOFTWARE=1 GDK_DEBUG=gl-disable ./gtk_test

clean:
	rm -f main
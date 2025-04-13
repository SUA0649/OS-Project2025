#include"chat.h"

#define PORT 8080
#define BUFFER_SIZE 1024
#define LOCAL_ADDRESS "127.0.0.1"

int main(){

	int server_sock, valread;
	struct sockaddr_in serv_addr;
	char buffer[BUFFER_SIZE];
	const char *message = "CLIENT CONNECTED\n";	

	if((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0){
		perror("SOCKET FAILED");
		exit(EXIT_FAILURE);
	}
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	if(inet_pton(AF_INET, LOCAL_ADDRESS, &serv_addr.sin_addr)<=0){
		perror("INVALID ADDRESS");
		exit(EXIT_FAILURE);
	}

	printf("CONNECTING TO %s PORT %d\n", LOCAL_ADDRESS, PORT);
	if(connect(server_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0){
		perror("CONNECTION FAILED");
		exit(EXIT_FAILURE);
	}
	init_ui();
	

	if(send_uname(server_sock)<0){
		exit_ui();
		printf("Failed to send username\n");
		return 0;
	}
	chat_loop(server_sock);
	exit_ui();
    	return 0;

}
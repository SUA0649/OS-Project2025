#include"chat.h"


void* connection_handler(void* socket);
void transmitt(int socket, int index);
int recv_uname(int socket);

#define PORT 8080

int n = 0;
int clients[MAX_CLIENTS];
char usernames[MAX_CLIENTS][MAX_UNAME];
/*testing*/

struct sockaddr_in address;
int addrlen = sizeof(address);
pthread_mutex_t lock;

int main(){

	int server_sock;
	int opt = 1;
	pthread_t pool[MAX_THREADS];
	

	if((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0){
		perror("SOCKET FAILED");
		exit(EXIT_FAILURE);
	}
	
	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt, sizeof(opt))) { 
        	perror("setsockopt"); 
        	exit(EXIT_FAILURE); 
         } 
	
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);
	
	if(bind(server_sock, (struct sockaddr *)&address, sizeof(address))<0){
		perror("BIND FAILED");
		exit(EXIT_FAILURE);
	}
	
	if(listen(server_sock, 3) < 0){
		perror("LISTEN FAILED");
		exit(EXIT_FAILURE);
	}
	
	printf("LISTENING ON PORT %d\n", PORT);

	pthread_mutex_init(&lock, NULL);

	for(int i=0;i<MAX_THREADS;i++){
	 	pthread_create(&pool[i], NULL, connection_handler, &server_sock);
	}

	printf("Server has been closed\n");

	for(int i =0;i<MAX_CLIENTS;i++){
		pthread_join(pool[i], 0);	
	}
	return 0;
}

void* connection_handler(void* socket){
	int server_sock = *((int *)socket);
	int client;
	int index;
	while(1){	
		printf("THREAD: %lx WAITING FOR CONNECTIONS\n", pthread_self());
		
		client = accept(server_sock, (struct sockaddr*)&address, (socklen_t*)&addrlen);
		
		pthread_mutex_lock(&lock);
		index = find_free(clients, MAX_CLIENTS);
		
		if(index < 0){
			printf("Client limit reached\n");
			pthread_mutex_unlock(&lock);
		}
		
		else{
			clients[index] = client;
			pthread_mutex_unlock(&lock);
			transmitt(client, index);
		}
	}
}


void transmitt(int socket, int index){
	printf("CONNECTION TO THREAD %lx\n", pthread_self());
	char tmp[MAX];
	char bye[MAX];
	//char uname[MAX_UNAME];
	char buffer[MAX+MAX_UNAME+1];
	int uname_index = 0;
	int temp;
	sprintf(bye, "%d", BYE_MESSAGE);

	if ((uname_index = recv_uname(socket)) < 0){
		printf("Failed to receive username. Disconnecting...\n");
		clients[index] = 0;
		if (uname_index >= 0 && uname_index < MAX_CLIENTS) {
            memset(usernames[uname_index], 0, MAX_UNAME);
        }
		return;
	}

	while(1){
		bzero(tmp, sizeof(tmp));
		
		bzero(buffer, sizeof(buffer));
		
		if(recv(socket, tmp, sizeof(tmp), 0)<0){
			printf("Failed to receive message\n");			
			snprintf(buffer, sizeof(buffer), "Error: Failed tp receive message");
			if(send(socket, buffer, sizeof(buffer), 0)<0){
				printf("Failed to send error message. Disconnecting...\n");
				clients[index]=0;
				bzero(usernames[uname_index], sizeof(usernames[uname_index]));
				return;
			}
		}

		if(strncmp(bye, tmp, MAX) == 0){
			printf("THREAD: %lx DISCONNECTED\n", pthread_self());
			snprintf(buffer, sizeof(buffer), "SERVER MESSAGE: %s HAS DISCONNECTED", usernames[uname_index]);
			
			//Sending the above message to everyone
			for(int i =0;i<MAX_CLIENTS;i++){
				if(clients[i] != 0){
					if(send(clients[i], buffer, sizeof(buffer), 0)<0){
						printf("Failed to send message to %d", clients[i]);
					}
				}
			}

			clients[index] = 0;
			bzero(usernames[uname_index], sizeof(usernames[uname_index]));
			return;
		}

		snprintf(buffer, sizeof(buffer), "%s: %s", usernames[uname_index], tmp);
		
		for(int i =0;i<MAX_CLIENTS;i++){
			if(clients[i] != 0){
				if(send(clients[i], buffer, sizeof(buffer), 0)<0){
					printf("Failed to send message to %d", clients[i]);
				}
			}
		}
		printf("%s\n", buffer);
	}
}

int recv_uname(int socket){
	char uname[MAX_UNAME];
	memset(uname, 0, sizeof(uname));
	char prompt[MAX_UNAME];
	int index = 0;
	char *un_arrp[MAX_UNAME];
	for (int i=0;i<MAX_CLIENTS;i++){
		un_arrp[i] = usernames[i];
	}

	snprintf(prompt, sizeof(prompt), "USERNAME: ");
	if(send(socket, prompt, sizeof(prompt), 0)<0){
		printf("Failed to send prompt message\n");
		return -1;
	}
	if(recv(socket, uname, MAX_UNAME, 0)<0){
		return -1;
	}

	pthread_mutex_lock(&lock);
	if(find_string_in_array(un_arrp, uname, MAX_CLIENTS)!=-1){
		snprintf(prompt, sizeof(prompt), "Username taken");
		if(send(socket, prompt, sizeof(prompt), 0)<0){
			printf("Failed to send error message\n");
		}
		pthread_mutex_unlock(&lock);
		return -1;
		}
	index = find_free((int*)usernames, MAX_CLIENTS);
	if(index < 0){
		printf("Client limit reached\n");
		pthread_mutex_unlock(&lock);
	}
	snprintf(usernames[index], sizeof(uname), "%s", uname);
	pthread_mutex_unlock(&lock);
	return index;
}

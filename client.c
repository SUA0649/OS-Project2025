#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int sock_fd;
    struct sockaddr_in server_addr;
    char buf[100];
    
    // Create socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr); // Connect to localhost
    
    // Connect to server
    connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    // Send message
    char *msg = "Hello server!";
    write(sock_fd, msg, strlen(msg)+1);
    
    // Read response
    read(sock_fd, buf, sizeof(buf));
    printf("Client received: %s\n", buf);
    
    close(sock_fd);
    
    return 0;
}
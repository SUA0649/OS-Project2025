#include "client.h"


int main() {
    signal(SIGINT,handle_sigint);
    get_username();
    // Get valid port
    init_ui();

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8"); // Google DNS
    serv.sin_port = htons(53); // DNS port
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0 || connect(fd, (struct sockaddr *)&serv, sizeof(serv))) {
        close(fd);
        endwin();
        fprintf(stderr, "Connection failed\n");
        return 1;
    }
    
    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    getsockname(fd, (struct sockaddr *)&name, &namelen);
    inet_ntop(AF_INET, &name.sin_addr, LOCAL_IP, INET_ADDRSTRLEN);
    close(fd);


    current_socket = connect_to_server();
    if(current_socket < 0) {
        endwin();
        fprintf(stderr, "Connection failed\n");
        return 1;
    }

    chat_loop();

    if(p2p_socket != -1) close(p2p_socket);
    if(current_socket != -1) close(current_socket);
    endwin();
    clear();
    return 0;
}
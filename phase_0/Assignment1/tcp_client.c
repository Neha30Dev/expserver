#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFF_SIZE 10000

int main() {
    // Creating listening sock
    int client_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Creating an object of struct socketaddr_in
    struct sockaddr_in server_addr;

    char* server_ip=NULL;
    size_t len=0;
    getline(&server_ip,&len,stdin);
    server_ip[strcspn(server_ip,"\n")]='\0';
    char* server_port=NULL;
    getline(&server_port,&len,stdin);
    server_port[strcspn(server_port,"\n")]='\0';
    int port=(int)strtol(server_port,NULL,10);

    // printf("%s",server_ip);
    // printf("%d",port);

    // Setting up server addr
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(port);

    // Connect to tcp server
    if (connect(client_sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        printf("[ERROR] Failed to connect to tcp server\n");
        exit(1);
    } else {
        printf("[INFO] Connected to tcp server\n");
    }

    // while (1) {
    //     // Get message from client terminal
    //     char *line;
    //     size_t line_len = 0;
    //     ssize_t read_n = getline(&line, &line_len, stdin);

    //     send(client_sock_fd, line, read_n, 0);

    //     char buff[BUFF_SIZE];
    //     memset(buff, 0, BUFF_SIZE);

    //     // Read message from client to buffer
    //     read_n =  recv(client_sock_fd, buff, BUFF_SIZE, 0);

    //     if (read_n <= 0) {
    //         printf("[INFO] Error occured. Closing client\n");
    //         close(client_sock_fd);
    //         exit(1);
	// 	}

    //     // Print message from cilent
    //     printf("[SERVER MESSAGE] %s\n", buff);
    // }

  return 0;


}
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#define MAXLINE 1024

// function for receiving 
void rec_from_server(int sockfd, struct sockaddr_in *servaddr,char *buffer) {
    int n;
    socklen_t len;

    len = sizeof(*servaddr);
    n = recvfrom(sockfd, (char *)buffer, MAXLINE, 0,(struct sockaddr *) servaddr, &len);
    if(n < 0) {
        perror("Receive failed"); 
        exit(EXIT_FAILURE); 
    }
    buffer[n] = '\0';
    if(strcmp(buffer,"ERROR") == 0) { // ERROR is a special message sent by server when file format is not correct 
        printf("Incorrect file format\n");
        exit(EXIT_FAILURE);
    }
}

// function for sending
void send_to_server(int sockfd, struct sockaddr_in *servaddr,char *buffer) {
    int status = sendto(sockfd, (const char *)buffer, strlen(buffer), 0,(const struct sockaddr *)servaddr, sizeof(*servaddr));
    if(status < 0) {
        perror("Send failed"); 
        exit(EXIT_FAILURE); 
    }
}

int main() {
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8181);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    char filename[] = "input.txt"; // filename
    
    send_to_server(sockfd,&servaddr,filename); // sending filename

    char buffer[MAXLINE];
    rec_from_server(sockfd,&servaddr,buffer);

    if(strcmp(buffer, "HELLO\n") != 0) { // checking if file is present
        printf("File %s Not Found\n", filename);
        close(sockfd);
        return 0;
    }
    
    char localFile[] = "clientCopy.txt"; // local file created by client
    FILE *file;
    file = fopen(localFile,"w");

    int wordNumber = 1;

    while(1) {
        sprintf(buffer,"WORD%d",wordNumber);
        send_to_server(sockfd,&servaddr,buffer); // sending wordi
        rec_from_server(sockfd,&servaddr,buffer); // receiving wordi from server
        if(strcmp(buffer, "END\n") != 0 && strcmp(buffer, "END") != 0) {
            fprintf(file,"%s",buffer); // writing to file
        } else {
            break;
        }
        wordNumber++;
    }
    printf("Local File created successfully\n");
    fclose(file); // closing file
    close(sockfd); // closing connection
    return 0;
}
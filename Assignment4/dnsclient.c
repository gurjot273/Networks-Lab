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
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8181);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    char domainName[MAXLINE];
    printf("Enter the domain name : ");
    scanf("%s",domainName); // reading domain name

    send_to_server(sockfd,&servaddr,domainName); // sending domain name to server
    
    char ipAddress[MAXLINE];
    rec_from_server(sockfd,&servaddr,ipAddress); // receiving ip address 
    if(strcmp(ipAddress,"ERROR") == 0) // checking if domain name is incorrect
        printf("Domain name incorrect\n");
    else
        printf("Ip Address of %s is %s\n",domainName,ipAddress); // printing ip address
    close(sockfd);
    return 0;
}
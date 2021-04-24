#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <fcntl.h> 
#include <errno.h> 
#include <netdb.h>
#include <dirent.h>

#define MAXLINE 1024 

// function for receiving
void rec_from_client(int sockfd, struct sockaddr_in *cliaddr,char *buffer) {
    int n;
    socklen_t len;

    len = sizeof(*cliaddr);
    n = recvfrom(sockfd, (char *)buffer, MAXLINE, 0,(struct sockaddr *) cliaddr, &len);
    if(n < 0) {
        perror("Receive failed"); 
        exit(EXIT_FAILURE); 
    }
    buffer[n] = '\0';
}

// function for sending
void send_to_client(int sockfd, struct sockaddr_in *cliaddr,char *buffer) {
    int status = sendto(sockfd, (const char *)buffer, strlen(buffer), 0, (const struct sockaddr *) cliaddr, sizeof(*cliaddr));
    if(status < 0) {
        perror("Send failed"); 
        exit(EXIT_FAILURE); 
    }
}

int main() {
    int udp_sockfd; 
    struct sockaddr_in udp_servaddr, udp_cliaddr;

    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0); // creating udp socket
    if(udp_sockfd < 0) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
      
    memset(&udp_servaddr, 0, sizeof(udp_servaddr)); 
    memset(&udp_cliaddr, 0, sizeof(udp_cliaddr)); 
      
    udp_servaddr.sin_family = AF_INET; 
    udp_servaddr.sin_addr.s_addr = INADDR_ANY; 
    udp_servaddr.sin_port = htons(8181); 
      
    if(bind(udp_sockfd,(const struct sockaddr *)&udp_servaddr,sizeof(udp_servaddr)) < 0) { // binding udp socket
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    }

    int tcp_sockfd, tcp_connfd;
    socklen_t tcp_len; 
    struct sockaddr_in tcp_servaddr, tcp_cliaddr; 
      
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0); // creating udp socket
    if ( tcp_sockfd < 0 ) {  
        perror("Socket creation failed"); 
        exit(EXIT_FAILURE); 
    }
      
    memset(&tcp_servaddr, 0, sizeof(tcp_servaddr)); 
    memset(&tcp_cliaddr, 0, sizeof(tcp_cliaddr)); 
      
    tcp_servaddr.sin_family = AF_INET; 
    tcp_servaddr.sin_addr.s_addr = INADDR_ANY; 
    tcp_servaddr.sin_port = htons(8181); 
      
    if(bind(tcp_sockfd,(const struct sockaddr *)&tcp_servaddr, sizeof(tcp_servaddr)) < 0 ) { // Bind the socket with the server address 
        perror("Bind failed"); 
        exit(EXIT_FAILURE); 
    }

    if(listen(tcp_sockfd,5) != 0) { // listening on port
        perror("Listen failed\n");
        exit(EXIT_FAILURE); 
    }

    fd_set readSet;
    FD_ZERO(&readSet); // clear the descriptor set 
    int maxfd = ((tcp_sockfd>udp_sockfd)?tcp_sockfd:udp_sockfd) + 1; // get maxfd 

    while(1) {
        // set listenfd and udpfd in readset
        FD_SET(tcp_sockfd, &readSet);
        FD_SET(udp_sockfd, &readSet);

        // select the ready descriptor
        if(select(maxfd, &readSet, NULL, NULL, NULL) <= 0)
            break;

        // if tcp socket is readable then handle 
        // it by accepting the connection 
        if(FD_ISSET(tcp_sockfd, &readSet)) {
            tcp_len = sizeof(tcp_cliaddr);
            tcp_connfd = accept(tcp_sockfd,(struct sockaddr *)&tcp_cliaddr, &tcp_len); // accepting connection from client
            if(tcp_connfd < 0) {
                perror("Server accept failed\n");
                continue;
            }

            pid_t pid;
            pid = fork();  // creating child process for tcp
            if(pid == 0) { 
                char subDirectory[MAXLINE];
                if(read(tcp_connfd,subDirectory,sizeof(subDirectory)) <= 0) // reading subdirectory
                    return 0;

                char directoryName[MAXLINE];
                strcpy(directoryName,"image/");
                strcat(directoryName,subDirectory); // creating directory name
                char end[4];
                strcpy(end,"END"); 
                DIR *dir;
                struct dirent *ent;
                int readBytes;
                if((dir = opendir(directoryName)) != NULL) { // opening directory
                    while ((ent = readdir (dir)) != NULL) { // reading files in directory
                        char fileName[MAXLINE];
                        strcpy(fileName,directoryName);
                        strcat(fileName,"/");
                        strcat(fileName,ent->d_name);
                        if(strcmp(ent->d_name,".") == 0)
                            continue;
                        if(strcmp(ent->d_name,"..") == 0)
                            continue;
                        int file;
                        if((file = open(fileName, O_RDONLY)) < 0) { // opening file
                            continue;
                        }
                        char buffer[MAXLINE];
                        int readBytes;
                        while((readBytes = read(file,buffer,sizeof(buffer))) > 0){ // reading from file into buffer until file finished
                            write(tcp_connfd,buffer,readBytes); // writing to client from buffer
                        }
                        char fileflag[MAXLINE]; // flag to show image has been sent
                        strcpy(fileflag, "IMAGE"); 
                        write(tcp_connfd,fileflag,strlen(fileflag));
                        close(file); 
                    }
                    write(tcp_connfd,end,4); // END after sending all the images
                } else {
                    write(tcp_connfd,"ERROR",6); // if directory not present
                }
                close(tcp_connfd);
                return 0;
            }
            close(tcp_connfd);
        } else if(FD_ISSET(udp_sockfd, &readSet)) { // if udp socket is readable receive the message.
            char domainName[MAXLINE]; 
            rec_from_client(udp_sockfd, &udp_cliaddr, domainName); // recieving domain name from client

            struct hostent *host;
            char *ipAddress;
            host = gethostbyname(domainName); // getting host from domain name
            if(host == NULL) { // if host not found
                send_to_client(udp_sockfd, &udp_cliaddr, "ERROR"); // flag for domain name incorrect
                continue;
            }

            ipAddress = inet_ntoa(*((struct in_addr*)host->h_addr_list[0])); // finding ip address from host
            send_to_client(udp_sockfd, &udp_cliaddr, ipAddress); // sending ip address
        }
    }

    close(tcp_sockfd);
    close(udp_sockfd);

}
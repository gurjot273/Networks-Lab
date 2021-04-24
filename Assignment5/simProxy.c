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

// function to find max
int max(int a, int b){
    return (a>b)?a:b;
}

int main(int argc, char* argv[]) {

    if(argc<4){
        printf("Number of command line arguments insufficient\n");
        exit(EXIT_FAILURE);
    }
    
    int tcp_sockfd;
    int tcp_connfd[MAXLINE],institute_sockfd[MAXLINE];

// if no connections setting all to -1
    for(int i=0;i<MAXLINE;i++){
        tcp_connfd[i]=-1;
        institute_sockfd[i]=-1;
    }

    socklen_t tcp_len; 
    struct sockaddr_in tcp_servaddr, tcp_cliaddr,institute_addr; 

    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0); // creating tcp socket
    if ( tcp_sockfd < 0 ) {  
        printf("Socket creation failed"); 
        exit(EXIT_FAILURE); 
    }
      
    memset(&institute_addr, 0,sizeof(institute_addr));
    memset(&tcp_servaddr, 0, sizeof(tcp_servaddr)); 
    memset(&tcp_cliaddr, 0, sizeof(tcp_cliaddr)); 

    institute_addr.sin_family = AF_INET;
    institute_addr.sin_addr.s_addr = inet_addr(argv[2]);
    institute_addr.sin_port = htons(atoi(argv[3]));

    tcp_servaddr.sin_family = AF_INET; 
    tcp_servaddr.sin_addr.s_addr = INADDR_ANY; 
    tcp_servaddr.sin_port = htons(atoi(argv[1]));

    if(bind(tcp_sockfd,(const struct sockaddr *)&tcp_servaddr, sizeof(tcp_servaddr)) < 0 ) { // Bind the socket with the server address 
        printf("Bind failed\n"); 
        exit(EXIT_FAILURE); 
    }

    if(listen(tcp_sockfd,5) != 0) { // listening on port
        printf("Listen failed\n");
        exit(EXIT_FAILURE); 
    }

    fcntl(tcp_sockfd, F_SETFL, O_NONBLOCK);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    fd_set readSet;

    int maxfd = tcp_sockfd + 1; // get maxfd 

    int numConn=0;
    char buffer[MAXLINE];

    printf("Proxy running on port %d. Forwarding all connections to %s:%d\n",(int)ntohs(tcp_servaddr.sin_port),inet_ntoa(institute_addr.sin_addr),(int)ntohs(institute_addr.sin_port));

    while(1) {
        // select the ready descriptor

        FD_ZERO(&readSet); // clear the descriptor set
        FD_SET(tcp_sockfd, &readSet); // setting fd in readset
        FD_SET(STDIN_FILENO, &readSet);

        for(int i=0;i<numConn;i++){
            if(tcp_connfd[i]!=-1)
                FD_SET(tcp_connfd[i], &readSet);
            if(institute_sockfd[i]!=-1)
                FD_SET(institute_sockfd[i], &readSet);
        }
        // select the ready descriptor
        if(select(maxfd, &readSet, NULL, NULL, NULL) <= 0)
            continue;

        // if tcp socket is readable then handle 
        // it by accepting the connection 

        if(FD_ISSET(STDIN_FILENO, &readSet)){
            
            int readBytes,filled=0;
            while(1){
                readBytes = read(STDIN_FILENO,buffer+filled,MAXLINE-filled-1); // reading
                if(readBytes<=0)
                    break;
                filled+=readBytes;
            }
            buffer[filled]='\0';
            if((strcmp(buffer,"exit\n")==0)||(strcmp(buffer,"exit")==0)){ // exiting and closing connection
                for(int i=0;i<numConn;i++){
                    if(tcp_connfd[i]!=-1)
                        close(tcp_connfd[i]);
                    if(institute_sockfd[i]!=-1)
                        close(institute_sockfd[i]);
                }
                close(tcp_sockfd);
                exit(0);
            }
        }

        if(FD_ISSET(tcp_sockfd, &readSet)) {
            tcp_len = sizeof(tcp_cliaddr);
            int conn_fd;
            conn_fd = accept(tcp_sockfd,(struct sockaddr *)&tcp_cliaddr, &tcp_len); // accepting connection from client
            
            if(conn_fd < 0) {
                printf("Server accept failed\n");
                continue;
            }

            printf("Connection accepted from %s:%d\n",inet_ntoa(tcp_cliaddr.sin_addr),(int)ntohs(tcp_cliaddr.sin_port));

            int ptr;

            for(ptr=0;ptr<numConn;ptr++){
                if((tcp_connfd[ptr]==-1) && (institute_sockfd[ptr]==-1)){
                    tcp_connfd[ptr]=conn_fd; // setting tcp_connfd = conn_fd
                    break;
                }
            }

            if(ptr==numConn){
                if(numConn==MAXLINE){ // checking if number of connections exceeded
                    close(conn_fd);
                    printf("Number of connections exceeded\n");
                    continue;
                }else{
                    tcp_connfd[ptr]=conn_fd;
                    numConn++; // updating number of connection
                }
            }

            maxfd=max(maxfd,tcp_connfd[ptr]+1);
            institute_sockfd[ptr] = socket(AF_INET, SOCK_STREAM, 0); // socket to connect to institute proxy

            if(institute_sockfd[ptr] < 0){
                close(tcp_connfd[ptr]);
                tcp_connfd[ptr]=-1;
                institute_sockfd[ptr]=-1;
                printf("Institute Socket creation failed\n"); 
                continue;
            }

            maxfd=max(maxfd,institute_sockfd[ptr]+1);

            if(connect(institute_sockfd[ptr], (struct sockaddr *)&institute_addr, sizeof(institute_addr)) != 0){ // connecting to institute proxy server
                close(tcp_connfd[ptr]);
                close(institute_sockfd[ptr]);
                tcp_connfd[ptr]=-1;
                institute_sockfd[ptr]=-1;
                printf("Institute Socket connect failed\n"); 
                continue;
            }

            fcntl(tcp_connfd[ptr], F_SETFL, O_NONBLOCK);
            fcntl(institute_sockfd[ptr], F_SETFL, O_NONBLOCK);

            FD_SET(tcp_connfd[ptr], &readSet);
            FD_SET(institute_sockfd[ptr], &readSet);
        } 

        for(int i=0;i<numConn;i++){
            if(tcp_connfd[i]!=-1 && FD_ISSET(tcp_connfd[i],&readSet)){
                int readBytes;
                readBytes = recv(tcp_connfd[i],buffer,MAXLINE,0); // reading from client 
                if(readBytes==-1) // no data to be sent
                    continue;
                if(readBytes==0){ //connection closed
                    close(tcp_connfd[i]);
                    close(institute_sockfd[i]);
                    tcp_connfd[i]=-1;
                    institute_sockfd[i]=-1;
                }
                int ptr=0,sendBytes;
                while(readBytes>0){ // writing data to institute proxy server
                    sendBytes=write(institute_sockfd[i],buffer+ptr,readBytes);
                    readBytes-=(sendBytes>0?sendBytes:0);
                    ptr+=(sendBytes>0?sendBytes:0);
                }
            }
        }

        for(int i=0;i<numConn;i++){
            if(institute_sockfd[i]!=-1 && FD_ISSET(institute_sockfd[i],&readSet)){
                int readBytes;
                readBytes = recv(institute_sockfd[i],buffer,MAXLINE,0); // reading response from institute proxy server
                if(readBytes==-1) // no data to be sent
                    continue;
                if(readBytes==0){ //connection closed
                    close(tcp_connfd[i]);
                    close(institute_sockfd[i]);
                    tcp_connfd[i]=-1;
                    institute_sockfd[i]=-1;
                }
                int ptr=0,sendBytes;
                while(readBytes>0){ // writing data back to client 
                    sendBytes=write(tcp_connfd[i],buffer+ptr,readBytes);
                    readBytes-=(sendBytes>0?sendBytes:0);
                    ptr+=(sendBytes>0?sendBytes:0);
                }
            }
        }

    }
    for(int i=0;i<numConn;i++){ // closing all fd's
        if(tcp_connfd[i]!=-1)
            close(tcp_connfd[i]);
        if(institute_sockfd[i]!=-1)
            close(institute_sockfd[i]);
    }
    close(tcp_sockfd);
    return 0;
}
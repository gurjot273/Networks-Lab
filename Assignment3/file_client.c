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

#define MAXLINE 50

int main() { 
    int sockfd, connfd, len; 
    struct sockaddr_in servaddr, cliaddr; 
      
    sockfd = socket(AF_INET, SOCK_STREAM, 0); //creating socket
    if ( sockfd < 0 ) { 
        perror("Socket creation failed"); 
        exit(EXIT_FAILURE); 
    } else {
        printf("Socket created successfully\n");
    }
      
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr)); 
      
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(8181); 

    if(connect(sockfd,(const struct sockaddr *)&servaddr,sizeof(servaddr)) != 0) { //connecting to server
        perror("Connection with server failed\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Connected to the server\n");
    }

    char filename[MAXLINE];
    printf("Enter the filename with extension :  "); //reading filename
    scanf("%s",filename);

    write(sockfd, filename, sizeof(filename)); //sending filename to server

    char buffer[MAXLINE];

    int readBytes;
    if((readBytes=read(sockfd,buffer,sizeof(buffer)) )<= 0) { // if connection closed by server without sending anything
        printf("File Not Found\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int file;
    char outfile[1000];
    strcpy(outfile,"client_");
    strcat(outfile,filename); //client filename

    if((file = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, 0644)) < 0) { // opening new file for writing
        printf("Client file could not be created, so closing\n");
        close(sockfd);
        return 0;
    }

    int numWords=0;
    int numBytes=0;
    int i=0;
    int wordFlag = 0; // indicates if we have encountered characters other than delimiters

    do{
        write(file,buffer,readBytes);
        for(i=0;i<readBytes;i++){
            if(buffer[i]==',' || buffer[i]==';' || buffer[i]==':' || buffer[i]=='.' || buffer[i]==' ' || buffer[i]=='\n'){
                if(wordFlag != 0) {
                    numWords++; // if word was there before delimiter, increment
                }
                wordFlag = 0;
            }else{
                wordFlag = 1; // word found
            }
        }
        numBytes+=readBytes; // incrementing number of bytes
    }while((readBytes=read(sockfd,buffer,sizeof(buffer))) > 0); // till server connection not closed
    
    if(wordFlag == 1) { //last
        numWords++;
    }

    close(file); // closing file
    close(sockfd); // closing socket

    printf("The file transfer is successful. Size of the file = %d bytes, no, of words = %d\n",numBytes,numWords);
}
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

#define MAXLINE 1024

int main() { 
    int sockfd, connfd, len; 
    struct sockaddr_in servaddr, cliaddr; 
      
    sockfd = socket(AF_INET, SOCK_STREAM, 0); //creating socket
    if ( sockfd < 0 ) { 
        perror("Socket creation failed"); 
        exit(EXIT_FAILURE); 
    }
      
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr)); 
      
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(8181); 

    if(connect(sockfd,(const struct sockaddr *)&servaddr,sizeof(servaddr)) != 0) { //connecting to server
        perror("Connection with server failed\n");
        exit(EXIT_FAILURE);
    }

    char subDirectory[MAXLINE];
    printf("Enter the name of the subdirectory : ");
    scanf("%s",subDirectory); // reading subdirectory

    write(sockfd,subDirectory,sizeof(subDirectory)); // sending subdirectory to server

    char buffer[MAXLINE];
    int imageCounter = 0;
    int readBytes;
    while((readBytes = read(sockfd,buffer,sizeof(buffer))) > 0) { // receiving images
        int i;
        int bFlag = 0;
        for(i=0;i<readBytes;i++) {
            // checking for image sent flag
            if((i+4)<readBytes && buffer[i] == 'I' && buffer[i+1] == 'M' && buffer[i+2] == 'A' && buffer[i+3] == 'G' && buffer[i+4] == 'E') { 
                imageCounter++;
            } else if((i+2)<readBytes && buffer[i] == 'E' && buffer[i+1] == 'N' && buffer[i+2] == 'D') { // checking for end
                bFlag = 1;
                break;
            } else if((i+4)<readBytes && buffer[i] == 'E' && buffer[i+1] == 'R' && buffer[i+2] == 'R' && buffer[i+3] == 'O' && buffer[i+4] == 'R') { // error flag is subdirectory not present
                close(sockfd);
                printf("Subdirectory not found\n");
                return 0;
            }
        }
        if(bFlag) {
            break;
        }
    }
    printf("Number of image received is %d\n",imageCounter); // printing total images found 

    close(sockfd); 
}
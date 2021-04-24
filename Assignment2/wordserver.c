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
void rec_from_client(int sockfd, struct sockaddr_in *cliaddr,char *buffer) {
    int n;
    socklen_t len;

    len = sizeof(*cliaddr);
    n = recvfrom(sockfd, (char *)buffer, MAXLINE, 0,(struct sockaddr *) cliaddr, &len);
    if(n < 0) {
        perror("Recieve failed"); 
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
    int sockfd; 
    struct sockaddr_in servaddr, cliaddr; 
      
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
      
    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 
      
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(8181); 
      
    if(bind(sockfd,(const struct sockaddr *)&servaddr,sizeof(servaddr)) < 0) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    printf("\nServer Running....\n");

    char filename[MAXLINE];
    rec_from_client(sockfd, &cliaddr, filename); // receiving filename 

    FILE *file;
    char buffer[MAXLINE];
    if(!(file = fopen(filename, "r"))) { // checking if file exists
        strcpy(buffer,"NOTFOUND ");
        strcat(buffer,filename);
        send_to_client(sockfd,&cliaddr,buffer); // sending not found message
        close(sockfd);
        return 0;
    }

    char word[MAXLINE];
    if(!fgets(word,MAXLINE,file) || (strcmp("HELLO\n", word) != 0)) { // checking if HELLO is present
        printf("Incorrect format\n"); // sending error message
        strcpy(word,"ERROR");
        send_to_client(sockfd,&cliaddr,word);
        fclose(file);
        close(sockfd);
        return 0;
    }
    send_to_client(sockfd,&cliaddr,word); // sending hello

    int wordNumber = 1;

    while(1) {
        rec_from_client(sockfd,&cliaddr,word); // receiving wordi
        sprintf(buffer,"WORD%d",wordNumber);
        if(strcmp(word,buffer) == 0 && fgets(word,MAXLINE,file)) { // reading word from file 
            send_to_client(sockfd,&cliaddr,word); // sending word to client
        } else {
            printf("Incorrect format\n"); // if END not present at the end of the file
            strcpy(buffer, "ERROR");
            send_to_client(sockfd,&cliaddr,buffer);
            break;
        }
        if(strcmp(word,"END\n") == 0 || strcmp(word,"END") == 0 ) { // exiting when END encountered
            break;
        }
        wordNumber++;
    }

    printf("Server closed\n");
    fclose(file); // closing file
    close(sockfd); // closing connection
}
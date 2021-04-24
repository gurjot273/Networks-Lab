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

#define MAXLINE 200
#define SZ 8192

// function to find max
int max(int a, int b){
    return (a>b)?a:b;
}

void parseUrl(char url[],char host[],int *port,char page[]){ // functio to parse url
    int j=7; // for http://
    int ptr=0;
    int len=strlen(url); // finding length
    while(j<len && url[j]!=':' && url[j]!='/'){
        host[ptr++]=url[j++]; // till : or / is found
    }
    host[ptr]='\0';
    if(j==len){ // if neither : or /, default port 80
        *port=80;
        strcpy(page,"");
        return;
    }
    if(url[j]=='/'){ // if no : , default port 80
        *port=80;
        j++;
        ptr=0;
        while(j<len){
            page[ptr++]=url[j++]; // finding page
        }
        page[ptr]='\0';
        return;
    }
    j++;
    // if : found
    char temp[SZ];
    ptr=0;
    while(j<len && url[j]!='/') // while / not found
        temp[ptr++]=url[j++];
    temp[ptr]='\0';
    *port=atoi(temp); // getting port
    if(j==len){
        strcpy(page,"");
        return;
    }
    j++;
    ptr=0;
    while(j<len){
        page[ptr++]=url[j++]; // finding page
    }
    page[ptr]='\0';
    return; 
}

void parseHttp(char text[],int len,char method[],char url[],char version[],char rest[]){ // function to parse http headers
    char temp[SZ];
    int isComplete=1,ptr=0;
    int words=0;
    for(int k=0;k<len;k++){
        if(text[k]!=' '){ // while space not encountered
            temp[ptr++]=text[k];
            isComplete=0;
        }
        else{
            if(!isComplete){
                isComplete=1; //word complete
                temp[ptr]='\0'; 
                if(words==0){
                    strcpy(method,temp); // if first word
                    ptr=0; //setting ptr=0 for next word
                    words++; // incrementing words
                }else if(words==1){
                    strcpy(url,temp); // if second word, then is url
                    strcpy(rest,text+k); // remaining part of request
                    char *pos=strstr(rest,"\r\n"); // finding version
                    if(pos==NULL){
                        strcpy(version,"");
                        return;
                    }
                    int leng=(pos-rest); // finding length of version
                    strncpy(version,rest,leng); // setting version
                    return;
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if(argc<2){
        printf("Number of command line arguments insufficient\n");
        exit(EXIT_FAILURE);
    }
    int tcp_sockfd;
    int tcp_connfd[MAXLINE],forward_fd[MAXLINE]; // client and outgoing fds
    char readBuffer[MAXLINE][SZ]; // read buffer for client
    int in[MAXLINE]; // till where data
    char writeBuffer[MAXLINE][SZ]; // write buffer for client
    int out[MAXLINE],toWrite[MAXLINE]; // out specifies till where has data already been written, toWrite specifies till where to write

    char forwardBuff[MAXLINE][SZ]; // readBuffer for outgoing servers, used for reading into it and writing from it
    int forwardOut[MAXLINE],forwardWrite[MAXLINE]; // forwardOut specifies till where has data already been written,forwardWrite specifies till where to write

    char buffer[SZ],rest[SZ],url[SZ],method[SZ],host[SZ],page[SZ],rem[SZ],num[SZ],version[SZ];
    
    
    for(int i=0;i<MAXLINE;i++){ // initialising the buffers and fd arrays
        tcp_connfd[i]=-1;
        forward_fd[i]=-1;
        strcpy(readBuffer[i],"");
        strcpy(writeBuffer[i],"");
        in[i]=0;
        out[i]=0;
        toWrite[i]=0;
        strcpy(forwardBuff[i],"");
        forwardOut[i]=0;
        forwardWrite[i]=0;
    }

    socklen_t tcp_len; 
    struct sockaddr_in tcp_servaddr, tcp_cliaddr,forward_addr; 
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0); // creating tcp socket
    if ( tcp_sockfd < 0 ) {  
        printf("Socket creation failed"); 
        exit(EXIT_FAILURE); 
    }
    int enable = 1;
	setsockopt(tcp_sockfd , SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)); // to enable reuse of port to avoid bind fail error
    memset(&forward_addr, 0,sizeof(forward_addr));
    memset(&tcp_servaddr, 0, sizeof(tcp_servaddr)); 
    memset(&tcp_cliaddr, 0, sizeof(tcp_cliaddr)); 
    forward_addr.sin_family = AF_INET;
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
    fcntl(tcp_sockfd, F_SETFL, O_NONBLOCK); // setting as nonblocking
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);  // setting as nonblocking
    fd_set readSet,testReadSet,writeSet,testWriteSet; // testReadSet is given to select so that readSet does not get modified by select
    FD_ZERO(&readSet); // clear the descriptor set
    FD_SET(tcp_sockfd, &readSet); // setting fd in readset
    FD_SET(STDIN_FILENO, &readSet);
    FD_ZERO(&writeSet); // clear the write set
    int maxfd = tcp_sockfd + 1;
    int numConn=0;
    printf("Proxy running on port %d\n",(int)ntohs(tcp_servaddr.sin_port));
    while(1) {
        for(int i=0;i<numConn;i++){ // checking all the read buffers of  client to see if any request header had been received completely
            if(tcp_connfd[i]!=-1 && in[i]!=0){
                char *pos=strstr(readBuffer[i],"\r\n\r\n"); // if any request header had been received completely, ends with \r\n\r\n
                if(pos==NULL || toWrite[i]!=0) // if not found or write buffer is not empty, something remains to be written from client to server
                    continue;
                int length=(pos-readBuffer[i]);
                length+=4; //for \r\n\r\n
                pos=strstr(readBuffer[i],"Content-Length:"); // finding length of payload using Content-Length:
                if(pos!=NULL){
                    int st=(pos-readBuffer[i]);
                    st+=15; // for Content-Length:
                    pos=strstr(readBuffer[i]+st,"\r\n"); // finding Content Length
                    if(pos==NULL) // if not found, continue
                        continue;
                    int len=(pos-(readBuffer[i]+st));
                    strncpy(num,readBuffer[i]+st,len);
                    int val=atoi(num); // finding Content Length
                    length+=val; // incrementing by payload length
                    if(length>strlen(readBuffer[i])) // if payload not completely received, continue
                        continue;
                }
                strcpy(rem,readBuffer[i]+length); // storing rest of the readBuffer, excluding current request
                readBuffer[i][length]='\0'; // removing other than current request
                parseHttp(readBuffer[i],strlen(readBuffer[i]),method,url,version,rest);
                if(strcmp(method,"GET")==0 || strcmp(method,"POST")==0){ // drop requests other than GET AND POST
                    int port=80;
                    parseUrl(url,host,&port,page); // parse url
                    printf("%s %s%s, Host: %s, Port: %d, Path: /%s\n",method,url,version,host,port,page); // print for current request
                    struct hostent *hst = gethostbyname(host); // find the ip address
                    if(hst==NULL){ // if ip address not found, continue
                       forward_fd[i]=-1;
                       readBuffer[i][length]=rem[0];
                       continue; 
                    }
                    char *ipAddress = inet_ntoa(*((struct in_addr*)hst->h_addr_list[0]));
                    forward_addr.sin_addr.s_addr = inet_addr(ipAddress);
                    forward_addr.sin_port = htons(port);
                    forward_fd[i] = socket(AF_INET, SOCK_STREAM, 0); // creating outgoing conn socket
                    if(forward_fd[i]<0){ // if not created, continue
                        forward_fd[i]=-1;
                        readBuffer[i][length]=rem[0];
                        continue;
                    }
                    fcntl(forward_fd[i], F_SETFL, O_NONBLOCK); // set socket as non blocking
                    connect(forward_fd[i],(struct sockaddr *)&forward_addr,sizeof(forward_addr)); // try to connect to server address and port
                    maxfd=max(maxfd,forward_fd[i]+1); 
                    out[i]=0; // setting out to pint to beginning of writeBuffer
                    sprintf(writeBuffer[i],"%s /%s%s",method,page,rest); // use relative address and storing request in writeBuffer
                    toWrite[i]=strlen(writeBuffer[i]); // finding length of request to write
                    FD_SET(forward_fd[i],&writeSet); // setting forward_fd[i] to writeSet, for sending request
                    FD_SET(forward_fd[i],&readSet); // setting forward_fd[i] to readSet, as its readBuffer would be empty
                }
                strcpy(readBuffer[i],rem); // remove current request from readBuffer
                in[i]=strlen(readBuffer[i]); // finding length of readBuffer
                FD_SET(tcp_connfd[i],&readSet); // setting tcp_connfd[i] to readSet as its readBuffer has been freed and can now read more data
            }
        }

        testReadSet=readSet; // setting testReadSet to readSet
        testWriteSet=writeSet;  // setting testWriteSet to writeSet

        if(select(maxfd, &testReadSet, &testWriteSet, NULL, NULL) <= 0) // select
            continue;
            
        if(FD_ISSET(STDIN_FILENO, &testReadSet)){ // if stdin ready for reading
            int readBytes;
            readBytes = read(STDIN_FILENO,buffer,SZ-1); // reaing from stdin
            if(readBytes!=-1){
                buffer[readBytes]='\0';
                if((strcmp(buffer,"exit\n")==0)||(strcmp(buffer,"exit")==0)){ // if exit entered, exiting and closing connection
                    for(int i=0;i<numConn;i++){
                        if(tcp_connfd[i]!=-1)
                            close(tcp_connfd[i]);
                        if(forward_fd[i]!=-1)
                            close(forward_fd[i]);
                    }
                    close(tcp_sockfd);
                    exit(0);
                }
            }
        }

        if(FD_ISSET(tcp_sockfd,&testReadSet)) {
            tcp_len = sizeof(tcp_cliaddr);
            int conn_fd;
            conn_fd=accept(tcp_sockfd,(struct sockaddr *)&tcp_cliaddr, &tcp_len); // accepting connection from client
            if(conn_fd < 0) {
                printf("Server accept failed\n");
                continue;
            }
            if(numConn==MAXLINE){ // checking if number of connections exceeded
                close(conn_fd);
                printf("Number of connections exceeded\n");
                continue;
            }
            printf("Connection accepted from %s:%d\n",inet_ntoa(tcp_cliaddr.sin_addr),(int)ntohs(tcp_cliaddr.sin_port));
            int ptr;
            for(ptr=0;ptr<numConn;ptr++){
                if((tcp_connfd[ptr]==-1) && (forward_fd[ptr]==-1)){
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
            fcntl(tcp_connfd[ptr],F_SETFL,O_NONBLOCK); // ssetting socket as non blocking 
            FD_SET(tcp_connfd[ptr],&readSet); // setting tcp_connfd[ptr] as its readBuffer is empty 
            numConn++;
        } 

        for(int i=0;i<numConn;i++){
            if(tcp_connfd[i]!=-1 && FD_ISSET(tcp_connfd[i],&testReadSet) && (in[i]<(SZ-1))){ // if readBuffer not full
                int readBytes = recv(tcp_connfd[i],readBuffer[i]+in[i],SZ-in[i]-1,0); // reading from client
                if(readBytes==-1) // no data to be sent
                    continue;
                if(readBytes==0){ //connection closed
                    FD_CLR(tcp_connfd[i],&readSet);
                    FD_CLR(tcp_connfd[i],&writeSet);
                    FD_CLR(forward_fd[i],&readSet);
                    FD_CLR(forward_fd[i],&writeSet);
                    close(tcp_connfd[i]);
                    close(forward_fd[i]);
                    tcp_connfd[i]=-1;
                    forward_fd[i]=-1;
                    continue;
                }
                in[i]+=readBytes; // incrementing in[i] by readzBytes
                readBuffer[i][in[i]]='\0';
                if(in[i]>=SZ-1) // if readBuffer is full, remove tcp_connfd[i] from readSet
                    FD_CLR(tcp_connfd[i],&readSet);
            }
        }

        for(int i=0;i<numConn;i++){
            if(forward_fd[i]!=-1 && FD_ISSET(forward_fd[i],&testWriteSet) && (toWrite[i]!=0)){ // if writeBuffer is empty
                int sendBytes=send(forward_fd[i],writeBuffer[i]+out[i],toWrite[i]-out[i],MSG_NOSIGNAL); // sending to server
                if(sendBytes==-1)
                    continue;
                out[i]+=sendBytes;
                if(out[i]==toWrite[i]){ 
                    toWrite[i]=out[i]=0;
                    FD_CLR(forward_fd[i],&writeSet); // if writeBuffer becomes empty, remove forward_fd[i] from writeSet, as it has nothing to write
                }
            }
        }

        for(int i=0;i<numConn;i++){
            if(forward_fd[i]!=-1 && FD_ISSET(forward_fd[i],&testReadSet) && forwardWrite[i]==0){ // reading into forwardBuff, if nothing to write from forwardBuff
                int readBytes=recv(forward_fd[i],forwardBuff[i],SZ,0); // reading from server
                if(readBytes==-1) // no data to be sent
                    continue;
                if(readBytes==0){ //connection closed
                     FD_CLR(forward_fd[i],&readSet);
                     FD_CLR(forward_fd[i],&writeSet);
                     close(forward_fd[i]);
                     forward_fd[i]=-1;
                     continue;
                }
                forwardOut[i]=0; // seting out to beginning of forwardBuffer
                forwardWrite[i]=readBytes; // no of bytes to write to client are same as no of bytes read from server
                FD_CLR(forward_fd[i],&readSet); // remove forward_fd[i] from readSet, as must first write the data from forwardBuff to read more data
                FD_SET(tcp_connfd[i],&writeSet); // adding tcp_connfd[i] to write set, as there is data to be written from server to client
            }
        }


        for(int i=0;i<numConn;i++){
            if(tcp_connfd[i]!=-1 && FD_ISSET(tcp_connfd[i],&testWriteSet)  && forwardWrite[i]!=0){ // if something to write from ForwardBuff to client
                int sendBytes=send(tcp_connfd[i],forwardBuff[i]+forwardOut[i],forwardWrite[i]-forwardOut[i],MSG_NOSIGNAL); // sending to client
                if(sendBytes==-1)
                    continue;
                forwardOut[i]+=sendBytes; // sendBytes have been written
                if(forwardOut[i]==forwardWrite[i]){// if forwardBuff becomes empty after writing 
                    forwardOut[i]=forwardWrite[i]=0; // nothing to write 
                    FD_SET(forward_fd[i],&readSet); // adding forward_fd[i] to readSet as it can now read that everything has been written to client
                    FD_CLR(tcp_connfd[i],&writeSet); // removing tcp_connfd[i] from writeSet as it has nothing to write
                }
            }
        }

    }

    for(int i=0;i<numConn;i++){ // closing all fd's and exiting
        if(tcp_connfd[i]!=-1)
            close(tcp_connfd[i]);
        if(forward_fd[i]!=-1)
            close(forward_fd[i]);
    }
    close(tcp_sockfd);

    return 0;
}
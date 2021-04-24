#include "rsocket.h"

//GLobal Variables
int udpfd=-1;
int ct_trans=0;

int counter=0;
int send_start=0,send_end=0,send_ct=0;

int recv_start=0,recv_end=0,recv_ct=0;

/*Data Structures */
struct send_buffer_elem{
    int id;
    char msg[MSG_SIZE];
    size_t len;
    struct sockaddr_in dest_addr;
    socklen_t addrlen;
} * send_buffer;

struct receive_buffer_elem{
    char msg[MSG_SIZE];
    size_t msg_len;
    struct sockaddr_in src_addr;
    socklen_t addrlen;
} * receive_buffer;

struct unacknowledged_message{
    int id;
    char msg[MSG_SIZE];
    size_t len;
    struct timeval tv;
    struct sockaddr_in dest_addr;
    socklen_t addrlen;
} * unacknowledged_message_table;

int * received_message_id_table;

// Function to handle acknowledgement messages
void HandleACKMsgRecv(int id){
    for(int i=0;i<TABLE_SIZE;i++){
        if(unacknowledged_message_table[i].id==id){
            unacknowledged_message_table[i].id=-1;
        }
    }
}

// Handle application message
void HandleAppMsgRecv(char *buf,int id,struct sockaddr_in src_addr, socklen_t addr_len,int msg_len){
    int found=0;
    for(int i=0;i<TABLE_SIZE;i++)
        if(received_message_id_table[i]==id){ //finding if message is already recieved
            found=1;
            break;
        }
    if(!found){
        if(recv_ct>=TABLE_SIZE)
            return;
        int place=-1;
        for(int i=0;i<TABLE_SIZE;i++)
            if(received_message_id_table[i]==-1){ // finding place in received_message_id_table
                place=i;
                break;
            }
        if(place==-1)
            return;
        // If it is not a duplicate, copy the message to the receive buffer
        received_message_id_table[place]=id;
        memcpy(receive_buffer[recv_end].msg,buf,msg_len);
        receive_buffer[recv_end].msg_len=msg_len;
        receive_buffer[recv_end].src_addr=src_addr;
        receive_buffer[recv_end].addrlen=addr_len;
        recv_end=(recv_end+1)%TABLE_SIZE;
        recv_ct++;
    }
    char buffer[MSG_SIZE];
    buffer[0]='A'; //for ack
    memcpy(buffer+1,&id,sizeof(id));
    sendto(udpfd,buffer,1+sizeof(id),MSG_DONTWAIT,(struct sockaddr *)&src_addr,addr_len); // sending ack
}

// Function to handle the message received
void HandleReceive(){
    int numBytes;
    char buf[MSG_SIZE];
    struct sockaddr_in src_addr;
    socklen_t addrlen=sizeof(src_addr);
    // reveiving message
    numBytes=recvfrom(udpfd,buf,MSG_SIZE,MSG_DONTWAIT,(struct sockaddr *)&src_addr,&addrlen);
    if(numBytes==-1)
        return;
    int id;
    memcpy(&id,buf+1,sizeof(id));
    // if message is dropped
    if(dropMessage(DROP_PROB)==1){
        return;
    }
    // calling handle according to the type of message
    if(buf[0]=='M')
        HandleAppMsgRecv(buf+(1+sizeof(id)),id,src_addr,addrlen,numBytes-(1+sizeof(id)));
    else
        HandleACKMsgRecv(id);
}

// Function to handle retransmission of expired, unacknowledged messages
void HandleRetransmit(){
   for(int i=0;i<TABLE_SIZE;i++){
        if(unacknowledged_message_table[i].id==-1)
            continue;
        struct timeval cur;
        gettimeofday(&cur,NULL);
        // If the message has timed out,    
        if ((cur.tv_sec-unacknowledged_message_table[i].tv.tv_sec)>=T){
            // Retransmit the message
            sendto(udpfd,unacknowledged_message_table[i].msg,unacknowledged_message_table[i].len,MSG_DONTWAIT,(struct sockaddr *)(&unacknowledged_message_table[i].dest_addr),unacknowledged_message_table[i].addrlen);
            ct_trans++;
            // Update the time for the message
            gettimeofday(&unacknowledged_message_table[i].tv,NULL);
        }
   } 
}

// Function to handle transmission of messages
void HandleTransmit(){
    int place=-1;
    for(int i=0;i<TABLE_SIZE;i++)
        if(unacknowledged_message_table[i].id==-1){ // finding place in unacknowledged table
            place=i;
            break;
        }
    if(place==-1)
        return;
    while(send_ct>0){
        int place=-1;
        for(int i=0;i<TABLE_SIZE;i++)
            if(unacknowledged_message_table[i].id==-1){
                place=i;
                break;
            }
        if(place==-1)
            return;
        //udp send call
        sendto(udpfd,send_buffer[send_start].msg,send_buffer[send_start].len,MSG_DONTWAIT,(struct sockaddr *)(&send_buffer[send_start].dest_addr),send_buffer[send_start].addrlen);
        ct_trans++;
        // updating unacknowledgement table
        unacknowledged_message_table[place].id=send_buffer[send_start].id;
        unacknowledged_message_table[place].addrlen=send_buffer[send_start].addrlen;
        unacknowledged_message_table[place].dest_addr=send_buffer[send_start].dest_addr;
        unacknowledged_message_table[place].len=send_buffer[send_start].len;
        memcpy(unacknowledged_message_table[place].msg,send_buffer[send_start].msg,send_buffer[send_start].len);
        gettimeofday(&unacknowledged_message_table[place].tv,NULL);
        send_start=(send_start+1)%TABLE_SIZE;
        send_ct--;
    }
}

// Function for signal handler
void sighandler(int signo){
    HandleReceive();
    HandleRetransmit();
    HandleTransmit();
    return;
}

// function to create the socket
int r_socket(int domain, int type, int protocol){
    if(type!=SOCK_MRP)
        return -1;
    udpfd=socket(domain,SOCK_DGRAM,protocol);
    ct_trans=0;
    counter=0;
    send_start=send_end=send_ct=0;
    recv_start=recv_end=recv_ct=0;
    if(udpfd<0)
        return -1;

    //init tables
    send_buffer=(struct send_buffer_elem *)malloc(TABLE_SIZE*sizeof(struct send_buffer_elem));
    receive_buffer=(struct receive_buffer_elem *)malloc(TABLE_SIZE*sizeof(struct receive_buffer_elem));
    unacknowledged_message_table=(struct unacknowledged_message *)malloc(TABLE_SIZE*sizeof(struct unacknowledged_message));
    received_message_id_table=(int *)malloc(TABLE_SIZE*sizeof(int));
    for(int i=0;i<TABLE_SIZE;i++){
        unacknowledged_message_table[i].id=-1;
        received_message_id_table[i]=-1;  // -1 means id not presesnt
    }
    signal(SIGALRM,sighandler);
    struct itimerval timer;
    /* Configure the timer to expire after every PERIOD micro sec... */
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = PERIOD;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = PERIOD;
    setitimer(ITIMER_REAL,&timer,NULL);
    return udpfd;
}

// function to bind the socket
int r_bind(int socket, const struct sockaddr *address, socklen_t address_len){
    if(socket!=udpfd)
        return -1;
    return bind(socket, address, address_len);
}

// Function to send messages.
ssize_t r_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen){
    if(sockfd!=udpfd)
        return -1;
    while(send_ct>=TABLE_SIZE); // spin_block
    send_buffer[send_end].id=counter;
    send_buffer[send_end].msg[0]='M';
    memcpy(send_buffer[send_end].msg+1,&counter,sizeof(counter));
    char *buffer=(char *)buf;
    memcpy(send_buffer[send_end].msg+(1+sizeof(counter)),buffer,len);
    send_buffer[send_end].len=len+1+sizeof(counter);
    send_buffer[send_end].dest_addr=*(struct sockaddr_in *)dest_addr;
    send_buffer[send_end].addrlen=addrlen;
    counter++;
    send_end=(send_end+1)%TABLE_SIZE;
    send_ct++;
    return 0;
}

// Function to read data from the receive buffer
ssize_t r_recvfrom(int sockfd, void *buff, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen){
    char *buf=(char *)buff;
    if(sockfd!=udpfd)
        return -1;
    while(recv_ct==0)
        sleep(0.001);
    int msg_len=receive_buffer[recv_start].msg_len;
    size_t length=(msg_len<len)?msg_len:len;
    memcpy(buf,receive_buffer[recv_start].msg,length);
    *src_addr=*(struct sockaddr *)(&receive_buffer[recv_start].src_addr);
    *addrlen=receive_buffer[recv_start].addrlen;
    recv_start=(recv_start+1)%TABLE_SIZE;
    recv_ct--;
    return length;
}

// Function to close the socket
int r_close(int sockfd){
    if(sockfd!=udpfd)
        return -1;

    int isComplete=0;
    // Check if there is any unacknowledged message left
    while(!isComplete){
        isComplete=1;
        if(send_ct>0)
            isComplete=0;
        for(int i=0;i<TABLE_SIZE;i++){
            if(unacknowledged_message_table[i].id!=-1)
                isComplete=0;
        }
    }
    // Print the number of transmissions before closing the socket
    printf("No of transmissions = %d\n", ct_trans);
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL,&timer,NULL); // stopping the timer
    free(send_buffer);
    free(receive_buffer);
    free(unacknowledged_message_table);
    free(received_message_id_table);
    return close(udpfd);
}

// function to find whether to drop the message
int dropMessage(float p){
    float val=((float)rand()/(float)RAND_MAX);
    return val<p;
}
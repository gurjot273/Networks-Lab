#include "rsocket.h"

#define ROLL 1058
#define SZ 100

int main(){
    struct sockaddr_in src_addr,dest_addr;
    memset(&src_addr,0,sizeof(src_addr));
    memset(&dest_addr,0,sizeof(dest_addr));
    src_addr.sin_family = AF_INET;
	src_addr.sin_addr.s_addr = INADDR_ANY;
	src_addr.sin_port = htons(50000+2*ROLL);
    dest_addr.sin_family = AF_INET;
	dest_addr.sin_addr.s_addr = INADDR_ANY;
    dest_addr.sin_port = htons(50000+2*ROLL+1);
    int sockfd=r_socket(AF_INET,SOCK_MRP,0);
    if(sockfd<0){
        printf("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }
    socklen_t src_len,dest_len;
    src_len=sizeof(src_addr);
    if(r_bind(sockfd,(const struct sockaddr *)&src_addr,src_len)<0){
        printf("Bind failed\n");
        exit(EXIT_FAILURE);
    }
    printf("Started sender\n");
    char text[SZ];
    printf("Enter the input string of length between 25 and 100\n");
    scanf("%s",text);
    int leng=strlen(text);
    for(int i=0;i<leng;i++){
        dest_len=sizeof(dest_addr);
        r_sendto(sockfd,text+i,1,0,(const struct sockaddr *)&dest_addr,dest_len);
        printf("Sent character: %c\n",text[i]);
    }
    r_close(sockfd);
    return 0;
}
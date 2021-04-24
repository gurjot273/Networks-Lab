#ifndef RSOCKET_H
#define RSOCKET_H

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
#include <signal.h>
#include <time.h>
#include <sys/time.h> 

#define T 2
#define DROP_PROB 0.20
#define MSG_SIZE 105 // 5 extra bytes , 1 for storing ack or msg and 4 for storing integer id
#define SOCK_MRP 1
#define TABLE_SIZE 100
#define PERIOD 2500 // timer period in micro seconds


int r_socket(int domain, int type, int protocol);

int r_bind(int socket, const struct sockaddr *address, socklen_t address_len);

ssize_t r_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

ssize_t r_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

int r_close(int sockfd);

int dropMessage(float p);

#endif
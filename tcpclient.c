#include<stdio.h>	
#include<string.h> 
#include<stdlib.h> 
#include <unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>

#define BUFSIZE 1024
#define PORT 1234

#define P "ECHO"

void stop(char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	struct sockaddr_in sockaddr;
	int sockfd;
	char message[BUFSIZE+1];

	if ( (sockfd=socket(AF_INET, SOCK_STREAM , 0)) == -1) {
		stop("socket");
	}

	sockaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(PORT);
	
	//Connect to remote server
	if (connect(sockfd , (struct sockaddr *)&sockaddr , sizeof(sockaddr)) < 0) {
		perror("Error connect");
	}
	
	printf("Connected\n");

  for(int i=0; i<3; i++) {
		
    //send the message
		if(send(sockfd, P, strlen(P) , 0) == -1) {
			stop("Error send");
		}

    bzero(&message ,BUFSIZE+1);
		if(recv(sockfd, message, BUFSIZE, 0) < 0 ){
			stop("Error recv");
		}
		printf("Recv: %s\n", message);			
	}

	close(sockfd);
	return 0;
}

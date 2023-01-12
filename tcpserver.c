#include <stdio.h>	
#include <string.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER "127.0.0.1"
#define BUFSIZE 1024
#define PORT 1234

void stop(char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

int main(int agrc, char **argv) {
  struct sockaddr_in servaddr, clientaddr;
	int sockfd, clientfd;
	char message[BUFSIZE+1];

	if ((sockfd=socket(AF_INET, SOCK_STREAM , 0)) == -1) {
		stop("Error socket");
	}

	memset((char *) &servaddr, 0, sizeof(servaddr));
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(PORT);
	
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
		stop("Error bind");
	}

	// Now server is ready to listen	
	if (listen(sockfd, 5) != 0) {
    stop("Error listen");
  }

  // Accept a client
  memset((char *) &clientaddr, 0, sizeof(clientaddr));
  int clientaddrLength = sizeof(clientaddr);
  if((clientfd = accept(sockfd, (struct sockaddr *) &clientaddr, (socklen_t *)&clientaddrLength)) < 0) {
    stop("Error accept");
  }

  for(;;) {
    bzero(&message ,BUFSIZE+1);
		if(recv(clientfd, message, BUFSIZE, 0) < 0 ){
			stop("Error recv");
		}

    if(strlen(message) > 0)
		  printf("Recv: %s\n", message);	

		//send the message
		if(send(clientfd, message, strlen(message) , 0) == -1) {
			stop("Error send");
		}
  }

  close(clientfd);
	close(sockfd);

  return EXIT_SUCCESS;
}

#include<stdio.h>	
#include<string.h> 
#include<stdlib.h> 
#include <unistd.h>
#include <string.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <time.h>

void ntp_request(char*);
void notify(void);
void stop(char*);

#define BUFSIZE 1024
#define NTP_TIMESTAMP_DELTA 2208988800ull

int main(int argc, char **argv)
{
	struct sockaddr_in sockaddr;
	int sockfd;
	char message[BUFSIZE+1];

	// User inputs
	int port; 
	char server_addr[30];
	char uname[30];
	char dt[50];

  char inbuf[BUFSIZE+2];

	// printf("server_addr: ");
	// scanf("%s", server_addr);
	// printf("port: ");
	// scanf("%d", &port);

  strcpy(server_addr, "127.0.0.1");
  port = 8000;

	if ( (sockfd=socket(AF_INET, SOCK_STREAM , 0)) == -1) {
		stop("socket");
	}

  int nfds = STDOUT_FILENO;
  if (sockfd > nfds) {
    nfds = sockfd;
  }
  ++nfds;

	sockaddr.sin_addr.s_addr = inet_addr(server_addr);
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);
	
	//Connect to remote server
	printf("Connecting to %s port %d...\n", server_addr, port);
	if (connect(sockfd , (struct sockaddr *)&sockaddr , sizeof(sockaddr)) < 0) {
		perror("Error connect");
	}
	
	printf("Connected\n");

	while(1) {
		printf("uname: ");
		scanf("%s", uname);
		printf("Authorizing to %s port %d with uname %s...\n", server_addr, port, uname);
		if(send(sockfd, uname, strlen(uname) , 0) == -1) {
			stop("Error send");
		}

		bzero(&message ,BUFSIZE+1);
		if(recv(sockfd, message, BUFSIZE, 0) < 0 ){
			stop("Error recv");
		}

		if(strlen(message) > 0) {
			if(strcmp(message, "unauthorized") == 0)
				printf("uname alreary used.\n");	
			else if(strcmp(message, "authorized") == 0)
				break;
		}
	}

	ntp_request(dt);
	printf("[%s] Authorized\n", dt);

  int s;

  while(1) {
    fd_set rfds;
    fd_set wfds;

    FD_SET(STDIN_FILENO, &rfds);
    FD_SET(sockfd, &rfds);
    FD_SET(STDOUT_FILENO, &wfds);
    FD_SET(sockfd, &wfds);

    s = select(nfds, &rfds, &wfds, NULL, NULL);

    if (s == -1) {
      perror("Error select");
      close(sockfd);
      return EXIT_FAILURE;
    }

    // User IO
    if (FD_ISSET(STDIN_FILENO, &rfds)) {
      ssize_t r;
     
      memset(&inbuf, 0, sizeof inbuf);
      r = read(STDOUT_FILENO, inbuf, BUFSIZE);
      if (r < 1) {
        perror("Error recv");
        close(sockfd);
        return EXIT_FAILURE;
      }
      
      inbuf[r-1] = '\0';

      /* ensure we can write to sock */
      if (!FD_ISSET(sockfd, &wfds)) {
              /* select on sock for writing */
              fd_set wfds_sock;
              FD_SET(sockfd, &wfds_sock);
              s = select(sockfd + 1, NULL, &wfds_sock, NULL, NULL);
              /* always going to be sock or error */
              if (s == -1) {
                perror("Error select");
                close(sockfd);
                return EXIT_FAILURE;
              }
      }

      /* write to socket */
      r = send(sockfd, inbuf, r + 1, 0);
      if (r == -1) {
        perror("Error send");
        close(sockfd);
        return EXIT_FAILURE;
      }
    }

    // Server IO
    if (FD_ISSET(sockfd, &rfds)) {
      bzero(&message ,BUFSIZE+1);
      if(recv(sockfd, message, BUFSIZE, 0) < 1){
        perror("Error recv");
        close(sockfd);
        return EXIT_FAILURE;
      }

      if(strlen(message) > 0)
        ntp_request(dt);
        printf("[%s] %s\n", dt, message);
    }
  }

	return EXIT_SUCCESS;
}

void 
notify(void) {
  putchar('\a');
  return; 
}

void 
stop(char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}


void 
ntp_request(char *dt) {
  int sockfd, n; // Socket file descriptor and the n return result from writing/reading from the socket.

  int port = 123; // NTP UDP port number.

  char* host_name = "us.pool.ntp.org"; // NTP server host-name.

  // Structure that defines the 48 byte NTP packet protocol.

  typedef struct
  {

    uint8_t li_vn_mode;      // Eight bits. li, vn, and mode.
                             // li.   Two bits.   Leap indicator.
                             // vn.   Three bits. Version number of the protocol.
                             // mode. Three bits. Client will pick mode 3 for client.

    uint8_t stratum;         // Eight bits. Stratum level of the local clock.
    uint8_t poll;            // Eight bits. Maximum interval between successive messages.
    uint8_t precision;       // Eight bits. Precision of the local clock.

    uint32_t rootDelay;      // 32 bits. Total round trip delay time.
    uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
    uint32_t refId;          // 32 bits. Reference clock identifier.

    uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
    uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.

    uint32_t origTm_s;       // 32 bits. Originate time-stamp seconds.
    uint32_t origTm_f;       // 32 bits. Originate time-stamp fraction of a second.

    uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
    uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.

    uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
    uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.

  } ntp_packet;              // Total: 384 bits or 48 bytes.

  // Create and zero out the packet. All 48 bytes worth.

  ntp_packet packet = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  memset( &packet, 0, sizeof( ntp_packet ) );

  // Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3. The rest will be left set to zero.

  *( ( char * ) &packet + 0 ) = 0x1b; // Represents 27 in base 10 or 00011011 in base 2.

  // Create a UDP socket, convert the host-name to an IP address, set the port number,
  // connect to the server, send the packet, and then read in the return packet.

  struct sockaddr_in server_addr; // Server address data structure.
  struct hostent *server;      // Server data structure.

  sockfd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ); // Create a UDP socket.

  if ( sockfd < 0 )
    stop( "ERROR opening socket" );

  server = gethostbyname( host_name ); // Convert URL to IP.

  if ( server == NULL )
    stop( "ERROR, no such host" );

  // Zero out the server address structure.

  bzero( ( char* ) &server_addr, sizeof( server_addr ) );

  server_addr.sin_family = AF_INET;

  // Copy the server's IP address to the server address structure.

  bcopy( ( char* )server->h_addr, ( char* ) &server_addr.sin_addr.s_addr, server->h_length );

  // Convert the port number integer to network big-endian style and save it to the server address structure.

  server_addr.sin_port = htons( port );

  // Call up the server using its IP address and port number.

  if ( connect( sockfd, ( struct sockaddr * ) &server_addr, sizeof( server_addr) ) < 0 )
    stop( "ERROR connecting" );

  // Send it the NTP packet it wants. If n == -1, it failed.

  n = write( sockfd, ( char* ) &packet, sizeof( ntp_packet ) );

  if ( n < 0 )
    stop( "ERROR writing to socket" );

  // Wait and receive the packet back from the server. If n == -1, it failed.

  n = read( sockfd, ( char* ) &packet, sizeof( ntp_packet ) );

  if ( n < 0 )
    stop( "ERROR reading from socket" );

  // These two fields contain the time-stamp seconds as the packet left the NTP server.
  // The number of seconds correspond to the seconds passed since 1900.
  // ntohl() converts the bit/byte order from the network's to host's "endianness".

  packet.txTm_s = ntohl( packet.txTm_s ); // Time-stamp seconds.
  packet.txTm_f = ntohl( packet.txTm_f ); // Time-stamp fraction of a second.

  // Extract the 32 bits that represent the time-stamp seconds (since NTP epoch) from when the packet left the server.
  // Subtract 70 years worth of seconds from the seconds since 1900.
  // This leaves the seconds since the UNIX epoch of 1970.
  // (1900)------------------(1970)**************************************(Time Packet Left the Server)

  time_t txTm = ( time_t ) ( packet.txTm_s - NTP_TIMESTAMP_DELTA );

  // Print the time we got from the server, accounting for local timezone and conversion from UTC time.

  sprintf(dt, "%s", ctime( ( const time_t* ) &txTm ));
	dt[strlen(dt)-1] = '\0';

  close(sockfd);
}
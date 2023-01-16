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
#include <ctype.h>

#define BUFSIZE 1024
#define NTP_TIMESTAMP_DELTA 2208988800ull
#define MAX_FILENAME_LENGTH 32

void stop(char*);
void ntp_request(char*);

char *
trim(char *s) 
{
    char *ptr;
    if (!s)
        return NULL;   // handle NULL string
    if (!*s)
        return s;      // handle empty string
    for (ptr = s + strlen(s) - 1; (ptr >= s) && isspace(*ptr); --ptr);
    ptr[1] = '\0';
    return s;
}

int
send_file(int sockfd, char *filename){
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) 
  {
    perror("Error in reading file.");
    fclose(fp);
    return EXIT_FAILURE;
  }
  char data[BUFSIZE+1] = {0};

  while(fgets(data, BUFSIZE, fp) != NULL) {
    if ( send(sockfd, data, sizeof(data), 0) == -1 ) {
      perror("Error in sending file.");
      fclose(fp);
      return EXIT_FAILURE;
    }
    printf("%s", data);
    bzero(data, BUFSIZE + 1);
  }
  fclose(fp);
  return EXIT_SUCCESS;
}

void 
write_file(int sockfd, char *filename)
{
  int n;
  FILE *fp;
  char buffer[BUFSIZE + 1];
  fp = fopen(filename, "w");
  while ( (n = read(sockfd, buffer, BUFSIZE + 1)) > 0) 
  {
    fprintf(fp, "%s", buffer);
    bzero(buffer,  BUFSIZE + 1);
  }
  fclose(fp);
  return;
}

int main(int argc, char **argv)
{
	struct sockaddr_in sockaddr, new_addr, listeneraddr;
  socklen_t addr_size;
	int sockfd;
  int lsockfd;
  int rsockfd;
  int new_sockfd;
  int max_sd;
  int dcc = 0;
	char message[BUFSIZE+1];

	int port; 
	char server_addr[30];
	char dt[50];
  char random_string[MAX_FILENAME_LENGTH + 1];
  char filename[MAX_FILENAME_LENGTH + 1];
  char *token;  

  char inbuf[BUFSIZE+2];
  char original_inbuf[BUFSIZE+2];

	// printf("server_addr: ");
	// scanf("%s", server_addr);
	// printf("port: ");
	// scanf("%d", &port);

  strcpy(server_addr, "127.0.0.1");
  port = 8000;

	if ( (sockfd=socket(AF_INET, SOCK_STREAM , 0)) == -1) {
		stop("socket");
	}

	sockaddr.sin_addr.s_addr = inet_addr(server_addr);
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);
	
	//Connect to remote server
	printf("Connecting to %s port %d...\n", server_addr, port);
	if (connect(sockfd , (struct sockaddr *)&sockaddr , sizeof(sockaddr)) < 0) {
		perror("Error connect");
	}

  printf("Connected.\n");
  
  while(1) {
    fd_set rfds;
    fd_set wfds;

    max_sd = STDOUT_FILENO;
    if (sockfd > max_sd) {
      max_sd = sockfd;
    }

    if ( dcc ) 
    {
      FD_SET(lsockfd, &rfds);
      if ( lsockfd > max_sd )
      {
        max_sd = lsockfd;
      }
    }
    
    FD_SET(STDIN_FILENO, &rfds);
    FD_SET(sockfd, &rfds);
    FD_SET(STDOUT_FILENO, &wfds);
    FD_SET(sockfd, &wfds);

    if ( select(max_sd + 1, &rfds, &wfds, NULL, NULL) == -1 ) 
    {
      perror("Error select");
      close(sockfd);
      return EXIT_FAILURE;
    }

    // User IO
    if (FD_ISSET(STDIN_FILENO, &rfds)) {
      ssize_t r;
     
      bzero(&inbuf, BUFSIZE + 1);
      r = read(STDOUT_FILENO, inbuf, BUFSIZE);
      if (r < 1) {
        perror("Error recv");
        close(sockfd);
        return EXIT_FAILURE;
      }
     
      if ( r > 1 )
        inbuf[r-1] = '\0';

      /* ensure we can write to sock */
      if ( !FD_ISSET(sockfd, &wfds) ) 
      {
        /* select on sock for writing */
        fd_set wfds_sock;
        FD_SET(sockfd, &wfds_sock);
      
        /* always going to be sock or error */
        if ( select(sockfd + 1, NULL, &wfds_sock, NULL, NULL) == -1 ) {
          perror("Error select");
          close(sockfd);
          return EXIT_FAILURE;
        }
      }

      if ( strlen(inbuf) >= 1 && *inbuf != '\n' && *inbuf != ' ' ) 
      {

        memset(&original_inbuf, 0, sizeof inbuf);
        strcpy(original_inbuf, inbuf);
        token = strtok(inbuf, " ");

        if ( strncmp(token, "/offer", 6) == 0 && dcc == 0) 
        {
          printf("\033[0;32mSetting up a listenner socket. \033[0;m\n");
      
          lsockfd = socket(AF_INET, SOCK_STREAM, 0);
          if(lsockfd < 0) 
          {
            perror("\033[0;31mError socket.\033[0;m");
          } 
          else 
          {
            printf("\033[0;32mServer socket created successfully.\033[0;m\n");
            token = strtok(NULL, " ");

            // get filename
            token = strtok(NULL, " ");
            bzero(&filename, MAX_FILENAME_LENGTH + 1);
            strncpy(filename, token, strlen(token));

            sockaddr.sin_family = AF_INET;
            sockaddr.sin_port = htons(8080);
            sockaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

            if( bind(lsockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0 ) 
            {
              perror("\033[0;31mError in bind.\033[0;m");
            } 
            else{
              printf("\033[0;32mBinding successfull.\033[0;m\n");

              if( listen(lsockfd, 2) == 0 )
              {
                printf("\033[0;32mListening...\033[0;m\n");
                dcc = 1;
              }
              else
              {
                perror("\033[0;31mError in listening.\033[0;m");
              }
            }
          }
        } 
        
        if ( strncmp(token, "/download", 9) == 0 && dcc == 0)
        {
          token = strtok(NULL, " ");

          bzero(random_string, MAX_FILENAME_LENGTH + 1);
          strcpy(random_string, "XXXXXXXXX");
          mktemp(random_string);
          sprintf(filename, "%s_%s", random_string, token);

          rsockfd = socket(AF_INET, SOCK_STREAM, 0);
          if(rsockfd < 0) {
            perror("Error socket");
            exit(1);
          }
          printf("\033[0;32mServer socket created successfully.\033[0;m\n");

          listeneraddr.sin_family = AF_INET;
          listeneraddr.sin_port = htons(8080);
          listeneraddr.sin_addr.s_addr = inet_addr("127.0.0.1");

          if( connect(rsockfd, (struct sockaddr*)&listeneraddr, sizeof(listeneraddr)) == -1 ) {
            perror("Error socket");
            exit(1);
          }
          printf("\033[0;32mConnected to Server.\033[0;m\n");

          write_file(rsockfd, filename);
          printf("\033[0;32mData written successfully in \033[0;33m%s\033[0;m.\033[0;m\n", filename);


          printf("\033[0;32mClosing the connection.\033[0;m\n");
          close(rsockfd);
        }
        else 
        {
          /* write to socket */
          if ( send(sockfd, original_inbuf, r + 1, 0) == -1 ) 
          {
            perror("Error send");
            close(sockfd);
            return EXIT_FAILURE;
          }
        }
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

    // Listener IO
    if ( FD_ISSET(lsockfd, &rfds) )
    {
      addr_size = sizeof(new_addr);
      if ( (new_sockfd = accept(lsockfd, (struct sockaddr *)&sockaddr, (socklen_t*)&addr_size)) < 0 )
      {
        perror("Error accept");
      } 
      else 
      {
        //inform user of socket number - used in send and receive commands
        printf("\033[0;33mNew connection\033[0;m , socket fd is %d , ip is : %s , port : %d \n" , new_sockfd , inet_ntoa(sockaddr.sin_addr) , ntohs(sockaddr.sin_port));

        if ( send_file(new_sockfd, filename) == EXIT_SUCCESS ) 
        {
          printf("\033[0;32mFile data sent successfully.\033[0;m\n");
        }
        else
        {
          printf("\033[0;31mError send file.\033[0;m\n");
        }
        
        close(new_sockfd);
      }
      printf("\033[0;32mClosing the connection.\033[0;m\n");
      close(lsockfd);
      dcc = 0;
    }
  }

	return EXIT_SUCCESS;
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
  int server_len;
  struct timeval tv = {
    .tv_usec = 400000
  };

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

  if( setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0 ) {
      stop("Error setsockopt");
  }

  if ( sockfd < 0 )
    stop( "ERROR opening socket" );

  server = gethostbyname( host_name ); // Convert URL to IP.

  if ( server == NULL ) {
    perror( "ERROR, no such host" );
    strcpy(dt, "Error no host");
    return;
  }

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

  server_len = sizeof(server_addr);
  if ( (n = recvfrom( sockfd, ( char* ) &packet, sizeof( ntp_packet ), 0, ( struct sockaddr * ) &server_addr, (socklen_t *)&server_len)) < 0 ) {
    perror( "Error recvfrom" );
    strcpy(dt, "Error recvfrom");
    return;
  }
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
  return;
}
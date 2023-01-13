#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>

#define BUFSIZE 1024
#define PORT 8000
#define GREETING_MESSAGE "Connected."
#define AUTHORIZED "authorized"
#define UNAUTHORIZED "unauthorized"
#define NTP_TIMESTAMP_DELTA 2208988800ull
#define INVALID_NUMBER_ARGS "invalid numbers of arguments."
#define MAX_UNAME_LENGTH 32
#define MAX_PWORD_LENGTH 32
#define MAX_MSG_LENGTH 2048
#define COMMAND_MAX_LENGTH 30

typedef struct Client
{
  int fd;
  struct Client *next;
  char uname[MAX_UNAME_LENGTH + 1];
  int authorized;
} Client;

typedef struct
{
  Client *head;
} List;

typedef struct User
{
  char uname[MAX_UNAME_LENGTH + 1];
  char pword[MAX_PWORD_LENGTH + 1];
  struct User *next;
} User;

typedef struct
{
  User *head;
} RegisterList;

void stop(char *);
void send_message_to_all(List, char *);
int get_command(char *, char *);
int is_command(char *);
void disconnect_user(int, struct sockaddr_in, int, List *);
void ntp_request(char *);
int get_pseudo(char *, char *);
int delete_user(RegisterList *l, char *uname, char *pword);
int uname_exist(List, char *);

void change_user_nickname(Client *c, char *uname);
int modify_registered_nickname(RegisterList *l, Client *c, char *uname, char *pword);
int is_registered(RegisterList, char *);

void init_list(List *l)
{
  l->head = NULL;
}

void add_client_socket(List *l, int fd)
{
  Client *new_client = (Client *)malloc(sizeof(Client));
  new_client->fd = fd;
  new_client->next = l->head;
  new_client->authorized = 0;
  l->head = new_client;
}

void authorize_client(Client *c)
{
  c->authorized = 1;
}

void delete_client_socket(List *l, int fd)
{
  Client *temp = l->head, *prev;

  // If head node itself holds the key to be deleted
  if (temp != NULL && temp->fd == fd)
  {
    l->head = temp->next; // Changed head
    free(temp);           // free old head
    return;
  }

  // Search for the key to be deleted, keep track of the
  // previous node as we need to change 'prev->next'
  while (temp != NULL && temp->fd != fd)
  {
    prev = temp;
    temp = temp->next;
  }

  // If key was not present in linked list
  if (temp == NULL)
    return;

  // Unlink the node from linked list
  prev->next = temp->next;
  free(temp); // Free memory
}

void register_user(RegisterList *register_list, char *uname, char *pword)
{
  User *new_user = (User *)malloc(sizeof(User));
  strcpy(new_user->uname, uname);
  strcpy(new_user->pword, pword);
  new_user->next = register_list->head;
  register_list->head = new_user;
}

void print_linked_list(List l)
{
  Client *c = l.head;
  printf("head: %p\n", l.head);
  while (c != NULL)
  {
    printf("addr: %p fd: %d auth: %d uname: %s next: %p\n", c, c->fd, c->authorized, c->uname, c->next);
    c = c->next;
  }
}

void print_register(RegisterList l)
{
  User *c = l.head;
  printf("head: %p\n", l.head);
  while (c != NULL)
  {
    printf("addr: %p uname: %s pword: %s next: %p\n", c, c->uname, c->pword, c->next);
    c = c->next;
  }
}
int get_fd(List l, char *uname)
{
  Client *c = l.head;
  while (strcmp(c->uname, uname) != 0)
    c = c->next;
  return c->fd;
}

int get_argc(char *p)
{
  int c = 0;
  for (; *p != '\0'; p++)
    if (*p == ' ')
      c++;
  return c + 1;
}

int main(void)
{
  int opt = 1, argc, r;
  char *token;
  int master_socket, addrlen, new_socket, activity, valread, sd;
  int max_sd;
  char dt[50];
  char uname[MAX_UNAME_LENGTH + 1];
  char msg[MAX_MSG_LENGTH + 1];
  char command[COMMAND_MAX_LENGTH + 1];

  List client_socket;
  init_list(&client_socket);

  RegisterList register_list;
  register_list.head = NULL;

  struct sockaddr_in servaddr;

  char buffer[1025];

  fd_set readfds;

  // create a master socket
  if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
  {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // set master socket to allow multiple connections , this is just a good habit, it will work without this
  if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
  {
    stop("Error setsockopt");
  }

  memset((char *)&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  servaddr.sin_port = htons(PORT);

  // bind the socket to localhost port 8000
  if (bind(master_socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
  {
    stop("Error bind");
  }
  printf("Listener on port %d \n", PORT);

  // try to specify maximum of 3 pending connections for the master socket
  if (listen(master_socket, 3) < 0)
  {
    stop("Error listen");
  }

  // accept the incoming connection
  addrlen = sizeof(servaddr);
  puts("Waiting for connections ...");

  while (1) {
    // clear the socket set
    FD_ZERO(&readfds);

    // add master socket to set
    FD_SET(master_socket, &readfds);
    max_sd = master_socket;

    Client *tempClient = client_socket.head;
    while (tempClient != NULL)
    {
      sd = tempClient->fd;
      if (sd > 0)
        FD_SET(sd, &readfds);

      // highest file descriptor number, need it for the select function
      if (sd > max_sd)
        max_sd = sd;

      tempClient = tempClient->next;
    }
    //print_linked_list(client_socket);

    // wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

    if ((activity < 0) && (errno != EINTR))
    {
      printf("select error\n");
    }

    if (FD_ISSET(master_socket, &readfds))
    {
      if ((new_socket = accept(master_socket, (struct sockaddr *)&servaddr, (socklen_t *)&addrlen)) < 0)
      {
        stop("Error accept");
      }

      // inform user of socket number - used in send and receive commands
      printf("New connection , socket fd is %d , ip is : %s , port : %d \n", new_socket, inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));

      add_client_socket(&client_socket, new_socket);
    }

    Client *currentClient = client_socket.head;
    sd = currentClient->fd;
    while (currentClient != NULL) {
      // Handles new client
      if (currentClient->authorized == 0)
      {
        authorize_client(currentClient);
        bzero(uname, MAX_UNAME_LENGTH + 1);
        do
        {
          strcpy(uname, "XXXXXXXXXXXX");
          mktemp(uname);
        } while (uname_exist(client_socket, uname));
        strcpy(currentClient->uname, uname);
        bzero(&msg, MAX_MSG_LENGTH + 1);
        sprintf(msg, "%s: %s", uname, GREETING_MESSAGE);
        r = send(sd, msg, strlen(msg), 0);
        if ( r < 1) 
          perror("Error send");
      }

      if (FD_ISSET(sd, &readfds))
      {
        // Check if it was for closing , and also read the incoming message
        bzero(&buffer, BUFSIZE + 1);
        if ((valread = read(sd, buffer, BUFSIZE)) == 0)
        {
          // Somebody disconnected , get his details and print
          currentClient = currentClient->next;
          disconnect_user(sd, servaddr, addrlen, &client_socket);
          continue;
        }

        else
        {
          // set the string terminating NULL byte on the end of the data read
          if (strlen(buffer) > 0)
          {
            buffer[valread] = '\0';
            printf("recv from %s: %s\n", currentClient->uname, buffer);

            bzero(&msg, MAX_MSG_LENGTH + 1);

            if ((argc = get_argc(buffer)) > 3)
            {
              sprintf(msg, "invalid number of arguments.");
              r = send(sd, msg, strlen(msg), 0);
              if ( r < 1 ) 
                perror("error send");
            }
            else
            {
              // if (is_command(buffer))
               if ( 0 )
              {

                // get command
                bzero(&command, COMMAND_MAX_LENGTH + 1);
                token = strtok(buffer, " ");
                // strcpy(command, token);
                strcpy(command, "jjj");
                printf("currentUser: %s\n", currentClient->uname);
                if (strncmp(command, "/unregister", 11) == 0)
                {
                  if (argc != 3)
                  {
                    send(currentClient->fd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                  }
                  else
                  {
                    token = strtok(NULL, " ");
                    bzero(&uname, MAX_UNAME_LENGTH + 1);
                    strncpy(uname, token, strlen(token));

                    if (strcmp(currentClient->uname, uname) == 0)
                    {
                      token = strtok(NULL, " ");
                      if (delete_user(&register_list, uname, token) == 1)
                      {
                        sprintf(msg, "\033[0;32mnunregistered nickname.\033[0m");
                        send(currentClient->fd, msg, strlen(msg), 0);
                      }
                      else
                      {
                        sprintf(msg, "\033[0;32mnwrong password.\033[0m");
                        send(currentClient->fd, msg, strlen(msg), 0);
                      }
                    }
                    else
                    {
                      sprintf(msg, "\033[0;32myou are not registered as\033[0m \033[0;31m%s\033[0m\033[0;32m.\033[0;31m", uname);
                      send(currentClient->fd, msg, strlen(msg), 0);
                    }
                  }
                }
                else if (strncmp(command, "/register", 9) == 0)
                {
                  if (argc != 3)
                  {

                    send(currentClient->fd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                  }
                  else
                  {

                    token = strtok(NULL, " ");
                    bzero(&uname, MAX_UNAME_LENGTH + 1);
                    strncpy(uname, token, strlen(token));

                    if (strcmp(currentClient->uname, uname) != 0)
                    {
                      sprintf(msg, "\033[0;32myou must\033[0m \033[0;36m/nickname %s\033[0m \033[0;32mfirst to register pseudo\033[0m \033[0;33m%s\033[0m.", uname, uname);
                      send(currentClient->fd, msg, strlen(msg), 0);
                    }
                    else
                    {
                      token = strtok(NULL, " ");

                      register_user(&register_list, uname, token);
                      sprintf(msg, "\033[0;32mnew nickname registered.\033[0m");
                      send(currentClient->fd, msg, strlen(msg), 0);

                      print_register(register_list);
                    }
                  }
                }
                else if (strncmp(command, "/alert", 5) == 0)
                {
                  if (argc == 2)
                  {
                    token = strtok(NULL, " ");
                    sprintf(msg, "\a%s: %s", currentClient->uname, token);
                    send_message_to_all(client_socket, msg);
                  }
                  else if (argc == 3)
                  {
                    token = strtok(NULL, " ");
                    bzero(&uname, MAX_UNAME_LENGTH + 1);
                    strncpy(uname, token, strlen(token));
                    if (strcmp(currentClient->uname, uname) == 0)
                    {
                      sprintf(msg, "invalid command.");
                      send(currentClient->fd, msg, strlen(msg), 0);
                    }
                    else if (uname_exist(client_socket, uname))
                    {
                      token = strtok(NULL, " ");
                      sprintf(msg, "\a\033[0;31m[alert message from %s]\033[0m %s: %s", currentClient->uname, currentClient->uname, token);
                      send(get_fd(client_socket, uname), msg, strlen(msg), 0);
                    }
                    else
                    {
                      sprintf(msg, "user does not exist.");
                      send(currentClient->fd, msg, strlen(msg), 0);
                    }
                  }
                  else
                  {
                    send(currentClient->fd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                  }
                }
                else if (strncmp(command, "/exit", 4) == 0)
                {
                  if (argc != 1)
                  {
                    send(currentClient->fd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                  }
                  else
                  {
                    currentClient = currentClient->next;
                    disconnect_user(sd, servaddr, addrlen, &client_socket);
                    continue;
                  }
                }
                else if (strncmp(command, "/date", 4) == 0)
                {
                  if (argc != 1)
                  {
                    send(currentClient->fd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                  }
                  else
                  {
                    ntp_request(dt);
                    sprintf(msg, "[%s]%s: %s", command, currentClient->uname, dt);
                    send(currentClient->fd, msg, strlen(msg), 0);
                  }
                }
                else if (strncmp(command, "/blue", 4) == 0)
                {
                  if (argc != 2)
                  {
                    send(currentClient->fd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                  }
                  else
                  {
                    token = strtok(NULL, " ");
                    sprintf(msg, "%s: \033[0;34m%s\033[0m", currentClient->uname, token);
                    send_message_to_all(client_socket, msg);
                  }
                }
                else if (strncmp(command, "/red", 3) == 0)
                {
                  if (argc != 2)
                  {
                    send(currentClient->fd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                  }
                  else
                  {
                    token = strtok(NULL, " ");
                    sprintf(msg, "%s: \033[0;31m%s\033[0m", currentClient->uname, token);
                    send_message_to_all(client_socket, msg);
                  }
                }
                else if (strncmp(command, "/green", 5) == 0)
                {
                  if (argc != 2)
                  {
                    send(currentClient->fd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                  }
                  else
                  {
                    token = strtok(NULL, " ");
                    sprintf(msg, "%s: \033[0;32m%s\033[0m", currentClient->uname, token);
                    send_message_to_all(client_socket, msg);
                  }
                }
                else if (strncmp(command, "/mp", 3) == 0)
                {
                  if (argc != 3)
                  {
                    send(currentClient->fd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                  }
                  else
                  {
                    token = strtok(NULL, " ");
                    bzero(&uname, MAX_UNAME_LENGTH + 1);
                    strncpy(uname, token, strlen(token));
                    if (strcmp(currentClient->uname, uname) == 0)
                    {
                      sprintf(msg, "invalid command.");
                      send(currentClient->fd, msg, strlen(msg), 0);
                    }
                    else if (uname_exist(client_socket, uname))
                    {
                      token = strtok(NULL, " ");
                      sprintf(msg, "\033[0;31m[private message from %s]\033[0m %s: %s", currentClient->uname, currentClient->uname, token);
                      send(get_fd(client_socket, uname), msg, strlen(msg), 0);
                    }
                    else
                    {
                      sprintf(msg, "user does not exist.");
                      send(currentClient->fd, msg, strlen(msg), 0);
                    }
                  }
                }
                else if (strncmp(command, "/nickname", 9) == 0)
                {
                  if (argc == 1)
                  {
                    sprintf(msg, "\033[0;32myour current nickname is\033[0m \033[0;31m%s\033[0m.", currentClient->uname);
                    r = send(sd, msg, strlen(msg), 0);
                    if ( r < 1) {
                      perror("Error send");
                    }
                  }
                  else if ( argc == 2 )
                  {

                    token = strtok(NULL, " ");

                    if ( uname_exist(client_socket, token) == 0 )
                    {
                      bzero(currentClient->uname, MAX_UNAME_LENGTH + 1);
                      change_user_nickname(currentClient, token);
                      printf("new nickname: %s\n", currentClient->uname);
                      sprintf(msg, "\033[0;32mnew nickname. your current nickname is\033[0m \033[0;31m%s\033[0m.", currentClient->uname);
                      r = send(sd, msg, strlen(msg), 0);
                      if ( r < 1) {
                        perror("Error send");
                      }
                    }
                    else
                    {
                      sprintf(msg, "\033[0;31mnickname already used.\033[0m");
                      r = send(currentClient->fd, msg, strlen(msg), 0);
                      if ( r < 1)
                        perror("Error send");
                    }
                  }
                  else
                  {
                    token = strtok(NULL, " ");
                    bzero(&uname, MAX_UNAME_LENGTH + 1);
                    strncpy(uname, token, strlen(token));
                    if (uname_exist(client_socket, uname) == 0)
                    {
                      token = strtok(NULL, " ");
                      if (modify_registered_nickname(&register_list, currentClient, uname, token))
                      {
                        sprintf(msg, "[%s] \033[0;32mnew registered nickname. Your current nickname is\033[0m \033[0;31m%s\033[0m.", command, currentClient->uname);
                        send(sd, msg, strlen(msg), 0);
                      }
                      else
                      {
                        sprintf(msg, "[%s] \033[0;32mnwrong password.\033[0m", command);
                        send(sd, msg, strlen(msg), 0);
                      }
                    }
                    else
                    {
                      sprintf(msg, "[%s] \033[0;32m%s already taken.\033[0m", uname, command);
                      send(sd, msg, strlen(msg), 0);
                    }
                  }
                }
                else
                {
                  sprintf(msg, "command not found");
                  r = send(sd, msg, strlen(msg), 0);
                  if ( r < 1 ) 
                    perror("Error send");
                }
              }
              else
              { 
                // Send a message to all users
                sprintf(msg, "[%s] %s", currentClient->uname, buffer);
                send_message_to_all(client_socket, msg);
              }
            }
          }
        }
      }
      currentClient = currentClient->next;
    }
  }
  close(master_socket);
  return EXIT_SUCCESS;
}

int is_registered(RegisterList l, char *uname)
{
  User *u = l.head;
  while (u != NULL)
  {
    if (strcmp(u->uname, uname) == 0)
      return 1;
    u = u->next;
  }
  return 0;
}

void change_user_nickname(Client *c, char *uname)
{
  strcpy(c->uname, uname);
  return;
}

int delete_user(RegisterList *l, char *uname, char *pword)
{
  User *u = l->head;
  while (u != NULL)
  {
    if (strcmp(u->uname, uname) == 0)
    {
      if (strcmp(u->pword, pword) == 0)
      {
        User *temp = l->head, *prev;

        // If head node itself holds the key to be deleted
        if (temp != NULL && strcmp(temp->uname, uname) != 0)
        {
          l->head = temp->next; // Changed head
          free(temp);           // free old head
          return 1;
        }

        // Search for the key to be deleted, keep track of the
        // previous node as we need to change 'prev->next'
        while (temp != NULL && strcmp(temp->uname, uname) != 0)
        {
          prev = temp;
          temp = temp->next;
        }

        // If key was not present in linked list
        if (temp == NULL)
        {
          return 0;
        }

        // Unlink the node from linked list
        prev->next = temp->next;
        free(temp); // Free memory

        return 1;
      }
      else
      {
        return 0;
      }
    }
    u = u->next;
  }
  return -1;
}

int modify_registered_nickname(RegisterList *l, Client *c, char *uname, char *pword)
{
  User *u = l->head;
  while (u != NULL)
  {
    if (strcmp(u->uname, uname) == 0)
    {
      if (strcmp(u->pword, pword) == 0)
      {
        change_user_nickname(c, uname);
        return 1;
      }
      else
      {
        return 0;
      }
    }
    u = u->next;
  }
  return -1;
}

void stop(char *msg)
{
  perror(msg);
  exit(EXIT_FAILURE);
}

int uname_exist(List l, char *uname)
{
  Client *c = l.head;
  while (c != NULL)
  {
    if (strcmp(c->uname, uname) == 0)
      return 1;
    c = c->next;
  }
  return 0;
}

void send_message_to_all(List client_socket, char *msg)
{
  Client *tempClient = client_socket.head;
  while (tempClient != NULL)
  {
    send(tempClient->fd, msg, strlen(msg), 0);
    tempClient = tempClient->next;
  }
  return;
}

int is_command(char *p)
{
  return *p == '/';
}

int get_command(char *ans, char *p)
{
  int i;
  for (i = 0; *p != ' ' && *p != '\0' && *p != '\n'; i++, p++)
    *(ans + i) = *p;

  if (*p == '\n')
    *(ans + i) = '\0';
  else
    *(ans + i + 1) = '\0';

  return i;
}

void disconnect_user(int sd, struct sockaddr_in servaddr, int addrlen, List *client_socket)
{
  getpeername(sd, (struct sockaddr *)&servaddr, (socklen_t *)&addrlen);
  printf("Host disconnected , ip %s , port %d, fd %d\n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port), sd);

  // Close the socket and mark as 0 in list for reuse
  close(sd);
  delete_client_socket(client_socket, sd);
}

int get_pseudo(char *ans, char *p)
{
  int i;
  for (i = 0; *p != ' ' && *p != '\0'; i++, p++)
    *(ans + i) = *p;

  if (*p == '\n')
    *(ans + i) = '\0';
  else
    *(ans + i + 1) = '\0';

  return i;
}

void ntp_request(char *dt)
{
  int sockfd, n; // Socket file descriptor and the n return result from writing/reading from the socket.

  int port = 123; // NTP UDP port number.
  int server_len;
  struct timeval tv = {
      .tv_sec = 1};

  char *host_name = "us.pool.ntp.org"; // NTP server host-name.

  // Structure that defines the 48 byte NTP packet protocol.

  typedef struct
  {

    uint8_t li_vn_mode; // Eight bits. li, vn, and mode.
                        // li.   Two bits.   Leap indicator.
                        // vn.   Three bits. Version number of the protocol.
                        // mode. Three bits. Client will pick mode 3 for client.

    uint8_t stratum;   // Eight bits. Stratum level of the local clock.
    uint8_t poll;      // Eight bits. Maximum interval between successive messages.
    uint8_t precision; // Eight bits. Precision of the local clock.

    uint32_t rootDelay;      // 32 bits. Total round trip delay time.
    uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
    uint32_t refId;          // 32 bits. Reference clock identifier.

    uint32_t refTm_s; // 32 bits. Reference time-stamp seconds.
    uint32_t refTm_f; // 32 bits. Reference time-stamp fraction of a second.

    uint32_t origTm_s; // 32 bits. Originate time-stamp seconds.
    uint32_t origTm_f; // 32 bits. Originate time-stamp fraction of a second.

    uint32_t rxTm_s; // 32 bits. Received time-stamp seconds.
    uint32_t rxTm_f; // 32 bits. Received time-stamp fraction of a second.

    uint32_t txTm_s; // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
    uint32_t txTm_f; // 32 bits. Transmit time-stamp fraction of a second.

  } ntp_packet; // Total: 384 bits or 48 bytes.

  // Create and zero out the packet. All 48 bytes worth.

  ntp_packet packet = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  memset(&packet, 0, sizeof(ntp_packet));

  // Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3. The rest will be left set to zero.

  *((char *)&packet + 0) = 0x1b; // Represents 27 in base 10 or 00011011 in base 2.

  // Create a UDP socket, convert the host-name to an IP address, set the port number,
  // connect to the server, send the packet, and then read in the return packet.

  struct sockaddr_in server_addr; // Server address data structure.
  struct hostent *server;         // Server data structure.

  sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Create a UDP socket.

  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
  {
    stop("Error setsockopt");
  }

  if (sockfd < 0)
    stop("ERROR opening socket");

  server = gethostbyname(host_name); // Convert URL to IP.

  if (server == NULL)
  {
    perror("ERROR, no such host");
    strcpy(dt, "Error no host");
    return;
  }

  // Zero out the server address structure.

  bzero((char *)&server_addr, sizeof(server_addr));

  server_addr.sin_family = AF_INET;

  // Copy the server's IP address to the server address structure.

  bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);

  // Convert the port number integer to network big-endian style and save it to the server address structure.

  server_addr.sin_port = htons(port);

  // Call up the server using its IP address and port number.

  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    stop("ERROR connecting");

  // Send it the NTP packet it wants. If n == -1, it failed.

  n = write(sockfd, (char *)&packet, sizeof(ntp_packet));

  if (n < 0)
    stop("ERROR writing to socket");

  // Wait and receive the packet back from the server. If n == -1, it failed.

  server_len = sizeof(server_addr);
  if ((n = recvfrom(sockfd, (char *)&packet, sizeof(ntp_packet), 0, (struct sockaddr *)&server_addr, (socklen_t *)&server_len)) < 0)
  {
    perror("Error recvfrom");
    strcpy(dt, "Error recvfrom");
    return;
  }
  // These two fields contain the time-stamp seconds as the packet left the NTP server.
  // The number of seconds correspond to the seconds passed since 1900.
  // ntohl() converts the bit/byte order from the network's to host's "endianness".

  packet.txTm_s = ntohl(packet.txTm_s); // Time-stamp seconds.
  packet.txTm_f = ntohl(packet.txTm_f); // Time-stamp fraction of a second.

  // Extract the 32 bits that represent the time-stamp seconds (since NTP epoch) from when the packet left the server.
  // Subtract 70 years worth of seconds from the seconds since 1900.
  // This leaves the seconds since the UNIX epoch of 1970.
  // (1900)------------------(1970)**************************************(Time Packet Left the Server)

  time_t txTm = (time_t)(packet.txTm_s - NTP_TIMESTAMP_DELTA);

  // Print the time we got from the server, accounting for local timezone and conversion from UTC time.

  sprintf(dt, "%s", ctime((const time_t *)&txTm));
  dt[strlen(dt) - 1] = '\0';

  close(sockfd);
  return;
}
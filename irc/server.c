#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <netinet/in.h>
#include <netdb.h>
 
#define BUFSIZE 1024
#define NTP_TIMESTAMP_DELTA 2208988800ull
#define PORT 8000
#define MAX_UNAME_LENGTH 32
#define MAX_PWORD_LENGTH 32
#define MAX_MSG_LENGTH 4096
#define MAX_COMMAND_LENGTH 32
#define INVALID_NUMBER_ARGS "\033[0;31minvalid numbers of arguments.\033[0m"
#define INVALID_COMMAND "\033[0;31minvalid command.\033[0m"
#define INVALID_UNAME "\033[0;31mplease enter a valid pseudo.\033[0m"
#define INVALID_PWORD "\033[0;31mplease enter a valid password.\033[0m"
#define WRONG_PWORD "\033[0;31mwrong password.\033[0m"
#define INVALID_FILENAME "\033[0;31minvalid filename.\033[0m"
#define COMMAND_NOT_FOUND  "\033[0;31mcommand not found.\033[0m"
#define NRECORDS (100)
#define bp printf("breakpoint\n");

struct Status;

typedef struct Nickname{
  char uname[MAX_UNAME_LENGTH + 1];
  char pword[MAX_PWORD_LENGTH + 1];
  struct Status *status;
  struct Nickname *next;
} Nickname;

typedef struct {
  Nickname *head;
} NicknameList;


typedef struct{
  char uname[MAX_UNAME_LENGTH + 1];
  char pword[MAX_PWORD_LENGTH + 1];
} RECORD;

typedef struct Client {
  int fd;
  Nickname *nickname;
  int logged;
  struct Client *next;
} Client;

typedef struct {
  Client *head;
} List;

typedef struct Status{
  int registered;
  int logged;
} Status;

void ntp_request(char *);
void save_nicknames(NicknameList);
void add_record(Nickname);
void print_nickname_list(NicknameList);

void 
stop(char *msg)
{
  perror(msg);
  exit(EXIT_FAILURE);
}

int 
check_pword_valididy(char *pword) {
  if ( !pword )
    return 0;
  else if ( strlen(pword) > MAX_PWORD_LENGTH )
    return 0;
  else 
    return 1;
}

int 
check_uname_valididy(char *uname) {
  if ( !uname )
    return 0;
  else if ( strlen(uname) > MAX_UNAME_LENGTH )
    return 0;
  else 
    return 1;
}

int 
is_nickname_used(NicknameList l, char *uname) {
  Nickname *tempNickname = l.head;
  while( tempNickname != NULL ) {
    if ( strcmp(tempNickname->uname, uname) == 0 ) {
      return 1;
    }
    tempNickname = tempNickname->next;
  }
  return 0; 
}

Nickname * 
add_nickname(NicknameList *l, char *uname)
{
  Nickname *new_nickname = (Nickname *)malloc(sizeof(Nickname));
  
  strcpy(new_nickname->uname, uname);
  strcpy(new_nickname->pword, "");

  Status *new_status = (Status *)malloc(sizeof(Status));
  new_status->logged = 0;
  new_status->registered = 0;

  new_nickname->status = new_status;

  new_nickname->next = l->head;
  l->head = new_nickname;

  return new_nickname;
}

void 
add_new_client(List *l, NicknameList *nickname_list, int fd, char *uname)
{
  Client *new_client = (Client *)malloc(sizeof(Client));
  new_client->fd = fd;
  new_client->nickname = add_nickname(nickname_list, uname);
  new_client->logged = 0;
  new_client->next = l->head;
  l->head = new_client;
}

Client *
get_client(List l, char *uname) {
  Client *c = l.head;
  while ( c != NULL ) 
  {
    if ( strcmp(c->nickname->uname, uname) == 0 )
    {
      return c;
    }
    c = c->next;
  } 
  return NULL;
}

Nickname * 
get_nickname(NicknameList l, char *uname) {
  Nickname *tempNickname = l.head;
  while( tempNickname != NULL ) {
    if ( strcmp(tempNickname->uname, uname) == 0 )
      return tempNickname;
    tempNickname = tempNickname->next;
  }
  return NULL;
}

void 
register_nickname(Nickname *nickname, char *pword) {
  nickname->status->registered = 1;
  strcpy(nickname->pword, pword);
}

void 
unregister_nickname(Nickname *nickname) {
  nickname->status->registered = 0;
}

void 
delete_nickname(NicknameList *l, char *uname) 
{
  Nickname *temp = l->head, *prev;

  // If head node itself holds the key to be deleted
  if (temp != NULL && strcmp(temp->uname, uname) == 0 )
  {
    l->head = temp->next; // Changed head
    free(temp);           // free old head
    return;
  }

  // Search for the key to be deleted, keep track of the
  // previous node as we need to change 'prev->next'
  while (temp != NULL && strcmp(temp->uname, uname) == 0 )
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

void 
delete_client_socket(List *l, int fd)
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

void 
disconnect_user(int sd, struct sockaddr_in servaddr, int addrlen, List *client_socket)
{
  getpeername(sd, (struct sockaddr *)&servaddr, (socklen_t *)&addrlen);
  printf("\033[0;33mHost disconnected\033[0;m , ip %s , port %d, fd %d\n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port), sd);

  // Close the socket and mark as 0 in list for reuse
  close(sd);
  delete_client_socket(client_socket, sd);
}

void 
print_client_list(List l)
{
  Client *c = l.head;
  printf("head: %p\n", l.head);
  while (c != NULL)
  {
    printf("addr: %p fd: %d uname: %s nickname: %p next: %p\n", c, c->fd, c->nickname->uname, c->nickname, c->next);
    c = c->next;
  }
}

void 
print_nickname_list(NicknameList l)
{
  Nickname *c = l.head;
  printf("head: %p\n", l.head);
  while (c != NULL)
  {
    printf("addr: %p  uname: %s  pword: %s  status.registered: %d  status.logged: %d  next: %p\n", c, c->uname, c->pword, c->status->registered, c->status->logged, c->next);
    c = c->next;
  }
}

int 
is_command(char *p)
{
  return *p == '/';
}

int 
get_argc(char *p)
{
  int c = 0;
  for (; *p != '\0'; p++)
    if (*p == ' ')
      c++;
  return c + 1;
}

void 
send_message_to_all(List client_socket, char *msg)
{
  Client *tempClient = client_socket.head;
  while (tempClient != NULL)
  {
    send(tempClient->fd, msg, strlen(msg), 0);
    tempClient = tempClient->next;
  }
  return;
}

void 
load_nicknames(NicknameList *nickname_list) {
  RECORD records;
  FILE *fp;
  fp = fopen("records.dat","rb");
  if( fp == NULL ) 
  {
    fp = fopen("records.dat","a");
    fclose(fp);
    printf("\033[0;32mrecords.dat created.\033[0;m\n");
    return;
  }
  fseek(fp, 0, SEEK_SET);
  Nickname *tempNickname = NULL;

  while ( fread(&records, sizeof(records), 1, fp) == 1 ) 
  {  
    printf("will load: %s %s \n", records.uname, records.pword);
    tempNickname = add_nickname(nickname_list, records.uname);
    register_nickname(tempNickname, records.pword);
  }
 
  fclose(fp);
  return;
}

void
add_record(Nickname n) {
  RECORD record;
  FILE *fp;
  fp = fopen("records.dat","r+");
  if( fp == NULL ) exit(1);
  fseek(fp, 0, SEEK_END);  
  
  sprintf(record.uname, "%s", n.uname);
  sprintf(record.pword, "%s", n.pword);
  // strcpy(record.uname, n.uname);
  // strcpy(record.pword, n.pword);
  printf("will save: %s %s\n", record.uname, record.pword);
  fwrite(&record, sizeof(record), 1, fp);
  fclose(fp);
  return;
}

void 
save_nicknames(NicknameList nickname_list) {
  RECORD record;
  FILE *fp;
  fp = fopen("records.dat","w+");
  if( fp == NULL ) exit(1);
  fseek(fp, 0, SEEK_SET);
  Nickname *tempNickname = nickname_list.head;
  while( tempNickname != NULL ) 
  {
    if( tempNickname->status->registered == 1 ) 
    {
      sprintf(record.uname, "%s", tempNickname->uname);
      sprintf(record.pword, "%s", tempNickname->pword);
      printf("will save: %s %s\n", record.uname, record.pword);
      fwrite(&record, sizeof(record), 1, fp);
      tempNickname = tempNickname->next;
    }
    tempNickname = tempNickname->next;
  }
  fclose(fp);
  return;
}

int main(int argc , char *argv[])
{
    int opt = 1;
    int master_socket , addrlen , new_socket , activity, valread , sd, pword_len;
	  int max_sd;
    char *token;
    struct sockaddr_in address;

    char uname[MAX_UNAME_LENGTH + 1];
    char msg[MAX_MSG_LENGTH + 1];
    char command[MAX_COMMAND_LENGTH + 1];
    char dt[50];
    bzero(&dt, 50);
     
    char buffer[BUFSIZE + 1];  //data buffer of 1K

    List client_socket;
    client_socket.head = NULL;
     
    NicknameList nickname_list;
    nickname_list.head = NULL;

    //set of socket descriptors
    fd_set readfds;
     
    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) 
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
 
    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
 
    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons( PORT );
     
    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
	  printf("Listener on port %d. \n", PORT);
	
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
     
    //accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");
    
    //load records  
    printf("Loading registered pseudos ...\n");
    load_nicknames(&nickname_list);
    printf("Pseudos loaded.\n");

	while(1) {
        //clear the socket set
        FD_ZERO(&readfds);
 
        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;
		
        //add child sockets to set
        Client *currentClient = client_socket.head;
        while (currentClient != NULL)
        {
          sd = currentClient->fd;
          if (sd > 0)
            FD_SET(sd, &readfds);

          // highest file descriptor number, need it for the select function
          if (sd > max_sd)
            max_sd = sd;

          currentClient = currentClient->next;
        }
 
        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);
   
        if ((activity < 0) && (errno!=EINTR)) 
        {
            printf("select error");
        }
         
        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) 
        { 
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }
         
            //inform user of socket number - used in send and receive commands
            printf("\033[0;33mNew connection\033[0;m , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
            
            //add new client
            //generate a temp uname
            bzero(uname, MAX_UNAME_LENGTH + 1);
            do
            {
              strcpy(uname, "XXXXXXXXXXXX");
              mktemp(uname);
            } while ( is_nickname_used(nickname_list, uname) == 1 );
            
            add_new_client(&client_socket, &nickname_list, new_socket, uname);

            // Send greeting message
            bzero(msg, MAX_MSG_LENGTH + 1);
            sprintf(msg, "Welcome \033[0;32m%s\033[0;m!", uname);
            send( new_socket, msg, strlen(msg) , 0 );
        }
         
        //else its some IO operation on some other socket :)
        currentClient = client_socket.head;
        while( currentClient != NULL )
        {
            bzero(msg, MAX_MSG_LENGTH + 1);
            sd = currentClient->fd;
             
            if (FD_ISSET( sd , &readfds)) 
            {
                bzero(&buffer, BUFSIZE + 1);
                //Check if it was for closing , and also read the incoming message
                if ((valread = read( sd , buffer, BUFSIZE)) == 0)
                {
                    //Close the socket and mark as 0 in list for reuse
                    if ( currentClient->nickname->status->registered == 0 )
                    {
                      delete_nickname(&nickname_list, currentClient->nickname->uname);
                    }
                    if ( currentClient->logged == 1 )
                    {
                      currentClient->logged = 0;
                      currentClient->nickname->status->logged = 0;
                    }
                    currentClient = currentClient->next;
                    disconnect_user(sd, address, addrlen, &client_socket);
                    print_nickname_list(nickname_list);
                    continue;
                }
                 
                else
                {
                    //set the string terminating NULL byte on the end of the data read
                    buffer[valread] = '\0';
                    argc  = get_argc(buffer);

                    if ( is_command(buffer) ) 
                    {
                      
                      bzero(&command, MAX_COMMAND_LENGTH + 1);
                      token = strtok(buffer, " ");
                      strncpy(command, token, strlen(token));
                      // printf("%s %d argc: %d\n", command, strlen(command), argc);
                      if ( argc > 3 ) {
                        
                        int r = send(sd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                        if ( r < 1 ) 
                          perror("error send");
                      } 
                      else 
                      {
                        if ( strncmp(command, "/exit", 4) == 0 ) 
                        {
                          if (argc != 1) 
                          {
                            send(sd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                          } 
                          else 
                          { 
                            if ( currentClient->nickname->status->registered == 0 )
                            {
                              delete_nickname(&nickname_list, currentClient->nickname->uname);
                            }
                            if ( currentClient->logged == 1 )
                            {
                              currentClient->logged = 0;
                              currentClient->nickname->status->logged = 0;
                            }
                            currentClient = currentClient->next;
                            disconnect_user(sd, address, addrlen, &client_socket);
                            print_nickname_list(nickname_list);
                            continue;
                          }
                          
                        }
                        else if ( strncmp(command, "/offer", 5) == 0 ) 
                        {
                          int s = 1;
                          if (argc != 3)
                          {
                            sprintf(msg, INVALID_NUMBER_ARGS);
                            
                          }
                          else
                          {
                            // get uname
                            token = strtok(NULL, " ");
                            bzero(&uname, MAX_UNAME_LENGTH + 1);
                            strncpy(uname, token, strlen(token)+1);

                            if ( check_uname_valididy(uname) == 0 ) 
                            {
                              sprintf(msg, INVALID_UNAME);
                            } 
                            else 
                            {
                              Client *tempClient = get_client(client_socket, uname);

                              // check if client exist
                              if ( tempClient == NULL ) 
                              {
                                sprintf(msg, INVALID_UNAME);
                              }
                              else 
                              {
                                // get filename
                                token = strtok(NULL, " ");
                                if ( !token )
                                {
                                  sprintf(msg, INVALID_FILENAME);
                                } 
                                else
                                {
                                  getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                                  sprintf(msg, "\033[0;32mfile offer from %s. use /download \033[0;33m%s\033[0;m \033[0;32mto accept offer and download file.\033[0;m", currentClient->nickname->uname, token);
                                  send(tempClient->fd, msg, strlen(msg), 0);
                                  bzero(&msg, MAX_MSG_LENGTH + 1);
                                  sprintf(msg,"\033[0;32mSent offer request of file %s to %s.\033[0;m", token, tempClient->nickname->uname);
                                  send(sd, msg, strlen(msg), 0);
                                  s = 0;
                                }
                              }
                            }
                          }
                          if ( s )
                            send(sd, msg, strlen(msg), 0);       
                        }
                        else if ( strncmp(command, "/logout", 7) == 0 ) 
                        {
                          if (argc != 1)
                          {
                            sprintf(msg, INVALID_NUMBER_ARGS);
                          }
                          else if ( currentClient->logged == 1 )
                          {
                            currentClient->nickname->status->logged = 0;
                            currentClient->logged = 0;
                            sprintf(msg, "\033[0;32myou are no more logged as %s.\033[0m", currentClient->nickname->uname);
                          }
                          else
                          {
                            sprintf(msg, "\033[0;31myou are not logged.\033[0m");
                          }
                          send(sd, msg, strlen(msg), 0);
                        }
                        else if ( strncmp(command, "/login", 6) == 0 ) 
                        {
                          if (argc != 3)
                          {
                            send(sd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                          } 
                          else if (currentClient->logged == 1) 
                          {
                            send(sd, "\033[0;31myou are already logged.\033[0;m", strlen("\033[0;31myou are already logged.\033[0;m"), 0);
                          }
                          else
                          {
                            // get uname
                            token = strtok(NULL, " ");
                            bzero(&uname, MAX_UNAME_LENGTH + 1);
                            strncpy(uname, token, strlen(token));

                            if ( check_uname_valididy(uname) == 1 )
                            {
                              // get password
                              token = strtok(NULL, " ");

                              if ( check_pword_valididy(token) ==  1)
                              {
                                Nickname *tempNickname = get_nickname(nickname_list, uname);

                                if ( tempNickname == NULL ) 
                                {
                                  sprintf(msg, "\033[0;31muser not found.\033[0m");
                                } 
                                else if ( tempNickname->status->logged == 1 )
                                { 
                                  sprintf(msg, "\033[0;31man user is already logged with that pseudo.\033[0m");
                                }
                                else if ( tempNickname->status->registered == 0 )
                                {
                                  sprintf(msg, "\033[0;31mplease enter a registered pseudo.\033[0m");
                                }
                                else 
                                {
                                  if ( strcmp(tempNickname->pword, token) == 0 )
                                  {
                                    // finally log client

                                    // delete old nickname
                                    if ( currentClient->nickname->status->registered == 0 )
                                        delete_nickname(&nickname_list, currentClient->nickname->uname);
                                    
                                    // log client
                                    currentClient->nickname = tempNickname;
                                    tempNickname->status->logged = 1;
                                    currentClient->logged = 1;
                                    sprintf(msg, "\033[0;32myou are now logged as %s. Welcome %s!\033[0m", currentClient->nickname->uname, currentClient->nickname->uname);
                                  }
                                  else
                                  {
                                    sprintf(msg, WRONG_PWORD);
                                  }
                                }
                              }
                              else 
                              {
                                sprintf(msg, INVALID_PWORD);
                              }
                            }
                            else
                            {
                              sprintf(msg, INVALID_UNAME);
                            }              
                            send(sd, msg, strlen(msg), 0);
                          }
                        }
                        else if ( strncmp(command, "/unregister", 11) == 0 )
                          {
                            if (argc != 3)
                            {
                              send(sd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                            }
                            else
                            {
                              // get uname
                              token = strtok(NULL, " ");
                              bzero(&uname, MAX_UNAME_LENGTH + 1);
                              strncpy(uname, token, strlen(token));

                              if ( check_uname_valididy(uname) == 1 )
                              {

                                // get password
                                token = strtok(NULL, " ");

                                if ( check_pword_valididy(token) ==  1)
                                {

                                  Nickname *tempNickname = get_nickname(nickname_list, uname);
                              
                                  // nickname exist?
                                  if ( tempNickname == NULL ) 
                                  { 
                                    sprintf(msg, "\033[0;31m%s is not a registered pseudo.\033[0m", uname);
                                  }
                                  else if ( tempNickname->status->registered == 0 ) 
                                  {
                                    sprintf(msg, "\033[0;31m%s is not a registered pseudo.\033[0m", uname);
                                  }
                                  else
                                  { 
                                    
                                    if ( tempNickname->status->logged == 1 ) 
                                    {
                                      if ( strcmp(currentClient->nickname->uname, tempNickname->uname) == 0 ) 
                                      {
                                        sprintf(msg, "\033[0;31myou must logout first.\033[0m");
                                      }
                                      else 
                                      {
                                        sprintf(msg, "\033[0;31manother client is currently logged with this %s pseudo.\033[0m", tempNickname->uname);
                                      }
                                    }
                                    else 
                                    {
                                      if ( strcmp(tempNickname->pword, token) == 0 )
                                      {
                                        // finally unregister nickname
                                        unregister_nickname(tempNickname);
                                        save_nicknames(nickname_list);
                                        sprintf(msg, "\033[0;32m%s pseudo unregistered.\033[0m", tempNickname->uname);
                                      }
                                      else
                                      {
                                        sprintf(msg, WRONG_PWORD);
                                      }
                                    }
                                  }
                                }
                                else 
                                {
                                  sprintf(msg, INVALID_PWORD);
                                }
                              } 
                              else
                              {
                                sprintf(msg, INVALID_UNAME);
                              }
                              send(sd, msg, strlen(msg), 0);
                            }
                        } 
                        else if ( strncmp(command, "/register", 9) == 0 )
                        {
                          if (argc != 3)
                          {
                            send(sd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                          }
                          else if ( currentClient->logged == 1) 
                          {
                            send(sd, "\033[0;31mlogout to use this command.\033[0;m", strlen("\033[0;31mlogout to use this command.\033[0;m"), 0);
                          }
                          else
                          {
                            // get uname
                            token = strtok(NULL, " ");
                            bzero(&uname, MAX_UNAME_LENGTH + 1);
                            strncpy(uname, token, strlen(token)+1);

                            if ( check_uname_valididy(uname) == 1 )
                            {

                              // get password
                              token = strtok(NULL, " ");
                              pword_len = strlen(token);
                              token[pword_len] = '\0';

                              if ( check_pword_valididy(token) ==  1)
                              {

                                Nickname *tempNickname = get_nickname(nickname_list, uname);

                                // check if pseudo is already used or not
                                if ( tempNickname == NULL ) 
                                { 
                                  // finaly register nickname

                                  // delete old nickname
                                    if ( currentClient->nickname->status->registered == 0 )
                                        delete_nickname(&nickname_list, currentClient->nickname->uname);

                                  tempNickname = add_nickname(&nickname_list, uname);
                                  currentClient->nickname = tempNickname;
                                  register_nickname(tempNickname, token);
                                  add_record(*tempNickname);
                                  sprintf(msg, "\033[0;32mnew nickname. your current nickname is\033[0m \033[0;32m%s\033[0m.\n\033[0;32mnew pseudo registered. use /login pseudo password command to log.\033[0m", currentClient->nickname->uname);
                                }
                                else 
                                {
                                  if ( tempNickname->status->registered == 1 ) 
                                  {
                                    sprintf(msg, "\033[0;31mpseudo already registered.\033[0m");
                                  }
                                  else 
                                  {
                                    print_client_list(client_socket);
                                    if ( strcmp(currentClient->nickname->uname, tempNickname->uname) == 0 ) 
                                    {            
                                      // finally register nickname                      
                                      tempNickname = add_nickname(&nickname_list, uname);
                                      currentClient->nickname = tempNickname;
                                      register_nickname(tempNickname, token);
                                      add_record(*tempNickname);
                                      sprintf(msg, "\033[0;32mnew pseudo registered. use /login pseudo password command to log.\033[0m");
                                    }
                                    else 
                                    {
                                      sprintf(msg, "\033[0;31mthis pseudo is currently used by another user.\033[0m");
                                    }       
                                  }
                                }
                              }
                              else 
                              {
                                sprintf(msg, INVALID_PWORD);
                              }
                            } 
                            else
                            {
                              sprintf(msg, INVALID_UNAME);
                            }
                  
                            send(sd, msg, strlen(msg), 0);
                          }
                        } 
                        else if ( strncmp(command, "/mp", 3) == 0 )
                        {
                          if (argc != 3)
                          {
                            send(currentClient->fd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                          }
                          else
                          {
                            token = strtok(NULL, " ");
                            if ( check_uname_valididy(token) == 1 ) 
                            { 
                              // get uname
                              bzero(&uname, MAX_UNAME_LENGTH + 1);
                              strncpy(uname, token, strlen(token));
                              
                              if ( strcmp(currentClient->nickname->uname, uname) == 0 )
                              {
                                sprintf(msg, INVALID_COMMAND);
                              }
                              else 
                              {
                                Client *tempClient = get_client(client_socket, uname);

                                if ( tempClient == NULL ) {
                                    send(sd, "\033[0;31muser does not exist.\033[0;m", strlen("\033[0;31muser does not exist.\033[0;m"), 0);
                                }
                                else
                                {
                                  // get message
                                  token = strtok(NULL, " ");
                                  sprintf(msg, "\033[0;31m[private message from %s]\033[0m %s", currentClient->nickname->uname, token);

                                  // send message to dest client
                                  send(tempClient->fd, msg, strlen(msg), 0); 
                                }
                              }
                            }
                            else
                            {
                              sprintf(msg, INVALID_UNAME);
                            }
                          }
                        } 
                        else if ( strncmp(command, "/date", 4) == 0 ) 
                        {
                          if (argc != 1)
                          {
                            send(sd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                          }
                          else
                          {
                            ntp_request(dt);
                            sprintf(msg, "\033[0;31m%s\033[0m", dt);
                            send(sd, msg, strlen(msg), 0);
                          }
                        } 
                        else if ( strncmp(command, "/blue", 4) == 0 ) 
                        {
                          if (argc != 2)
                          {
                            send(sd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                          }
                          else
                          {
                            token = strtok(NULL, " ");
                            sprintf(msg, "%s: \033[0;34m%s\033[0m", currentClient->nickname->uname, token);
                            send_message_to_all(client_socket, msg);
                          }
                        } 
                        else if ( strncmp(command, "/red", 3) == 0 ) 
                        {
                          if (argc != 2)
                          {
                            send(currentClient->fd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                          }
                          else
                          {
                            token = strtok(NULL, " ");
                            sprintf(msg, "%s: \033[0;31m%s\033[0m", currentClient->nickname->uname, token);
                            send_message_to_all(client_socket, msg);
                          }
                        } 
                        else if ( strncmp(command, "/green", 5) == 0 )
                        {
                          if (argc != 2)
                          {
                            send(currentClient->fd, INVALID_NUMBER_ARGS, strlen(INVALID_NUMBER_ARGS), 0);
                          }
                          else
                          {
                            token = strtok(NULL, " ");
                            sprintf(msg, "%s: \033[0;32m%s\033[0m", currentClient->nickname->uname, token);
                            send_message_to_all(client_socket, msg);
                          }
                        } 
                        else if ( strncmp(command, "/alert", 5) == 0 ) {
                            if (argc == 2)
                            { 
                              // get message
                              token = strtok(NULL, " ");
                              sprintf(msg, "\a%s: %s", currentClient->nickname->uname, token);

                              // send alerted message to all clients
                              send_message_to_all(client_socket, msg);
                            }
                            else if (argc == 3)
                            {
                              // get pseudo
                              token = strtok(NULL, " ");
                              bzero(&uname, MAX_UNAME_LENGTH + 1);
                              strncpy(uname, token, strlen(token));

                              if ( check_uname_valididy(uname) == 1)  
                              {
                                if (strcmp(currentClient->nickname->uname, uname) == 0)
                                {
                                  send(sd, INVALID_COMMAND, strlen(INVALID_COMMAND), 0);
                                }
                                else 
                                {
                                  Client *tempClient = get_client(client_socket, uname);

                                  if ( tempClient == NULL ) 
                                  {
                                    send(sd, "\033[0;31muser does not exist.\033[0;m", strlen("\033[0;31muser does not exist.\033[0;m"), 0);
                                  }
                                  else
                                  {
                                    token = strtok(NULL, " ");
                                    sprintf(msg, "\a\033[0;31m[alerted message from %s]\033[0m %s: %s", currentClient->nickname->uname, currentClient->nickname->uname, token);
                                    send(tempClient->fd, msg, strlen(msg), 0);
                                  }
                                }
                              } 
                              else 
                              {
                                send(sd, INVALID_UNAME, strlen(INVALID_UNAME), 0);
                              }
                            }
                        } 
                        else if ( strncmp(command, "/nickname", 9) == 0 ) 
                        {
                          if ( argc == 1 ) 
                          {
                            if ( currentClient->nickname->status->registered == 1 )
                              sprintf(msg, "your current nickname is \033[0;36m%s\033[0m.[registered]", currentClient->nickname->uname);
                            else
                              sprintf(msg, "your current nickname is \033[0;31m%s\033[0m.[current]", currentClient->nickname->uname);
                          } 
                          else 
                          {
                            // get pseudo
                            token = strtok(NULL, " ");

                            //check pseudo validity
                            if ( check_uname_valididy(token) == 1 ) 
                            {
                              if ( argc == 2 ) 
                              {  
                                bzero(&uname, MAX_UNAME_LENGTH + 1);
                                strncpy(uname, token, strlen(token));

                                if ( currentClient->logged == 0 ) 
                                {
                                  
                                    Nickname *tempNickname = get_nickname(nickname_list, uname);

                                    if ( tempNickname == NULL ) 
                                    {
                                      // finally change current nickname
                                      if ( currentClient->nickname->status->registered == 0 )
                                        delete_nickname(&nickname_list, currentClient->nickname->uname);
                                      tempNickname= add_nickname(&nickname_list, uname);
                                      currentClient->nickname = tempNickname;
                                      
                                      sprintf(msg, "\033[0;32mnew nickname. your current nickname is\033[0m \033[0;32m%s\033[0m.", currentClient->nickname->uname);
                                    } 
                                    else if ( tempNickname->status->registered == 1)
                                    {
                                      sprintf(msg, "\033[0;31mpseudo is registered.\033[0m \033[0;32muse /login pseudo password to log in.\033[0m ");
                                    }
                                    else
                                    {
                                      sprintf(msg, "\033[0;31mpseudo already used.\033[0m");
                                    }
                                } 
                                else 
                                {
                                  sprintf(msg, "\033[0;31myour are logged, use /nickname pseudo password command.\033[0m");
                                }
                              } 
                              else 
                              {

                                bzero(&uname, MAX_UNAME_LENGTH + 1);
                                strncpy(uname, token, strlen(token));

                                if ( currentClient->logged == 1 ) 
                                {
                                  // get password
                                  token = strtok(NULL, " ");

                                  if ( strcmp(currentClient->nickname->pword, token) == 0 ) 
                                  {
                                    sprintf(msg, "\033[0;32mrename %s by %s.\033[0m", currentClient->nickname->uname, uname);
                                    strcpy(currentClient->nickname->uname, uname);
                                   
                                  }
                                  else
                                  {
                                    sprintf(msg, "\033[0;31mwrong password.\033[0m");
                                  }

                                }
                                else
                                {
                                  sprintf(msg, "\033[0;31myou must be logged to use /nickname pseudo password command.\033[0m");
                                }
                              }
                            } 
                            else 
                            {
                              sprintf(msg, "\033[0;31mplease enter a valid pseudo.\033[0m");
                            }    
                          }
                          send(sd, msg, strlen(msg), 0);
                        } 
                        else 
                        {
                          send(sd , COMMAND_NOT_FOUND , strlen(COMMAND_NOT_FOUND) , 0 );
                        }
                      } 
                    }
                    //Echo back the message that came in 
                    else 
                    {
                      sprintf(msg, "%s: %s", currentClient->nickname->uname, buffer);
                      send_message_to_all( client_socket , msg );
                    }                    
                }
            }
          currentClient = currentClient->next;
        }
    }
    return EXIT_SUCCESS;
}

void 
ntp_request(char *dt)
{
  int sockfd, n; // Socket file descriptor and the n return result from writing/reading from the socket.

  int port = 123; // NTP UDP port number.
  int server_len;
  struct timeval tv = {
      .tv_sec = 1
  };

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
    sprintf(dt, "Error no host");
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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

/* function to make the server exit */
void run_exit(){
  printf("Server exiting...\n");
  exit(0);
}

/* funtion to send a file to the client */
void send_file(char *filename, int sockfd, struct sockaddr_in clientaddr){

    socklen_t clientlen = sizeof(clientaddr);
    FILE *filep = fopen(filename,"rb");
    int n;

    /* check if the file is present */
    if(!filep){
      printf("File not found in server\n");
      const char *msg = "File not found";
      n = sendto(sockfd, (const char *) msg, strlen(msg), 0, (struct sockaddr *) &clientaddr, clientlen);
    }
    else{
      const char *msg = "File found";
      n = sendto(sockfd, (const char *) msg, strlen(msg), 0, (struct sockaddr *) &clientaddr, clientlen);

      /* structure to hold the packet information and the data */
      typedef struct _data_packet {
            uint32_t packet_no;
            uint32_t packet_size;
            char netbuf[BUFSIZE];
        } data_packet;

      data_packet packet;
      bzero(packet.netbuf, sizeof(packet.netbuf)); 

      packet.packet_no = 0;
      int recv_packet_no = -1;

      /* Loop through the file and send data packets */
      while(!feof(filep)) {
          packet.packet_no ++;
          packet.packet_size = fread(packet.netbuf, 1, BUFSIZE - 1, filep);

          /* keep sending the same packet till it reaches the client and the client returns the same packet no */
          while(1){
            n = sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &clientaddr, clientlen);
            n = recvfrom(sockfd, &recv_packet_no, sizeof(int), 0, (struct sockaddr *) &clientaddr, &clientlen);
            if(recv_packet_no == packet.packet_no) break;
          }
      }
      fclose(filep);
      packet.packet_size = -1;

      /* send a packet with size -1 so that the client knows that the server is done sending */
      n = sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &clientaddr, clientlen);
      printf("File sent to client...\n");
    }
}

/* function to receive a file */
void receive_file(char *filename, int sockfd, struct sockaddr_in clientaddr){
    socklen_t clientlen = sizeof(clientaddr);
    char netbuf[BUFSIZE];
    bzero(netbuf, sizeof(netbuf)); 
    int n;

    /* check reply from the client to see if the file is present in the client */
    n = recvfrom(sockfd, netbuf, sizeof(netbuf), 0, (struct sockaddr *)&clientaddr, &clientlen);
    if(strcmp(netbuf,"File not found") == 0)
        printf("File not found on client\n");
    else{
        FILE *filep = fopen(filename,"wb");

        /* structure to hold the data packets to be received */
        typedef struct _data_packet {
            uint32_t packet_no;
            uint32_t packet_size;
            char netbuf[BUFSIZE];
        } data_packet;

        data_packet packet;
        int packet_count = 0;

        /* keep receiving the packets till the client sends a packet of size -1 */
        while(1) {
            
            n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&clientaddr, &clientlen);
            if (packet.packet_size == -1) {
                break;
            }
            /* if it is the expected packet, write to file and send the updated packet no to the client */
            if((packet_count+1) == packet.packet_no){
                if(packet.packet_size <= 1024 && packet.packet_size > 0){
                    fwrite(packet.netbuf, 1, packet.packet_size, filep);
                    packet_count++;
                    n = sendto(sockfd, &packet_count, sizeof(int), 0, (struct sockaddr *) &clientaddr, clientlen);
                }
            }
            /* else send the old packet no */
            else{
                n = sendto(sockfd, &packet_count, sizeof(int), 0, (struct sockaddr *) &clientaddr, clientlen);
            }
        }
        fclose(filep);
        printf("File transfer complete\n");
    }
}

/* function to get the list of files and store it in a file */
void run_ls(int sockfd, struct sockaddr_in clientaddr){
  system("ls | grep -v \"ls.out\" > ls.out");
  send_file("ls.out", sockfd, clientaddr);
  system("rm ls.out");
}

/* function to delete a file if it is present */
void run_delete(char *filename, int sockfd, struct sockaddr_in clientaddr){
  socklen_t clientlen = sizeof(clientaddr);
  int n;

  /* if file is not present, send failure status to the client */
  if(remove(filename)){
    printf("File not found\n");
    const char *netbuf = "File not found";
    n = sendto(sockfd, (const char *) netbuf, strlen(netbuf), 0, (struct sockaddr *) &clientaddr, clientlen);
  }
  /*else, delete it and send success status to the client */
  else {
    printf("File deleted successfully\n");
    const char *netbuf = "File deleted successfully";
    n = sendto(sockfd, (const char *) netbuf, strlen(netbuf), 0, (struct sockaddr *) &clientaddr, clientlen);
  }
}

/* function to handle invalid commands */
void run_not_found(int sockfd, struct sockaddr_in clientaddr){
  socklen_t clientlen = sizeof(clientaddr);
  int n;
  printf("Invalid command\n");
  const char *netbuf = "Invalid command";
  /* notify the client that the command is invalid */
  n = sendto(sockfd, (const char *) netbuf, strlen(netbuf), 0, (struct sockaddr *) &clientaddr, clientlen);
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  socklen_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  char *command;
  char *filename;
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  FILE *filep;
  
  /* check command line arguments */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* socket: create the parent socket */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

  /* build the server's Internet address */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* bind: associate the parent socket with a port */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* main loop: wait for a datagram and process commands */
  clientlen = sizeof(clientaddr);
  
  while (1) {

    printf("Server is up...\n");
    /* recvfrom: receive a UDP datagram from a client */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    /* gethostbyaddr: determine who sent the datagram */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");

    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");

    printf("Command received from client: %s\n", buf);

    /* ls */
    if (strcmp(buf, "ls\n") == 0){
      run_ls(sockfd, clientaddr);
    } 
    /* exit */
    else if(strcmp(buf, "exit\n") == 0){
      printf("Server exiting...\n");
      exit(0);
    }
    /* get, put, delete */
    else {
      command = strtok(buf," ");
      filename = strtok(NULL, "\n");
      if (strcmp(command, "get") == 0) {
        send_file(filename, sockfd, clientaddr);
      }
      else if(strcmp(command, "put") == 0){
        receive_file(filename, sockfd, clientaddr);
      }
      else if(strcmp(command, "delete") == 0){
        run_delete(filename, sockfd, clientaddr);
      }
      /* invalid command */
      else {
        run_not_found(sockfd, clientaddr);
      }
    }
  }
}

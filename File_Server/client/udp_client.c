/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

/* Function to send a file */
void send_file(char *filename, int sockfd, struct sockaddr_in serveraddr){

    socklen_t serverlen = sizeof(serveraddr);
    int n;
    FILE *filep = fopen(filename,"rb");

    /* check if the file is present */
    if(!filep){
      printf("File not found\n");
      const char *msg = "File not found";
      n = sendto(sockfd, (const char *) msg, strlen(msg), 0, (struct sockaddr *) &serveraddr, serverlen);
    }
    else{
        const char *msg = "File found";
        n = sendto(sockfd, (const char *) msg, strlen(msg), 0, (struct sockaddr *) &serveraddr, serverlen);

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

            /* Keep resending the same packet till it reaches the server and the server returns the same packet no */
            while(1){
                n = sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &serveraddr, serverlen);
                n = recvfrom(sockfd, &recv_packet_no, sizeof(int),0 ,(struct sockaddr *)&serveraddr, &serverlen);
                if(recv_packet_no == packet.packet_no) break;
            }
        }
        fclose(filep);

        /* send a packet with size -1 so that the server knows that client is done sending */
        packet.packet_size = -1;
        n = sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &serveraddr, serverlen);
        printf("File sent to server...\n");
    }
}

/* Function to receive a file */
void receive_file(char *filename, int sockfd, struct sockaddr_in serveraddr){
    socklen_t serverlen = sizeof(serveraddr);
    char netbuf[BUFSIZE];
    bzero(netbuf, sizeof(netbuf)); 
    int n;

    /* Check reply from the server to see if the file is present in the server */ 
    n = recvfrom(sockfd, netbuf, sizeof(netbuf), 0, (struct sockaddr *)&serveraddr, &serverlen);
    if(strcmp(netbuf,"File not found") == 0)
        printf("Reply from server: %s\n", netbuf);
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

        /* Keep receiving packets till the server sends a packet of size -1 */
        while(1) {

            n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (packet.packet_size == -1) {
                break;
            }
            /* If it is the expected packet, write to file and send the updated packet no to the server */
            if((packet_count+1) == packet.packet_no){
                if(packet.packet_size <= 1024 && packet.packet_size > 0){
                    fwrite(packet.netbuf, 1, packet.packet_size, filep);
                    packet_count++;
                    n = sendto(sockfd, &packet_count, sizeof(int), 0, (struct sockaddr *) &serveraddr, serverlen);
                }
            }
            /* else send the old packet no */
            else{
                n = sendto(sockfd, &packet_count, sizeof(int), 0, (struct sockaddr *) &serveraddr, serverlen);
            }
        }
        fclose(filep);
        printf("File transfer complete\n");
    }
}

/* Function to receive the ls file, display and delete it */
void run_ls(int sockfd, struct sockaddr_in serveraddr){
    receive_file("ls.out", sockfd, serveraddr);
    FILE *filep = fopen("ls.out", "rb");
    char buf[BUFSIZE];
    bzero(buf, sizeof(buf));
    printf("\nFiles in server...\n");

    /* Print the contents of the file */
    while(!feof(filep)) {
        fscanf(filep, "%s\n", buf);
        printf("%s\n", buf);
    }
    fclose(filep);
    remove("ls.out");
}

/* Function to check if a delete operation was successful or not */
void run_delete(int sockfd, struct sockaddr_in serveraddr){
    socklen_t serverlen = sizeof(serveraddr);
    char buf[BUFSIZE];
    bzero(buf, sizeof(buf));
    int n;
    n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&serveraddr, &serverlen);
    printf("Reply from server: %s\n", buf);
}

/* Function to handle invalid commands */
void run_not_found(int sockfd, struct sockaddr_in serveraddr){
    socklen_t serverlen = sizeof(serveraddr);
    char buf[BUFSIZE];
    bzero(buf, sizeof(buf));
    int n;
    n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&serveraddr, &serverlen);
    printf("Reply from server: %s\n", buf);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname, *cmd, *filename;
    char command[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    serverlen = sizeof(serveraddr);

    /* keep looping and receive commands */
    while(1) {
        printf("\nPlease enter your commands in the format shown below\n");
        printf("1. get [filename]\n");
        printf("2. put [filename]\n");
        printf("3. delete [filename] \n");
        printf("4. ls\n");
        printf("5. exit\n\n");

        bzero(command, BUFSIZE);
        fgets(command, BUFSIZE, stdin);

        //send command to server
        n = sendto(sockfd, command,strlen(command), 0, (struct sockaddr *) &serveraddr, serverlen);

        if (n < 0) {
            error("ERROR in sending command...\n");
        }
        else {
            printf("Command sent to server\n");

            /* ls */
            if (strcmp(command, "ls\n") == 0){
                run_ls(sockfd, serveraddr);
            } 
            /* exit */
            else if(strcmp(command, "exit\n") == 0){
                printf("Server exited...\n");
            }
            /* get, put, delete */
            else {
                cmd = strtok(command," ");
                filename = strtok(NULL, "\n");

                if(strcmp(cmd, "get") == 0){
                    receive_file(filename, sockfd, serveraddr);
                }
                else if(strcmp(cmd, "put") == 0){
                    send_file(filename, sockfd, serveraddr);
                }
                else if(strcmp(cmd, "delete") == 0){
                    run_delete(sockfd, serveraddr);
                }

                /* invalid command */
                else{
                    run_not_found(sockfd, serveraddr);
                }
            } 
        }      
    }

    return 0;
}

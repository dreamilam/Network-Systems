#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define BUFSIZE 1024

/* supported file extensions */
const char *exts[] = {".html",".txt",".png", ".gif", ".jpg", ".css", ".js"};
const char *content_type[] = {"text/html","text/plain","image/png", "image/gif", "image/jpeg", "text/css", "application/javascript"};

/* structure to hold the request parameters */
struct HTTPRequest {
    char *method;
    char *url;
    char *version;
};

/* structure to hold the response parameters */
struct HTTPHeader {
    int status;
    char *path;
    char *response;
    char *ext;
};

/* check if the extension is supported */
bool check_extension(struct HTTPRequest* http_request, struct HTTPHeader* http_header){
    for(int i = strlen(http_request->url)-1; i >= 0; i--) {
        if(http_request->url[i] == '.') {
            int len = strlen(http_request->url)-i-1;
            http_header->ext = malloc(sizeof(char) * len);
            strcpy(http_header->ext, &http_request->url[i]);
            break;
        }
    }
    for(int i = 0; i<7; i++){
        if(strcmp(exts[i], http_header->ext) == 0){
            return true;
        }
    }
    return false;
}

/* check if the file exists and create the file path if it does*/
bool get_response_path(struct HTTPRequest* http_request, struct HTTPHeader* http_header){
    char *basepath = "www";
    if(strcmp(http_request->url, "/") == 0) {
        strcat(http_request->url, "index.html");
    }

    // check if the file has an extension
    if(strstr(http_request->url, ".") == NULL){
        return false;
    }

    // check if the file extension is supported
    if(!check_extension(http_request, http_header)){
        return false;
    }
    
    http_header->path = malloc(sizeof(char)* (strlen(basepath) + strlen(http_request->url)));
    bzero(http_header->path, sizeof(*http_header->path)); 
    strcat(http_header->path, basepath);
    strcat(http_header->path, http_request->url);

    FILE *fp = fopen(http_header->path, "r");
    if(fp == NULL){
        return false;
    }
    return true;
}

/* create the header response */
void create_response(struct HTTPRequest* http_request, struct HTTPHeader* http_header){
    if(http_header->status == 200){
        
        char *str = " 200 OK:\r\nContent-Type: ";
        char *str1 = "; charset=UTF-8\r\n\r\n";

        int i;
        for(i = 0; i<7; i++){
            if(strcmp(exts[i], http_header->ext) == 0){
                break;
            }
        }
        int len = strlen(http_request->version) + strlen(str) + strlen(content_type[i])+ strlen(str1) + 1;
        http_header->response = malloc(sizeof(char)* len);
        bzero(http_header->response, sizeof(*http_header->response)); 
        strncpy(http_header->response, http_request->version, strlen(http_request->version) - 1);
        strcat(http_header->response, str);
        strcat(http_header->response, content_type[i]);
        strcat(http_header->response, str1);
    }
    else if(http_header->status == 404){
        printf("File not found on server...\n");
        char response[] = "HTTP/1.1 404 Not Found\r\n";
        http_header->response = malloc(sizeof(char)*(strlen(response) + 1));
        strcpy(http_header->response, response);
    }
    else{
        printf("Internal server error...\n");
        char response[] = "HTTP/1.1 500 Internal Server Error\r\n";
        http_header->response = malloc(sizeof(char)*(strlen(response) + 1));
        strcpy(http_header->response, response);
    }
}

/* check if the file exists and create the response */
void create_HTTPResponse(struct HTTPRequest* http_request, struct HTTPHeader* http_header){
    http_header->status = 500;
    bool file_exists = get_response_path(http_request, http_header);
    printf("%s\n", http_request->method);
    if(strcmp(http_request->method, "GET") == 0){
        if(file_exists){
        http_header->status = 200;
        }
        else{
        http_header->status = 404;
        }
    }
    create_response(http_request, http_header);
}

/* handle the request */
void handle_request(int connfd){
    struct HTTPHeader http_header;
    struct HTTPRequest http_request;
    int n;
    char buf[BUFSIZE];
    n = recv(connfd, buf, BUFSIZE, 0);
    char *rest, *temp;

    temp = strtok_r(buf, "\n", &rest);
    temp = strtok_r(temp, " ", &rest);
    http_request.method = malloc(sizeof(char)*(strlen(temp) + 1));
    strcpy(http_request.method, temp);

    temp = strtok_r(NULL, " ", &rest);
    http_request.url = malloc(sizeof(char)*(strlen(temp) + 1));
    strcpy(http_request.url, temp);

    printf("Requested URL: %s\n", http_request.url);

    http_request.version = malloc(sizeof(char)*(strlen(rest) + 1));
    strcpy(http_request.version, rest);

    create_HTTPResponse(&http_request, &http_header);

    // send file
    send(connfd, http_header.response, strlen(http_header.response), 0);
    if(http_header.status == 200){
        FILE *fp;
        fp = fopen(http_header.path, "r");
        while (!feof(fp)) {
        n = fread(buf, 1, BUFSIZE, fp);
        send(connfd, buf, n, 0);
        }
        fclose(fp);
    }
}

/* ctrl-C handler */
void signal_handler(int sig_num)
{
    signal(SIGINT, signal_handler);
    printf("\nServer is Exiting... \n");
    fflush(stdout);
    exit(0);
}

int main(int argc, char **argv){
    int sockfd, connfd;
    int portno = 8080;
    socklen_t clientlen;
    struct sockaddr_in serveraddr;
    struct sockaddr_in clientaddr;
    char buf[BUFSIZE];
    int n, optval;
    char *method, *url, *version;
    pid_t pid;
    
    /* create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) 
        perror("Error opening socket");

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

    /* bind the socket with the port */
    if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) 
        perror("ERROR on binding");
    
    /* listen */
    listen(sockfd, 3);
    printf("server is running and waiting for connections\n");

    signal(SIGINT, signal_handler);

    while(1){
        clientlen = sizeof(clientaddr);
        connfd = accept (sockfd, (struct sockaddr *) &clientaddr, &clientlen);
        pid = fork();

        if(pid == 0){
            handle_request(connfd);
        }
        close(connfd);
    }
}
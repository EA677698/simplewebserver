#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

#define BUFFSIZE 2048
#define WORKERS 3
#define HTML "text/html"
#define TXT "text/plain"
#define PNG "image/png"
#define GIF "image/gif"
#define JPG "image/jpg"
#define ICO "image/x-icon"
#define CSS "text/css"
#define JS "application/javascript"

struct Response{
    char** recHeader;
    int statusCode;
    char replyHeader[BUFFSIZE];
    char* contentType;
    long int contentSize;
    int content;
};

void getContentType(struct Response* response){
    if(strcmp(response->recHeader[1],"/") == 0){
        response->contentType = HTML;
        return;
    }
    char ext[strlen(strrchr(response->recHeader[1], '.')+1)];
    memset(ext,0,sizeof(ext));
    strcpy(ext,strrchr(response->recHeader[1],'.')+1);
    for(int i = 0; ext[i]; i++){
        if(ext[i] < 97){
            ext[i] += 32;
        }
    }

    if(strcmp(ext,"html") == 0){
        response->contentType = HTML;
        return;
    }
    if(strcmp(ext,"txt") == 0){
        response->contentType = TXT;
        return;
    }
    if(strcmp(ext,"png") == 0){
        response->contentType = PNG;
        return;
    }
    if(strcmp(ext,"gif") == 0){
        response->contentType = GIF;
        return;
    }
    if(strcmp(ext,"jpg") == 0){
        response->contentType = JPG;
        return;
    }
    if(strcmp(ext,"ico") == 0){
        response->contentType = ICO;
        return;
    }
    if(strcmp(ext,"css") == 0){
        response->contentType = CSS;
        return;
    }
    if(strcmp(ext,"js") == 0){
        response->contentType = JS;
        return;
    }
    response->statusCode = 400;

}

int checkStatusCode(struct Response* response){
    if(response->statusCode != 200){
        strcat(response->replyHeader, response->recHeader[2]);
        strcat(response->replyHeader, " ");
        char code[3];
        sprintf(code,"%d",response->statusCode);
        strcat(response->replyHeader, code);
        switch (response->statusCode) {
            case 403:
                strcat(response->replyHeader," Forbidden\r\n"
                                             "Content-Length: 13\r\n"
                                             "Content-Type: text/html\r\n\r\n"
                                             "403 Forbidden");
                break;
            case 404:
                strcat(response->replyHeader," Not Found\r\n"
                                             "Content-Length: 13\r\n"
                                             "Content-Type: text/html\r\n\r\n"
                                             "404 Not Found");
                break;
            case 405:
                strcat(response->replyHeader," Method Not Allowed\r\n"
                                             "Content-Length: 22\r\n"
                                             "Content-Type: text/html\r\n\r\n"
                                             "405 Method Not Allowed");
                break;
            case 505:
                strcat(response->replyHeader," HTTP Version Not Supported\r\n"
                                             "Content-Length: 30\r\n"
                                             "Content-Type: text/html\r\n\r\n"
                                             "505 HTTP Version Not Supported");
                break;
            default:
                strcat(response->replyHeader," Bad Request\r\n"
                                             "Content-Length: 15\r\n"
                                             "Content-Type: text/html\r\n\r\n"
                                             "400 Bad Request");
        }
        return 1;
    }
    return 0;
}

void openFile(struct Response* response){
    char url[(strlen(response->recHeader[1]) + 3)];
    memset(url,0,(strlen(response->recHeader[1]) + 3));
    strcpy(url,"www");
    if(strcmp(response->recHeader[1],"/") == 0){
        if(access("www/index.html",F_OK) == 0){
            strcat(url,"/index.html");
        } else{
            strcat(url,"/index.htm");
        }
    } else{
        strcat(url,response->recHeader[1]);
    }
    response->content = open(url, O_RDONLY);
    if(response->content == -1){
        if(errno == EACCES){
            response->statusCode = 403;
        } else{
            response->statusCode = 404;
        }
    }
}

void checkMethod(struct Response *response){
    if(strcmp(response->recHeader[0],"GET") == 0){
        response->statusCode = 200;
    } else{
        response->statusCode = 405;
    }
}

void checkHTTPV(struct Response *response){
    if(!(strcmp(response->recHeader[2],"HTTP/1.1") == 0 || strcmp(response->recHeader[2],"HTTP/1.0") == 0)){
        if(response->statusCode != 200){
            response->statusCode = 400;
        } else{
            response->statusCode = 505;
        }
    }
}

char** getArgs(char string[]){
    char** args = calloc(3,sizeof(char*));
    char* token = strtok(string, " ");
    if (token != NULL) {
        args[0] = malloc(strlen(token));
        strcpy(args[0],token);
        token = strtok(NULL, " ");
        if (token != NULL) {
            args[1] = malloc(strlen(token));
            strcpy(args[1],token);
            token = strtok(NULL, "\r\n");
            if (token != NULL) {
                args[2] = malloc(strlen(token));
                strcpy(args[2],token);
            }
        }
    }
    return args;
}

char* getHeader(char string[]){
    return strtok(string,"\n");
}

int desc;
pid_t children[WORKERS];

void sigint_handler(int signal){
    for(int i = 0; i<WORKERS; i++){
        if(children[i] != 0){
            kill(children[i],SIGTERM);
        }
    }

    while (wait(NULL) > 0);
    close(desc);
    printf("\nServer gracefully closed\n");
    exit(0);
}

int num_children = 0;

void sigchild_handler(int signal) {
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for(int i = 0; i<WORKERS; i++){
            if(children[i] == pid){
                children[i] = 0;
                break;
            }
        }
        num_children--;
    }
}


int main(int argc, char** args){
    if (argc < 2) {
        fprintf(stderr, "Missing port number.\n");
        return 1;
    }
    int port = atoi(args[1]);
    desc = socket(AF_INET, SOCK_STREAM, 0);
    if (desc < 0) {
        fprintf(stderr, "Error creating socket\n");
        return 1;
    }
    signal(SIGINT,sigint_handler);
    signal(SIGCHLD,sigchild_handler);
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
    if (bind(desc, (struct sockaddr*) &server, sizeof(server)) < 0) {
        fprintf(stderr, "Failed to bind with port %d.\n",port);
        return 1;
    }
    pid_t pid = 0;
    printf("Running your server with a port # of %d\n", port);
    listen(desc, 3);
    struct sockaddr_in client_addr;
    int client_fd;
    char BUFF[BUFFSIZE];
    while (1){
        memset(&client_addr, 0, sizeof(client_addr));
        memset(BUFF,0,BUFFSIZE);
        socklen_t client_addr_len = sizeof(client_addr);
        client_fd = accept(desc, (struct sockaddr *)&client_addr, &client_addr_len);
        if(client_fd < 0){
            fprintf(stderr,"Error accepting connection\n");
            continue;
        }
        if(num_children < WORKERS){
            if((pid = fork()) == 0) {
                close(desc);
                if (recv(client_fd, BUFF, BUFFSIZE, 0) < 0) {
                    fprintf(stderr, "Error receiving data from client.\n");
                    _exit(1);
                }
                struct Response *response = malloc(sizeof(struct Response));
                response->recHeader = getArgs(getHeader(BUFF));
                checkMethod(response);
                openFile(response);
                checkHTTPV(response);
                if(checkStatusCode(response)){
                    send(client_fd, response->replyHeader, BUFFSIZE,0);
                    close(client_fd);
                    free(response->recHeader[0]);
                    free(response->recHeader[1]);
                    free(response->recHeader[2]);
                    free(response->recHeader);
                    close(response->content);
                    free(response);
                    _exit(0);
                }
                getContentType(response);
                response->contentSize = lseek(response->content, 0, SEEK_END);
                lseek(response->content, 0 ,SEEK_SET);
                memset(BUFF, 0, BUFFSIZE);
                strcat(BUFF, response->recHeader[2]);
                strcat(BUFF, " 200 OK\r\nContent-Type: ");
                strcat(BUFF, response->contentType);
                strcat(BUFF, "\r\nContent-Length: ");
                char num[20];
                sprintf(num, "%ld", response->contentSize);
                strcat(BUFF, num);
                strcat(BUFF, "\r\n\r\n");
                if (send(client_fd, BUFF, strlen(BUFF),0) == -1) {
                    fprintf(stderr, "Error sending data to client.\nBuffer:\n%s\n", BUFF);
                    _exit(1);
                }
                memset(BUFF, 0, BUFFSIZE);
                size_t bytes_read;
                while ((bytes_read = read(response->content, BUFF, BUFFSIZE)) > 0) {
                    if (send(client_fd, BUFF, bytes_read,0) == -1) {
                        fprintf(stderr, "Error sending data to client.\nBuffer:\n%s\n", BUFF);
                        _exit(1);
                    }
                    memset(BUFF,0,BUFFSIZE);
                }
                close(client_fd);
                free(response->recHeader[0]);
                free(response->recHeader[1]);
                free(response->recHeader[2]);
                free(response->recHeader);
                close(response->content);
                free(response);
                _exit(0);
            } else{
                close(client_fd);
                for(int i = 0; i<WORKERS; i++){
                    if(children[i] == 0){
                        children[i] = pid;
                        break;
                    }
                }
                num_children++;
            }
        }
    }


}
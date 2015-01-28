#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <errno.h>

#define MAX_BUFFER_LENGTH 10001

struct Request
{
    char cmd[128];
    char file[128];
    char arg[MAX_BUFFER_LENGTH];
    char protocol[128];
};

int extractRequest(char *buffer, Request &request)
{
    char *tok, *tmp;
    int i = 0;
    for(tmp = strtok(buffer," ?\n"); tmp; tmp = strtok(NULL," ?\n"))
    {
        if (i == 0)
        {
            strcpy(request.cmd, tmp);
        }
        else if (i == 1)
        {
            strcpy(request.file, tmp+1);
        }
        else if (i == 2)
        {
            strcpy(request.arg, tmp);
        }
        else if (i == 3)
        {
            strcpy(request.protocol, tmp);
        }
        i++;
    }

    if (i == 4)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

void setEnvVar(Request &request)
{
    char *tmp;
    char query_string[MAX_BUFFER_LENGTH];
    char script_name[128];

    setenv("REQUEST_METHOD", request.cmd, 1);
    setenv("QUERY_STRING" , request.arg, 1);
    setenv("SCRIPT_NAME" , request.file, 1);
    setenv("CONTENT_LENGTH", " ", 1);
    setenv("REMOTE_HOST"   , " ", 1);
    setenv("REMOTE_ADDR"   , " ", 1);
    setenv("ANTH_TYPE"     , " ", 1);
    setenv("AUTH_TYPE"     , " ", 1);
    setenv("REMOTE_USER"   , " ", 1);
    setenv("REMOTE_IDENT"  , " ", 1);
}

void processRequest(char *buffer, int sockfd)
{
    Request request;
    int pid;
    char path[MAX_BUFFER_LENGTH];
    extractRequest(buffer, request);
    setEnvVar(request);
    strcpy(path, "/u/cs/100/0016026/public_html/");
    strcat(path, request.file);
    printf("path:[%s]\n", path);
    printf("file:[%s]\n", request.file);
    printf("arg:[%s]\n", request.arg);

    for (int i = 0; request.file[i] != '\0'; i++)
    {
        if (request.file[i] == '.')
        {
            if (request.file[i+1] == 'c' &&
                request.file[i+2] == 'g' &&
                request.file[i+3] == 'i')
            {
                //handle CGI
                write(sockfd, "HTTP/1.1 200 OK\r\n", 17);
                write(sockfd, "Server: rudyhttpd\r\n", 19);
                write(sockfd, "Content-Type: text/html\r\n", 25);
                write(sockfd, "\r\n", 2);
                pid = fork();
                //child
                if (pid == 0) 
                {           
                    close(1);
                    dup(sockfd);
                    if (execvp(path, NULL) < 0)
                    {
                        perror("execvp");
                    }
                }
                //parent
                else
                {
                    while (wait(NULL) > 0);
                }
            }
            else if (request.file[i+1] == 'h' &&
                     request.file[i+2] == 't' &&
                     request.file[i+3] == 'm' &&
                     request.file[i+4] == 'l')
            {
                //handle html
                write(sockfd, "HTTP/1.1 200 OK\r\n", 17);
                write(sockfd, "Server: rudyhttpd\r\n", 19);
                write(sockfd, "Content-Type: text/html\r\n", 25);
                write(sockfd, "\r\n", 2);
                pid = fork();
                //child
                if(pid == 0)
                {
                    close(1);
                    dup(sockfd);
                    
                    FILE *file_html;
                    char tmp[5000];
                    char tmp2[1000];
                    file_html = fopen(path, "r");
                    while(fgets(tmp2, 1000, file_html) != NULL)
                    {
                        strcat(tmp, tmp2);
                    }
                    fclose(file_html);
                    printf("%s", tmp);
                }
            }
        }
    }

}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("%s\n", "Usage: ./http-server [port-number]!");
        return 0;
    }
    int len;
    int server_sockfd;
    int child_sockfd;
    char buffer[MAX_BUFFER_LENGTH];
    
    struct sockaddr_in child_addr, server_addr;
    
    if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        perror("socket");
    }
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));
    
    int opt = 1;
    setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
    }
    
    listen(server_sockfd, 5);
    
    while (1)
    {
        if ((child_sockfd = accept(server_sockfd, NULL, NULL)) < 0)
        {
            perror("accept");
        }
        
        //Child
        if (fork() == 0)
        {
            close(server_sockfd);        
            if ((len = read(child_sockfd, buffer, sizeof(buffer))) > 0)
            {
                printf("=====\n%s\n====\n", buffer);
                processRequest(buffer, child_sockfd);
            }
            
            else
            {
                write(1, "recv no data\n", 13);
                break; 
            }
            exit(0);
        }
        //parent process
        else
        {
            close(child_sockfd);
        }
    }
    close(server_sockfd);
    return(0);
}
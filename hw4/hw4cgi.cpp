#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <errno.h>

#define MAX_HOST 6
#define MAX_BUFFER_LENGTH 10001
#define EXIT_STR "exit\r\n"

struct hostinfo
{
    FILE *file_fd;
    char ip[20];
    int port;
    int write_enable;
    int unsend;
    bool available;

    char socks_ip[20];
    int socks_port;
    bool socks_enable;

    hostinfo() {
        available = true;
        write_enable = 0;
        unsend = 0;
        socks_enable = true;
    }
};

struct hostinfo params[MAX_HOST];
fd_set afds;

int contain_prompt (char* line)
{
    int i, prompt = 0 ;
    for (i=0; line[i]; ++i) {
        switch ( line[i] ) {
            case '%' : prompt = 1 ; break;
            case ' ' : if ( prompt ) return 1;
            default : prompt = 0;
        }
    }
    return 0;
}

void printHtml(char *msg, int to, bool bold)
{
    printf("<script>document.all['m%d'].innerHTML +=\"", to);
    int i = 0;
    if (bold)
        printf("%s", "<b>");
    while (msg[i] != '\0')
    {
        switch(msg[i])
        {
            case '<':
                printf("%s", "&lt");
                break;
            case '>':
                printf("%s", "&gt");
                break;
            case ' ':
                printf("%s", "&nbsp;");
                break;
            case '\r':
                if (msg[i+1] == '\n')
                {
                    printf("%s", "<br>");
                    i++;
                }
                break;
            case '\n':
                printf("%s", "<br>");
                break;
            case '\\':
                printf("%s", "&#039");
                break;
            case '\"':
                printf("%s", "&quot;");
                break;
            default:
                printf("%c", msg[i]);
        }
        i++;
    }
    if (bold)
        printf("%s", "</b>");
    printf("\";</script>\n");
}

int recv_msg(int userno, int from)
{
    char buf[MAX_BUFFER_LENGTH],*tmp;
    int len,i;
  
    len = read(from, buf, sizeof(buf)-1);
    if (len < 0) return -1;

    buf[len] = 0;
    if (len > 0)
    {
        for(tmp = strtok(buf,"\n"); tmp; tmp = strtok(NULL,"\n"))
        {
            if (contain_prompt(tmp)) 
            {
                params[userno].write_enable = 1;
                printHtml(tmp, userno, false);
            }
            else
            {
                char lineFeed[MAX_BUFFER_LENGTH];
                strcpy(lineFeed, tmp);
                strcat(lineFeed, "\n");
                printHtml(lineFeed, userno, false);
            }
            
        }
    }
    fflush(stdout); 
    return len;
}

int readline(int fd, char *ptr, int maxlen)
{
    int n, rc;
    char c;
    *ptr = 0;
    for (n = 1; n < maxlen; n++)
    {
        rc = read(fd, &c, 1);
        if (rc == 1)
        {
            *ptr++ = c;
            if(c == '\n')  break;
        }
        else if (rc == 0)
        {
            if (n == 1)     return 0;
            else         break;
        }
        else return (-1);
    }
    return n;
}  

void send_header()
{
    printf("%s\n", 
        "<html>\
        <head>\
        <meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />\
        <title>Network Programming Homework 3</title>\
        </head>\
        <body bgcolor=#336699>\
        <font face=\"Courier New\" size=2 color=#FFFF99>");
}

void send_end()
{
    printf("%s\n",
        "</font>\
        </body>\
        </html>");
}

void parseArg()
{
    char *env = getenv("QUERY_STRING");
    // Parse and save parameters

    if (env != NULL)
    {
        char *token;
        int index;
        token = strtok(env, "&");
        while (token != NULL)
        {
            index = token[1] - '0';
            if (token[0] == 'h')
            {
                struct hostent *he;
                strcpy(params[index].ip, token+3);
                if (!strcmp(params[index].ip, ""))
                {
                    params[index].available = false;
                }
            }
            else if (token[0] == 'p')
            {
                char port[6];
                strcpy(port, token+3);
                if (!strcmp(port, ""))
                {
                    params[index].available = false;
                }
                else
                {
                    params[index].port = atoi(port);
                }
            }
            else if (token[0] == 'f')
            {
                char batch_file[256];
                strcpy(batch_file, token+3);
                if (!strcmp(batch_file, ""))
                {
                    params[index].available = false;
                }
                else
                {   
                    if ((params[index].file_fd = fopen(batch_file, "r")) == NULL)
                    {
                        params[index].available = false;
                    }
                    else
                    {
                        FD_SET(fileno(params[index].file_fd), &afds); 
                    }
                }
            }
            else if (token[0] == 's')
            {
                index = token[2] - '0';
                if (token[1] == 'h')
                {
                    struct hostent *he;
                    strcpy(params[index].socks_ip, token+4);
                    if (!strcmp(params[index].socks_ip, ""))
                    {
                        params[index].socks_enable = false;
                    }
                }
                else if (token[1] == 'p')
                {
                    char port[6];
                    strcpy(port, token+4);
                    if (!strcmp(port, ""))
                    {
                        params[index].socks_enable = false;
                    }
                    else
                    {
                        params[index].socks_port = atoi(port);
                    }
                }
            }

            token = strtok(NULL, "&");
        }
    }  
}

int main()
{
    fd_set              rfds;
    char                msg_buf[MAX_HOST][MAX_BUFFER_LENGTH];
    int                 len;
    int                 nfds;
    int                 client_fd[MAX_HOST];
    struct hostent      *he; 
    struct sockaddr_in  client_sin;


    FD_ZERO(&rfds);
    FD_ZERO(&afds);
    nfds = getdtablesize();


    parseArg();
    send_header();
    printf("<table width=\"800\" border=\"1\">\n");
    printf("<tr>\n");
    for (int i = 0; i < MAX_HOST; i++)
    {
        if (params[i].available)
        {
            printf("<td>%s</td>\n", params[i].ip);
        }
    }
    printf("</tr>\n");
    printf("<tr>\n");
    for (int i = 0; i < MAX_HOST; i++)
    {
        if (params[i].available)
        {    
            printf("<td valign=\"top\" id=\"m%d\"></td>\n", i);
        }
    }
    printf("</tr>\n");
    printf("</table>\n");
    send_end();

    for (int i = 1; i < MAX_HOST; i++)
    {
        if (params[i].available)
        {
            if (params[i].socks_enable)
            {
                char socks_packet[8];
                char cpCliIP[20];
                char *token;

                he = gethostbyname(params[i].socks_ip);
                client_fd[i] = socket(AF_INET, SOCK_STREAM, 0);
                bzero(&client_sin, sizeof(client_sin));
                client_sin.sin_family = AF_INET;
                client_sin.sin_addr = *((struct in_addr *)he->h_addr); 
                client_sin.sin_port = htons((u_short)(params[i].socks_port));

                if (connect(client_fd[i], (struct sockaddr *)&client_sin , sizeof(client_sin)) < 0)
                {
                    params[i].available = false;
                }

                socks_packet[0] = 4;
                socks_packet[1] = 1;
                socks_packet[2] = params[i].port / 256;
                socks_packet[3] = params[i].port % 256;

                strcpy(cpCliIP, params[i].ip);
                token = strtok(cpCliIP, ".\r\n");
                for (int j = 4; j <= 7; j++)
                {
                    if (token == NULL)
                    {
                        perror("can't parse IP in the SOCKS header packet!");
                        return -1;
                    }
                    socks_packet[j] = atoi(token);
                    token = strtok(NULL, ".\r\n");
                }
            
                write(client_fd[i], socks_packet, 8);
                read(client_fd[i], socks_packet, 8);
            }
            else
            {
                he = gethostbyname(params[i].ip);
                client_fd[i] = socket(AF_INET, SOCK_STREAM, 0);
                bzero(&client_sin, sizeof(client_sin));
                client_sin.sin_family = AF_INET;
                client_sin.sin_addr = *((struct in_addr *)he->h_addr); 
                client_sin.sin_port = htons((u_short)(params[i].port));

                int flags = fcntl(client_fd[i], F_GETFL, 0);
                fcntl(client_fd[i], F_SETFL, flags | O_NONBLOCK);
                
                if (connect(client_fd[i], (struct sockaddr *)&client_sin , sizeof(client_sin)) < 0)
                {
                    if (errno != EINPROGRESS)
                    {
                        params[i].available = false;
                    }
                }
            }

            FD_SET(client_fd[i], &afds);
        }
    }

    while (1)
    {
        bool running = false;
        for (int i = 1; i < MAX_HOST; i++)
        {
            running = running | params[i].available;
        }
        if (!running) break;

        memcpy(&rfds, &afds, sizeof(rfds));
        if (select(nfds, &rfds, NULL, NULL, NULL) < 0) return 0;
        for (int i = 1; i < MAX_HOST; i++)
        {  
            if (params[i].available)
            {
                if (FD_ISSET(client_fd[i], &rfds))
                {
                    int error;
                    socklen_t n = sizeof(error);
                    if (getsockopt(client_fd[i], SOL_SOCKET, SO_ERROR, &error, &n) < 0 ||
                        error != 0)
                    {
                        //Non-blocking failed
                        return -1;
                    }
                    else if (recv_msg(i, client_fd[i]) < 0)
                    {
                        FD_CLR(client_fd[i], &afds);
                        close(client_fd[i]);
                        params[i].available = false;
                    }
                }

                if (params[i].unsend || FD_ISSET(fileno(params[i].file_fd), &rfds))
                {
                    if (!params[i].unsend) 
                    {
                        bool skip = false;
                        //送meesage
                        len = readline(fileno(params[i].file_fd), msg_buf[i], sizeof(msg_buf[i])-1);

                        if (len < 0)
                        {
                            params[i].available = false;
                        }

                        // input exceed buffer size
                        while (len == sizeof(msg_buf[i])-1)
                        {
                            skip = true;
                            len = readline(fileno(params[i].file_fd), msg_buf[i], sizeof(msg_buf[i])-1);
                        }
                        if (skip)
                            len = readline(fileno(params[i].file_fd), msg_buf[i], sizeof(msg_buf[i])-1);

                        msg_buf[i][len] = 0;
                        fflush(stdout);
                    }
                    params[i].unsend = 0;
                    if (!strncmp(msg_buf[i], "exit",4))  // exit all
                    {                  
                        if (params[i].write_enable)
                        {
                            if ((FD_ISSET(client_fd[i], &afds)))
                            {
                                if (write(client_fd[i], EXIT_STR, 6) == -1)
                                {
                                    params[i].available = false;
                                    continue;
                                }
                                params[i].write_enable = 0;
                                printHtml(EXIT_STR, i, true);
                                while (recv_msg(i, client_fd[i]) > 0);
                                FD_CLR(client_fd[i], &afds);
                                close(client_fd[i]);
                            }

                            FD_CLR(fileno(params[i].file_fd), &afds);
                            fclose(params[i].file_fd);
                            params[i].available = false;
                        }
                        else
                        {
                            params[i].unsend = 1;
                        }
                    }
                    else  // send command
                    {
                        if (params[i].write_enable) 
                        {
                            if (write(client_fd[i], msg_buf[i], strlen(msg_buf[i])) < 0)
                            {
                                //handle write error
                                printf("%s\n", "message exceed socket buffer length");
                            }
                            printHtml(msg_buf[i], i, true);
                            params[i].write_enable = 0; 
                        }
                        else 
                        {
                            params[i].unsend = 1;
                        }
                    }
                }
            }
        }
    }

}
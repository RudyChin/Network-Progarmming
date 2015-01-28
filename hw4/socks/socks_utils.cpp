#include "socks_utils.h"

using namespace std;

int readline(int fd, char* ptr, int maxlen)
{
    int n, rc;
    char c;
    for (n = 1; n < maxlen; ++n) 
    {
        if ((rc = read(fd, &c, 1)) == 1) 
        {
            *ptr++ = c;
            if (c == '\n') break;
        }
        else if (rc == 0) 
        {
            if (n == 1) return 0;
            else break;
        }
        else 
        {
            return -1;
        }
    }
    *ptr = '\0';
    return n;
}

//SOCK4_REQUEST
const unsigned char *getDomainName(const unsigned char * buf)
{
    buf = buf + 8;
    while (*buf != '\0') ++buf;
    return (buf + 1);   
}

int connectTCP(unsigned ip, unsigned port)
{
    int    client_fd;
    struct sockaddr_in client_sin;

    client_fd = socket(AF_INET, SOCK_STREAM ,0);
    bzero(&client_sin, sizeof(client_sin));
    client_sin.sin_family = AF_INET;
    client_sin.sin_addr.s_addr = htonl(ip); 
    client_sin.sin_port = htons((u_short)port);

    if (connect(client_fd, (struct sockaddr *)&client_sin, sizeof(client_sin)) == -1) 
        return -1;
    return client_fd;   
}
int passiveTCP(unsigned port)
{
    struct sockaddr_in serv_addr;
    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Error: can't open stream socket\n");
        return -1;  
    }

    bzero((char*) &serv_addr, sizeof(serv_addr));   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Error: can't bind local address\n");
        return -1;
    }

    if (listen(sockfd, 0) < 0){
        perror("Error: listen failed\n");
        exit(-1);
    }
    return sockfd;
}

void redirectData(int ssock, int rsock)
{
    fd_set rfds, afds;
    int nfds;
    nfds = getdtablesize();
    FD_ZERO(&afds);
    FD_SET(ssock, &afds);
    FD_SET(rsock, &afds); 

    while (true)
    {
        memcpy(&rfds, &afds, sizeof(rfds));
        if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
        {
            perror("server: select error\n");
            printf("%d\n", errno);
            exit(1);    
        }
        /* ssock -> rsock*/
        if (FD_ISSET(ssock, &rfds))
        {//檢查fdset聯系的文件句柄fd是否可讀寫，>0表示可讀寫
            char buffer[1024] = {0};
            int n;
            n = read(ssock, buffer, 1024);
            if (n < 0)
            {
                perror("read error\n");
                exit(1);    
            }
            else if (n == 0) 
            {
                break;
            }
            write(rsock, buffer, n);    
        }

        /* rsock -> ssock */
        if (FD_ISSET(rsock, &rfds))
        {
            char buffer[1024] = {0};
            int n;
            n = read(rsock, buffer, 1024);
            if (n < 0)
            {
                perror("read error\n");
                exit(1);
            }
            else if (n == 0) 
            {
                break;
            }
            write(ssock, buffer, n);    
        }
    }
}

int parseBlockIP(char *token)
{
    token = strtok(NULL, " ");
    while(token != NULL)
    {
        if (strcmp(token, "-"))
        {
            char *ptr;
            ptr = token;
            unsigned int ip_total = 0;
            for (int i = 3; i >= 0; i--)
            {
                while (*ptr != ' ' && *ptr != 0) ++ptr;
                *ptr = '\0';
                if (ptr == token) break; //token == NULL
                unsigned int temp = atoi(token);
                ip_total |= (temp << (8 * i));
                token = ptr + 1;
                ++ptr;
            }
            return ip_total;
        }
        token = strtok(NULL, " ");
    }
    return 0;
}

bool checkIP(vector<unsigned int>& deny, vector<unsigned int>& permit, unsigned int ip)
{
    int i;
    for (i = 0; i < deny.size(); i++)
    {
        if ((ip & deny[i]) == deny[i]) return false;//ip same
    }
    if (i == deny.size()) //no deny ip
    {
        int j;
        for (j = 0; j < permit.size(); j++)
        {
            if ((ip & permit[j]) == permit[j]) 
            {
                return true;//ip same
            }
        }
    }
    return false;
}

bool checkFirewall(vector<unsigned int>& permit_C, vector<unsigned int>& permit_B, vector<unsigned int>& deny_C, vector<unsigned int>& deny_B, const char * filename,  unsigned char CD, unsigned int ip)
{
    int fd = open(filename, O_RDONLY);
    char buffer[100] = {0};

    printf("%s\n", "firewall");

    while (readline(fd, buffer, 100))
    {
        strtok(buffer, "\r\n");
        char *token;
        token = strtok(buffer, " "); //token is permit or deny

        if (!strcmp(token, "permit"))
        {
            token = strtok(NULL, " "); //token is c or b

            if (*token == 'c')
            {
                permit_C.push_back(parseBlockIP(token));
            }
            else if (*token == 'b')
            {
                permit_B.push_back(parseBlockIP(token));
            }
        }
        else if (!strcmp(token, "deny"))
        {
            token = strtok(NULL, " "); //token is c or b
            if (*token == 'c')
            {
                deny_C.push_back(parseBlockIP(token));
            }
            else if (*token == 'b')
            {
                deny_B.push_back(parseBlockIP(token));
            }
        }

    }

    /* CONNECT MODE */
    if (CD == 1)
    {
        return checkIP(deny_C, permit_C, ip);
    }
    /* BIND */
    else if (CD == 2)
    {
        return checkIP(deny_B, permit_B, ip);
    } 
}

void child_term_handler(int no)
{
    wait(0);
}
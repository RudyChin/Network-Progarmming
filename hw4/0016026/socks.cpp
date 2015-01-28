#include "socks_utils.h"

using namespace std;

int *client_count;


void Handle_SOCKS(int newsockfd)
{
    socklen_t   client_len;
    struct      sockaddr_in client_addr;
    client_len = sizeof(client_addr);   

    unsigned char packet_buffer[MAX_BUF_LEN];

    if (read(newsockfd, packet_buffer, sizeof(packet_buffer)) < 0)
    {
        fprintf(stderr, "read error!\n");
        exit(1);    
    }

    unsigned char VN, CD;
    unsigned int dst_port, dst_ip, src_port, src_ip;
    const unsigned char * user_id = NULL; 
    const unsigned char * domain_name = NULL;
    
    //SOCK4_REQUEST
    VN = packet_buffer[0];
    
    if (VN != 4)
    {
        char err_str[200];
        snprintf(err_str, 200, "[%d] [%d] [%d] [%d] [%d] [%d] [%d] [%d]",
            packet_buffer[0], packet_buffer[1], packet_buffer[2], packet_buffer[3], packet_buffer[4],
            packet_buffer[5], packet_buffer[6], packet_buffer[7]);
        printf("%s\n", err_str);
        perror("Error: client sends unknown packet, connection closed\n");
        exit(-1);
    }

    CD = packet_buffer[1];
    dst_port = packet_buffer[2] << 8 | packet_buffer[3];
    dst_ip = packet_buffer[4] << 24 | packet_buffer[5] << 16 | packet_buffer[6] << 8 | packet_buffer[7];
    
    user_id = packet_buffer + 8;

    getpeername(newsockfd, (struct sockaddr*)&client_addr, &client_len);
    src_port = ntohs(client_addr.sin_port);
    src_ip = ntohl(client_addr.sin_addr.s_addr);

    if ((dst_ip & 0xFFFFFF00) == 0){
        domain_name = getDomainName(packet_buffer);
        printf("Domain Name: %s\n", domain_name);
        struct hostent *he = NULL;
        he = gethostbyname((const char*)domain_name);
        dst_ip = ntohl(((struct in_addr *)he->h_addr)->s_addr);
    }

    printf("VN: %d, CD: %d, " ,VN,CD);
    printf("DST IP : %d.%d.%d.%d, ", 
          (dst_ip >> 24) & 255, (dst_ip >> 16) & 255, (dst_ip >> 8) & 255, (dst_ip & 255));
    printf("DST PORT: %d, ", dst_port);
    printf("USERID: %s\n", user_id);
    fflush(stdout); 

    /* FireWall */
    vector<unsigned int> permit_C;
    vector<unsigned int> permit_B;
    vector<unsigned int> deny_C;
    vector<unsigned int> deny_B;

    if (checkFirewall(permit_C, permit_B, deny_C, deny_B , "socks.conf", CD , dst_ip) == false)
    {
        unsigned char reply[8];
        reply[0] = VN;
        reply[1] = 0x5B;
        reply[2] = dst_port / 256;
        reply[3] = dst_port % 256;
        reply[4] = (dst_ip >> 24) & 0xFF;
        reply[5] = (dst_ip >> 16) & 0xFF;
        reply[6] = (dst_ip >> 8)  & 0xFF;
        reply[7] = dst_ip & 0xFF;
        write(newsockfd, reply, 8);
        printf("Deny Src = %d.%d.%d.%d(%d), Dst = %d.%d.%d.%d(%d)\n",
                (src_ip >> 24 ) & 0xFF, (src_ip >> 16) & 0xFF, (src_ip >> 8) & 0xFF, src_ip & 0xFF, src_port, 
                (dst_ip >> 24) & 0xFF, (dst_ip >> 16) & 0xFF, (dst_ip >> 8) & 0xFF, dst_ip & 0xFF, dst_port);
        exit(0);
    }
    else
    {
        /* CONNECT MODE */
        if (CD == 1)
        {
            int rsock;
            unsigned char reply[8];
            if ((rsock = connectTCP(dst_ip, dst_port)) < -1)
            { 
                reply[0] = 0;
                reply[1] = 0x5B; 
                reply[2] = dst_port / 256;
                reply[3] = dst_port % 256;
                reply[4] = (dst_ip >> 24) & 0xFF;
                reply[5] = (dst_ip >> 16) & 0xFF;
                reply[6] = (dst_ip >> 8)  & 0xFF;
                reply[7] = dst_ip & 0xFF;
                write(newsockfd, reply, 8);
                printf("SOCKS_CONNECT FAILED ....\n");
                fflush(stdout);
                close(newsockfd);
                exit(0);
            }
            else 
            { 
                printf("Permit Src =  %d.%d.%d.%d(%d), Dst = %d.%d.%d.%d(%d)\n", 
                  (src_ip >> 24) & 255, (src_ip >> 16) & 255, (src_ip >> 8) & 255, src_ip & 255, src_port, 
                  (dst_ip >> 24) & 255, (dst_ip >> 16) & 255, (dst_ip >> 8) & 255, dst_ip & 255, dst_port);

                reply[0] = 0;
                reply[1] = 0x5A;
                reply[2] = dst_port / 256;
                reply[3] = dst_port % 256;
                reply[4] = (dst_ip >> 24) & 0xFF;
                reply[5] = (dst_ip >> 16) & 0xFF;
                reply[6] = (dst_ip >> 8)  & 0xFF;
                reply[7] = dst_ip & 0xFF;


                if ((*client_count) > 4)
                {
                    printf("Only accecpt 4 connections, current:[%d]\n", (*client_count)-1);
                    close(newsockfd);
                    close(rsock);
                    return;
                }

                (*client_count)++;

                write(newsockfd, reply, 8);

                printf("SOCKS_CONNECT GRANTED ....\n");
                fflush(stdout);
                /* Redirect the Data */

                redirectData(newsockfd, rsock);
                close(newsockfd);
                close(rsock);
                exit(-1);   
            }
        } //end if  (CD == 1)
        
        /* BIND MODE */
        else if (CD == 2)
        {
            int rsock, psock;
            srand(time(NULL));
            int port = 64000 + rand() % 1000;
            psock = passiveTCP(port);//passiveTCP
            dst_ip = 0;
            struct sockaddr_in b_addr;
            socklen_t blen = sizeof(b_addr);
            getsockname(psock, (struct sockaddr*)&b_addr, &blen);
            dst_port = ntohs(b_addr.sin_port);
            if (psock > -1)
            {
                printf("Permit Src =  %d.%d.%d.%d(%d), Dst = %d.%d.%d.%d(%d)\n", 
                  (src_ip >> 24) & 255, (src_ip >> 16) & 255, (src_ip >> 8) & 255, src_ip & 255, src_port, 
                  (dst_ip >> 24) & 255, (dst_ip >> 16) & 255, (dst_ip >> 8) & 255, dst_ip & 255, dst_port);
                /* Send Reply back to the source */
                unsigned char reply[8];
                reply[0] = 0;
                reply[1] = 0x5A;
                reply[2] = dst_port / 256;
                reply[3] = dst_port % 256;
                reply[4] = (dst_ip >> 24) & 0xFF;
                reply[5] = (dst_ip >> 16) & 0xFF;
                reply[6] = (dst_ip >> 8)  & 0xFF;
                reply[7] = dst_ip & 0xFF;

                write(newsockfd, reply, 8);
                
                if ((rsock = accept(psock, (struct sockaddr*) &client_addr, &client_len)) < 0){
                    printf("rsock = %d\n",rsock);
                    printf("accept error!\n");
                    fflush(stdout);
                    exit(0);
                }
                printf("%s\n", "accepted");

                write(newsockfd, reply, 8);
                
                printf("SOCKS_BIND GRANTED ....\n");
                fflush(stdout);
                /* Redirect the Data */
                redirectData(newsockfd, rsock);
                close(newsockfd);
                close(rsock);
                exit(0);
            }
            else
            {
                unsigned char reply[8];
                reply[0] = 0;
                reply[1] = 0x5B;
                reply[2] = dst_port / 256;
                reply[3] = dst_port % 256;
                reply[4] = (dst_ip >> 24) & 0xFF;
                reply[5] = (dst_ip >> 16) & 0xFF;
                reply[6] = (dst_ip >> 8)  & 0xFF;
                reply[7] = dst_ip & 0xFE;
                write(newsockfd, reply, 8);
                printf("SOCKS_BIND FAILED ....\n");
                fflush(stdout);
                close(newsockfd);
                exit(0);    
            }

        }//end else if (CD == 2)
        else
        {
            perror("Error: client sends unknown packet, connection closed\n");
            exit(-1);
        }
    }
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, childpid;

//CONNECTION COUNT

    int shmid = shmget(SHMKEY, sizeof(int), IPC_CREAT | 0600);
    if (shmid < 0)
    {
        return -1;
    }
    
    if ( (client_count = (int*)shmat(shmid, NULL, 0)) < 0 )
    {
        return -1;
    }

    *client_count = 0;
//CONNECTION COUNT


    socklen_t clilen;
    struct sockaddr_in cli_addr, serv_addr;
    signal(SIGCHLD, child_term_handler);
    
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s port_number\n", argv[0]);
        exit(-1);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "server: can't open stream socket\n");
        exit(-1);
    }

    int isSetSockOk = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &isSetSockOk, sizeof(int)) == -1)
    { 
        perror("SetSockOpt error");
        exit(-1);
    } 

    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
        fprintf(stderr, "server: can't bind local address");
        exit(-1);
    }
    if (listen(sockfd, 20) < 0){
        fprintf(stderr, "server: listen failed");
        exit(-1);
    }
    while (1)
    {
        printf("waiting for connection...\n");
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
        
        if (newsockfd < 0)
        {
            fprintf(stderr, "server: accept error");
            exit(-1);
        }
        
        childpid = fork();
        if (childpid < 0)
        {
            fprintf(stderr, "server: fork failed");
            exit(-1);
        }
        else if (childpid == 0)
        {
            close(sockfd);
            Handle_SOCKS(newsockfd);
            close(newsockfd);
            (*client_count)--;
            printf("count:[%d]\n", *client_count);
            shmdt(client_count);
            exit(0);
        }
        else 
        {
            close(newsockfd);
        }
    }
    return 0;  
}
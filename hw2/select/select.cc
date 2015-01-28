//
//  main.cpp
//  rush
//
//  Created by Rudy on 2014/10/28.
//  Copyright (c) 2014年 Rudy. All rights reserved.
//

#include "np_utils.h"

//Global variable used in share memory
UserInfo usrData[MAX_CLIENTS];
int uid;
fd_set afds;

void runShell(char *inputBuff)
{
    //Parsing out first command and pipe n
    int i = 0;
    while (!isReturn(inputBuff[i]))
    {
        int pipefd[2], pipeN;
        char cmd[MAX_COMMAND_LENGTH];
        char filename[MAX_COMMAND_LENGTH];
        bool pipeToSame = false;
        vector<fdcounts>::iterator pipeIt;
        Sign sign = NONE;
        
        //pass through spaces and tabs
        while (isSpace(inputBuff[i]))
        {
            i++;
        }
        
        //parsing command
        parseCmd(i, inputBuff, cmd, filename, sign, pipeN);
        
        //Decrease every counter by 1
        for (vector<fdcounts>::iterator it = usrData[uid].counterTable.begin();
                it != usrData[uid].counterTable.end();
                it++)
        {
            (it->counter)--;
            if (pipeN == it->counter)
            {
                pipeToSame = true;
                pipeIt = it;
            }
        }
        
        //create parcel data to flow between different situation
        envParcel parcel(&(usrData[uid].counterTable), cmd, usrData[uid].fd);
        if (sign == PIPE)
        {
            int status = evalCmdOfPipe(parcel, pipeToSame, pipefd, pipeN, pipeIt);
            if (status == -1)
            {
                usr_exit();
                return;
            }
            else if (status == -2)
            {
                prompt();
                return;
            }
        }
        else if (sign == EXPORT_FILE)
        {
            int status = evalCmdOfExport(parcel, filename);
            if (status == -1)
            {
                usr_exit();
                return;
            }
            else if (status == -2)
            {
                prompt();
                return;
            }
        }
        else if (sign == TRANS_USER)
        {
            char buf[MAX_MSG];
            bool flag = false;
            if (usrData[uid].is_send_to)
            {
                if (!isValidUsr(usrData[uid].is_send_to))
                {
                    flag = true;
                    sprintf(buf, "*** Error: user #%d does not exist yet. ***\n", usrData[uid].is_send_to);
                    write(usrData[uid].fd, buf, strlen(buf));
                }
                else if (usrData[uid].send_tag[usrData[uid].is_send_to] == 1)
                {
                    flag = true;
                    sprintf(buf, "*** Error: the pipe #%d->#%d already exists. ***\n", uid, usrData[uid].is_send_to);
                    write(usrData[uid].fd, buf, strlen(buf));
                }
            }
            if (usrData[uid].is_receive_from)
            {
                if (!isValidUsr(usrData[uid].is_receive_from))
                {
                    flag = true;
                    sprintf(buf, "*** Error: user #%d does not exist yet. ***\n", usrData[uid].is_receive_from);
                    write(usrData[uid].fd, buf, strlen(buf));
                }
                else if (usrData[usrData[uid].is_receive_from].send_tag[uid] == 0)
                {
                    flag = true;
                    sprintf(buf, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", usrData[uid].is_receive_from, uid);
                    write(usrData[uid].fd, buf, strlen(buf));
                }
            }
            if (!flag)
            {
                int status = evalTransUser(parcel);
                if (status == -1)
                {
                    usr_exit();
                    return;
                }
                else if (status == -2)
                {
                    prompt();
                    return;
                }
                else
                {
                    char wholeCmd[MAX_COMMAND_LENGTH];
                    strcpy(wholeCmd, inputBuff);
                    int j = 0;
                    while (!isReturn(wholeCmd[j]))
                    {
                        j++;
                    }
                    wholeCmd[j] = '\0';
                    if (usrData[uid].is_send_to)
                    {
                        sprintf(buf, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", usrData[uid].nick, 
                                uid, wholeCmd, usrData[usrData[uid].is_send_to].nick, usrData[uid].is_send_to);
                        yell(buf);
                        usrData[uid].is_send_to = 0;
                    }
                    if (usrData[uid].is_receive_from)
                    {
                        sprintf(buf, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", usrData[uid].nick,        
                            uid, usrData[usrData[uid].is_receive_from].nick, usrData[uid].is_receive_from, wholeCmd);
                        yell(buf);
                        usrData[uid].is_receive_from = 0;
                    }
                }
            }
        }
        else
        {
            int status = evalSingleCmd(parcel);
            if (status == -1)
            {
                usr_exit();
                return;
            }
            else if (status == -2)
            {
                prompt();
                return;
            }
            
        }
    }
    prompt();
}

int main (int argc, char *argv[])
{
    signal(SIGCHLD, SIG_IGN);

    fd_set rfds;
    int sockfd, newsockfd, childpid;
    int msglen;
    int nfds;
    char msg[MAX_MSG];
    socklen_t clilen;
    struct sockaddr_in cli_addr, serv_addr;

    FD_ZERO(&afds);
    FD_ZERO(&rfds);

    for (int i = 1; i < MAX_CLIENTS; i++)
    {
        init(i);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("server cannot open socket");
        return -1;
    }
    
    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(TELNET_TCP_PORT);
    
    if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("server cannot bind");
        return -1;
    }
    listen(sockfd, 5);

    FD_SET(sockfd, &afds);
    nfds = getdtablesize();

    while (1)
    {
        rfds = afds;

        if (select(nfds, &rfds, (fd_set*)0, (fd_set*)0, (struct timeval*)0) < 0)
        {
            perror("select error");
            exit(1);
        }

        for (int i = 0; i <= nfds; i++)
        {
            if (FD_ISSET(i, &rfds))
            {
                //Incoming client
                if (i == sockfd)
                {
                    clilen = sizeof(cli_addr);
                    newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
                    if (newsockfd < 0)
                    {
                        perror("server cannot accept");
                        return -1;
                    }
                    else
                    {
                        FD_SET(newsockfd, &afds);
                        nfds = getdtablesize();
                        uid = getUserId();
                        usrData[uid].fd = newsockfd;
                        usrData[uid].id = uid;
                        char *ip_tmp;
                        ip_tmp = inet_ntoa(cli_addr.sin_addr);
                        strcpy(usrData[uid].ip, ip_tmp);
                        usrData[uid].port = ntohs(cli_addr.sin_port);

                        msglen = sprintf(msg, "*** User '(no name)' entered from %s/%d. ***\n", usrData[uid].ip, usrData[uid].port);
                        welcomeTxt(newsockfd);
                        yell(msg);

                        write(newsockfd, "% ", 2);
                    }
                }
                else
                //client say something
                {
                    char buffer[MAX_INPUT_LENGTH];
                    int nbytes;
                    if (nbytes = recv(i, buffer, sizeof(buffer), 0) <= 0)
                    {
                        if (nbytes == 0)                
                        {
                            printf("socket hang.\n");
                        }
                        else
                        {
                            perror("recv error");
                        }
                        close(i);

                        FD_CLR(i, &afds);
                    }
                    else
                    {
                        for (uid = 1; uid < MAX_CLIENTS; uid++)
                        {
                            if (usrData[uid].fd == i)
                                break;
                        }
                        setenv(usrData[uid].env[0], usrData[uid].env_val[0], 1);
                        runShell(buffer);
                    }
                }
            }
        }
    }
    return 0;
}

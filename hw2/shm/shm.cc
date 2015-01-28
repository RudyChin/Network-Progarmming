//
//  main.cpp
//  rush
//
//  Created by Rudy on 2014/10/28.
//  Copyright (c) 2014年 Rudy. All rights reserved.
//

#include "np_utils.h"

//Global variable used in share memory
UserInfo *usrData;
int uid;
int newsockfd;

void sigHandler(int sig)
{
    write(newsockfd, usrData[uid].msg, strlen(usrData[uid].msg));
    strcpy(usrData[uid].msg, " ");
}

void runShell(int sockfd)
{
    char inputBuff[MAX_INPUT_LENGTH];
    vector<fdcounts> counterTable;
    setenv("PATH", ORIGINAL_PATH, 1);
    //redirect errmsg to socket
    if (dup2(sockfd, 2) == -1)
    {
        perror("change stderr to sock error");
        return;
    }
    
    while(1)
    {
        //Write prompt symbol
        write(sockfd, "% ", 2);
        
        //Reading a line from socket
        int pos = 0;
        char c;
        while (read(sockfd, &c, 1) == 1)
        {
            inputBuff[pos++] = c;
            // \n is the last character of line that should be read
            if (c == '\n')
                break;
        }
        
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
            for (vector<fdcounts>::iterator it = counterTable.begin();
                 it != counterTable.end();
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
            envParcel parcel(&counterTable, cmd, sockfd);
            if (sign == PIPE)
            {
                int status = evalCmdOfPipe(parcel, pipeToSame, pipefd, pipeN, pipeIt);
                if (status == -1)
                {
                    return;
                }
                else if (status == -2)
                {
                    break;
                }
            }
            else if (sign == EXPORT_FILE)
            {
                int status = evalCmdOfExport(parcel, filename);
                if (status == -1)
                {
                    return;
                }
                else if (status == -2)
                {
                    break;
                }
            }
            else
            {
                int status = evalSingleCmd(parcel);
                if (status == -1)
                {
                    return;
                }
                else if (status == -2)
                {
                    break;
                }
                
            }
        }
    }
    
}

int main (int argc, char *argv[])
{
    if (initShm() == -1)
    {
        perror("Cannot allocate share memory!");
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGALRM, sigHandler);

    int sockfd, childpid;
    int msglen;
    char msg[MAX_MSG];
    socklen_t clilen;
    struct sockaddr_in cli_addr, serv_addr;

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
    while (1)
    {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
        if (newsockfd < 0)
        {
            perror("server cannot accept");
            return -1;
        }
        if ((childpid = fork()) < 0)
        {
            perror("server fork error");
            return -1;
        }
        else if (childpid == 0)
        {
            //child
            //close original socket
            close(sockfd);
            uid = clientInit(cli_addr.sin_addr, cli_addr.sin_port);
            
            msglen = sprintf(msg, "*** User '(no name)' entered from %s/%d. ***\n", usrData[uid].ip, usrData[uid].port);
            welcomeTxt(newsockfd);
            yell(msg);

            runShell(newsockfd);
            
            msglen = sprintf(msg, "*** User '%s' left. ***\n", usrData[uid].nick);
            yell(msg);
            exitOfUser();

            shmdt(usrData);
            exit(0);
        }
        else
        {
            //parent
            close(newsockfd);
        }
    }
    return 0;
}

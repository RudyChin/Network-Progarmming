//
//  main.cpp
//  rush
//
//  Created by Rudy on 2014/10/28.
//  Copyright (c) 2014年 Rudy. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_INPUT_LENGTH 10001
#define MAX_PIPE_LENGTH 5
#define MAX_COMMAND_LENGTH 257
#define MAX_PATH_LENGTH 257
#define ORIGINAL_PATH "bin"
#define WR_FILE_MODE (O_CREAT|O_WRONLY|O_TRUNC)

using namespace std;

struct fdcounts
{
    int infd;
    int outfd;
    int counter;

    fdcounts(int infd, int outfd, int counter)
    {
        this->infd = infd;
        this->outfd = outfd;
        this->counter = counter;
    }
};

enum Sign
{
    VERTICAL_BAR,
    CLOSE_BRACKET,
    NONE
};

bool isReturn(char c)
{
    if (c == '\n' || c =='\r')
        return true;
    return false;
}

bool isDigit(char c)
{
    if ( (c - '0') >= 0 && (c - '0') <= 9 )
        return true;
    return false;
}

bool isSpace(char c)
{
    if (c == ' ' || c == '\t')
        return true;
    return false;
}

void welcomeTxt(int sockfd)
{
    write(sockfd, "****************************************\n", 41);
    write(sockfd, "** Welcome to the information server. **\n", 41);
    write(sockfd, "****************************************\n", 41);
    return;
}

void extractTelnetHeader(int sockfd)
{
    char c[30];
    //HARDCODE: telnet sends header of 30 characters
    if (read(sockfd, c, 30) != 30)
    {
        perror("extracting telnet header error");
    }
}

bool containSlash(char *cmd)
{
    int i = 0;
    while (cmd[i] != '\0')
    {
        if (cmd[i] == '/')
            return true;
        else
            i++;
    }
    return false;
}

void writeErrcmdToSock(int sockfd, char *cmd)
{
    int len = 0;
    write(sockfd, "Unknown command: [", 18);
    while (cmd[len++] != '\0');
    write(sockfd, cmd, len-1);
    write(sockfd, "]\n", 2);
}

/*/
 * Input
 *  inputBuff: the raw input string
 *
 * Output
 *  i: current position of file
 *  cmd: parsed command
 *  hasPipeSign: whether there is a '|' in command
 *  n: the number behind '|' sign
 */
void parseCmd(int &i, char *inputBuff, char *cmd, char *filename, Sign &sign, int &n)
{
    int posOfCmd = 0;
    int posOfFile = 0;
    char pipeLength[MAX_PIPE_LENGTH];
    while (!isReturn(inputBuff[i]) && i < MAX_INPUT_LENGTH)
    {
        if (inputBuff[i] == '|')
        {
            i++; //Move to the first digit
            sign = VERTICAL_BAR;
            int posOfPipeLength = 0;

            if (isSpace(inputBuff[i]) || isReturn(inputBuff[i]))
            {
                pipeLength[posOfPipeLength++] = '1';
            }
            else
            {
                while (isDigit(inputBuff[i]))
                {
                    pipeLength[posOfPipeLength++] = inputBuff[i++];
                }
            }
            pipeLength[posOfPipeLength] = '\0';
            break;
        }
        else if (inputBuff[i] == '>')
        {
            sign = CLOSE_BRACKET;

            while (isSpace(inputBuff[++i]));

            while (!isSpace(inputBuff[i]) && !isReturn(inputBuff[i]))
            {
                filename[posOfFile++] = inputBuff[i++];
            }
        }
        else
        {
            cmd[posOfCmd++] = inputBuff[i++];
        }
    }
    cmd[posOfCmd] = '\0';
    filename[posOfFile] = '\0';

    if (sign == VERTICAL_BAR)
        n = atoi(pipeLength);
    else
        n = 0;
}

bool isCmdExist(char *path, char *cmd)
{
    char tmp[MAX_COMMAND_LENGTH];
    char cpyCmd[MAX_COMMAND_LENGTH];
    int pos = 0;
    while (!isSpace(cmd[pos]) && cmd[pos] != '\0')
    {
        cpyCmd[pos] = cmd[pos];
        pos++;
    }
    cpyCmd[pos] = '\0';

    if (strcmp(cpyCmd, "printenv") == 0 || 
            strcmp(cpyCmd, "setenv") == 0 ||
            strcmp(cpyCmd, "exit") == 0 )
    {
        return true;
    }
    path = getenv("PATH");
    strcpy(tmp, path);
    strcat(tmp, "/");
    strcat(tmp, cpyCmd);
    int fd = open(tmp, O_RDONLY);
    if (fd == -1)
    {
        return false;
    }
    close(fd);
    return true;
}

void recoverFromErrCmd(vector<fdcounts> &counterTable)
{
    for (vector<fdcounts>::iterator it = counterTable.begin();
            it != counterTable.end();
            it++)
    {
        (it->counter)++;
    }
}

/**
 * Return
 *     0 indicates OK
 *     Otherwise failure
 **/
int processCmd(char *path, char *cmd, int infd, int outfd)
{
    int totalarg, childpid;
    char *token;
    char *arglist[MAX_COMMAND_LENGTH];
    path = getenv("PATH");

    //Check whether there is a slash in cmd
    if (containSlash(cmd))
    {
        return -1;
    }

    //Initial total number of arguments
    totalarg = 0;
    //Parse arguments
    token = strtok(cmd, " ");
    while (token != NULL)
    {
        arglist[totalarg] = strdup(token);
        totalarg++;
        token = strtok(NULL, " ");
    }

    //Null terminated for execv()
    arglist[totalarg] = NULL;

    //SETENV, GETENV
    if (strcmp(arglist[0], "printenv") == 0)
    {
        if (arglist[1] != NULL)
        {
            path = getenv(arglist[1]);
        }
        else
        {
            path = getenv("PATH");
        }
        if (path != NULL)
        {
            int sizeOfPath = 0;
            while (path[sizeOfPath] != '\0')
            {
                sizeOfPath++;
            }
            write(outfd, path, sizeOfPath);
            //line feed
            write(outfd, "\n", 1);
            return 0;
        }
    }
    else if (strcmp(arglist[0], "setenv") == 0)
    {
        setenv(arglist[1], arglist[2], 1);
        return 0;
    }
    else if (strcmp(arglist[0], "exit") == 0)
    {
        return -1;
    }

    childpid = fork();
    if (childpid < 0)
    {
        perror("fork() error");
        return -1;
    }
    else if (childpid == 0)
    {
        //Change stdin, stdout
        if (infd != 0)
        {
            if (dup2(infd, 0) == -1 )
            {
                perror("change stdin to pipe error");
                exit(-1);
            }
        }
        if (outfd != 1)
        {
            if (dup2(outfd, 1) == -1)
            {
                perror("change stdout to pipe error");
                exit(-1);
            }
        }

        //EXEC
        strcat(path, "/");
        strcat(path, arglist[0]);
        if (execv(path, arglist) == -1)
        {
            perror("exec error");
        }
        exit(0);
    }
    else
    {
        int status = 0;
        while (waitpid(childpid, &status, 0) > 0);
        if (status != 0)
        {
            return -1;
        }
    }
    return 0;
}

int evalOperator(vector<fdcounts> &counterTable, int infd, int outfd, char *path, char *cmd, int sockfd)
{
    bool flag = false;

    vector<fdcounts>::iterator it = counterTable.begin();
    while (it != counterTable.end())
    {
        if (it->counter == 0)
        {
            flag = true;
            if (!isCmdExist(path, cmd))
            {
                writeErrcmdToSock(sockfd, cmd);
                recoverFromErrCmd(counterTable);
                return -1;
            }
            close(it->outfd);
            int status = processCmd(path, cmd, it->infd, outfd);
            if (status == -1)
            {
                return -2;
            }
            close(it->infd);
            counterTable.erase(it);
        }
        else
        {
            it++;
        }
    }

    //Nothing pipe to this and nothing pipe to
    if (!flag)
    {
        if (!isCmdExist(path, cmd))
        {
            writeErrcmdToSock(sockfd, cmd);
            recoverFromErrCmd(counterTable);
            return -1;
        }
        //read from socket, write to socket
        int status = processCmd(path, cmd, infd, outfd);
        if (status == -1)
        {
            return -2;
        }
    } 
    return 0;
}

void runShell(int sockfd)
{
    char inputBuff[MAX_INPUT_LENGTH];
    vector<fdcounts> counterTable;
    char* path;
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

            if (sign == VERTICAL_BAR)
            {
                //Execute whose counters reach 0
                int infd = sockfd;
                int outfd;
                bool flag = false;
                int err_cmd = 0;

                //Reserve pipe
                if (pipeToSame)
                {
                    outfd = pipeIt->outfd;
                }
                else
                {
                    if (pipe(pipefd) < 0)
                    {
                        perror("Pipe error!");
                        return;
                    }
                    outfd = pipefd[1];
                }

                err_cmd = evalOperator(counterTable, infd, outfd, path, cmd, sockfd);
                if (err_cmd == -1)
                {
                    if (!pipeToSame)
                    {
                        close(pipefd[0]);
                        close(pipefd[1]);
                    }
                    break;
                }
                else if (err_cmd == -2)
                {
                    return;
                }

                if (!pipeToSame)
                {
                    counterTable.push_back(fdcounts(pipefd[0], pipefd[1], pipeN));
                }
            }
            else if (sign == CLOSE_BRACKET)
            {
                //exec
                int infd = sockfd;
                int outfd = open(filename, WR_FILE_MODE, S_IWUSR|S_IRUSR);
                bool flag = false;
                int err_cmd = 0;

                if (outfd == -1)
                {
                    perror("open file error");
                    break;
                }
                err_cmd = evalOperator(counterTable, infd, outfd, path, cmd, sockfd);
                if (err_cmd == -1)
                {
                    break;
                }
                else if (err_cmd == -2)
                {
                    return;
                }
                close(outfd);
            }
            else
            {
                //exec
                int infd = sockfd;
                int outfd = sockfd;
                bool flag = false;
                int err_cmd = 0;

                err_cmd = evalOperator(counterTable, infd, outfd, path, cmd, sockfd);
                if (err_cmd == -1)
                {
                    break;
                }
                else if (err_cmd == -2)
                {
                    return;
                }
            }
        }
    }

}

int main (int argc, char *argv[])
{
    int sockfd, newsockfd, childpid;
    if (argc != 2)
    {
        printf("Usage: %s <port num>\n", argv[0]);
        return 1;
    }

    int TELNET_TCP_PORT = atoi(argv[1]);
    socklen_t clilen;
    struct sockaddr_in cli_addr, serv_addr;
    signal(SIGCHLD, SIG_IGN);

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
            welcomeTxt(newsockfd);
            runShell(newsockfd);
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

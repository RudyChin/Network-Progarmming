//
//  np_utils.cpp
//  
//
//  Created by Rudy on 2014/11/15.
//
//

#include "np_utils.h"

extern UserInfo *usrData;
extern int uid;
extern int newsockfd;

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
    while (cmd[len] != '\0' && cmd[len] != ' ')
    {
        len++;
    }
    write(sockfd, cmd, len);
    write(sockfd, "].\n", 3);
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
            sign = PIPE;
            int posOfPipeLength = 0;
            
            if (isSpace(inputBuff[i]))
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
        else if (inputBuff[i] == '>' && inputBuff[i+1] == ' ')
        {
            sign = EXPORT_FILE;
            
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
    
    if (sign == PIPE)
        n = atoi(pipeLength);
    else
        n = 0;
}

bool isNativeCmd(char *cmd)
{
    for (int i = 0; i < sizeof(nativeCmds) / sizeof(char *); i++)
    {
        if (strcmp(cmd, nativeCmds[i]) == 0)
            return true;
    }
    return false;
}

bool isCmdExist(char *cmd)
{
    char *path, *token;
    char tmp[MAX_COMMAND_LENGTH];
    char cpyCmd[MAX_COMMAND_LENGTH];
    char *pathlist[MAX_COMMAND_LENGTH];
    int pos = 0;

    while (!isSpace(cmd[pos]) && cmd[pos] != '\0')
    {
        cpyCmd[pos] = cmd[pos];
        pos++;
    }
    cpyCmd[pos] = '\0';
    
    if (isNativeCmd(cpyCmd))
    {
        return true;
    }

    path = getenv("PATH");
    token = strtok(path, ":");
    int totalpath = 0;
    while (token != NULL)
    {
        pathlist[totalpath] = strdup(token);
        totalpath++;
        token = strtok(NULL, ":");
    }

    for (int i = 0; i < totalpath; i++)
    {
        if (!strcmp(pathlist[i], "."))
        {
            strcpy(tmp, cpyCmd);
        }
        else
        {
            strcpy(tmp, pathlist[i]);
            strcat(tmp, "/");
            strcat(tmp, cpyCmd);
        }
        int fd = open(tmp, O_RDONLY);
        if (fd != -1)
        {
            close(fd);
            return true;
        }
    }
    return false;
}

void recoverFromErrCmd(vector<fdcounts> *counterTable)
{
    for (vector<fdcounts>::iterator it = counterTable->begin();
         it != counterTable->end();
         it++)
    {
        (it->counter)++;
    }
}

/**
 * Return
 *     0 indicates OK
 *     -1: fatal failure
 **/
int processCmd(char *cmd, int infd, int outfd)
{
    int totalarg, childpid;
    char *token;
    char mCmd[MAX_COMMAND_LENGTH];
    char *arglist[MAX_COMMAND_LENGTH];
    
    //Check whether there is a slash in cmd
    if (containSlash(cmd))
    {
        return -1;
    }

    strcpy(mCmd, cmd);
    
    //Initial total number of arguments
    totalarg = 0;
    //Parse arguments
    token = strtok(mCmd, " ");
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
        char *path;
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
            write(outfd, "path=", 5);
            write(outfd, path, strlen(path));
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
    else if (strcmp(arglist[0], "yell") == 0)
    {
        char tmp_msg[MAX_MSG];
        char yellmsg[MAX_MSG];
        int i = 0;
        while (i < MAX_COMMAND_LENGTH && (cmd[i++] != ' '));
        while (i < MAX_COMMAND_LENGTH && (cmd[i++] == ' '));
        i--;
        if (i < MAX_COMMAND_LENGTH)
            strcpy(tmp_msg, cmd+i);

        sprintf(yellmsg, "*** %s yelled ***: %s\n", usrData[uid].nick, tmp_msg);
        yell(yellmsg);
        return 0;
    }
    else if (strcmp(arglist[0], "tell") == 0)
    {
        int touid, pos = 0;
        char uidstr[3];
        char tmp_msg[MAX_MSG];
        char tellmsg[MAX_MSG];
        int i = 0;
        while (i < MAX_COMMAND_LENGTH && (cmd[i++] != ' '));
        //ignore spaces
        while (i < MAX_COMMAND_LENGTH && (cmd[i++] == ' '));
        i--;
        while (i < MAX_COMMAND_LENGTH && (isDigit(cmd[i])))
        {
            uidstr[pos++] = cmd[i++];
        }
        uidstr[pos] = '\0';
        touid = atoi(uidstr);
        //ignore spaces
        while (i < MAX_COMMAND_LENGTH && (cmd[i++] == ' '));
        i--;
        if (i < MAX_COMMAND_LENGTH)
            strcpy(tmp_msg, cmd+i);

        if (usrData[touid].pid != -1)
        {
            sprintf(tellmsg, "*** %s told you ***: %s\n", usrData[uid].nick, tmp_msg);
        }
        else
        {
            sprintf(tellmsg, "*** Error: user #%d does not exist yet. ***\n", touid); 
            write(newsockfd, tellmsg, strlen(tellmsg));
        }
        tell(tellmsg, touid);
        return 0;
    }
    else if (strcmp(arglist[0], "name") == 0)
    {
        bool exist = false;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (strcmp(arglist[1], usrData[i].nick) == 0)
            {
                exist = true;
                break;
            }
        }

        char yellmsg[MAX_MSG];
        if (!exist)
        {
            strcpy(usrData[uid].nick, arglist[1]);
            sprintf(yellmsg, "*** User from %s/%d is named '%s'. ***\n", usrData[uid].ip, usrData[uid].port, usrData[uid].nick);
        }
        else
        {
            sprintf(yellmsg, "*** User '%s' already exists. ***\n", arglist[1]);
        }
        yell(yellmsg);
        return 0;
    }
    else if (strcmp(arglist[0], "who") == 0)
    {
        char buf[1024];
        strcpy(buf, "<ID>\t<nickname>\t<IP/Port>\t\t<indicate me>\n");
        write(newsockfd, buf, strlen(buf));
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (usrData[i].uid != -1)
            {
                sprintf(buf, "%d\t%s\t%s/%d", usrData[i].uid, usrData[i].nick, usrData[i].ip, usrData[i].port);
                write(newsockfd, buf, strlen(buf));
                if (usrData[i].uid == uid)
                {
                    sprintf(buf, "\t\t%s\n", "<--me");
                }
                else
                {
                    sprintf(buf, "\n");
                }
                write(newsockfd, buf, strlen(buf));
            }
        }
        return 0;
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
        if (execvp(arglist[0], arglist) == -1)
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

/**
 * Return value
 *  0: success
 * -1: fatal failure
 * -2: non-fatal, no such cmd
 **/
int evalCmdOfPipe(envParcel parcel, bool pipeToSame, int pipefd[2], int pipeN, vector<fdcounts>::iterator pipeIt)
{
    //Extract data from parcel
    char wholeCmd[MAX_COMMAND_LENGTH];
    char *cmd = parcel.cmd;
    int sockfd = parcel.sockfd;
    vector<fdcounts> *counterTable = parcel.counterTable;

    strcpy(wholeCmd, cmd);
    int in_usr = extractUserOp(cmd, '<');
    
    //Execute whose counters reach 0
    int outfd, infd;
    bool flag = false;
    bool err_cmd = false;
    
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
            return -1;
        }
        outfd = pipefd[1];
    }
    
    vector<fdcounts>::iterator it = counterTable->begin();
    while (it != counterTable->end())
    {
        if (it->counter == 0)
        {
            flag = true;
            if (!isCmdExist(cmd))
            {
                writeErrcmdToSock(sockfd, cmd);
                if (!pipeToSame)
                {
                    close(pipefd[0]);
                    close(pipefd[1]);
                }
                err_cmd = true;
                break;
            }
            close(it->outfd);
            int status = processCmd(cmd, it->infd, outfd);
            if (status == -1)
            {
                //exec err
                return -1;
            }
            close(it->infd);
            counterTable->erase(it);
        }
        else
        {
            it++;
        }
    }
    
    if (err_cmd)
    {
        recoverFromErrCmd(counterTable);
        return -2;
    }
    
    //Nothing pipe to this
    if (!flag)
    {
        if (!isCmdExist(cmd))
        {
            writeErrcmdToSock(sockfd, cmd);
            if (!pipeToSame)
            {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            recoverFromErrCmd(counterTable);
            return -2;
        }

        infd = sockfd;
        //socket pipe to this
        if (in_usr)
        {
           infd = evalImport(in_usr);
           if (infd <= 0)
           {
               infd = sockfd;
           }
        }
        int status = processCmd(cmd, infd, outfd);
        if (status == -1)
        {
            return -1;
        }
        if (infd != sockfd)
        {
            char buf[MAX_MSG];
            sprintf(buf, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", usrData[uid].nick,        
                uid, usrData[in_usr].nick, in_usr, wholeCmd);
            yell(buf);
            usrData[in_usr].send_tag[uid] = 0;
            close(infd);
        }
    }
    if (!pipeToSame)
    {
        counterTable->push_back(fdcounts(pipefd[0], pipefd[1], pipeN));
    }
    return 0;
}

/**
 * Return value
 *  0: success
 * -1: fatal failure
 * -2: non-fatal, no such cmd
 **/
int evalCmdOfExport(envParcel parcel, char *filename)
{
    //Extract data from parcel
    char wholeCmd[MAX_COMMAND_LENGTH];
    char *cmd = parcel.cmd;
    int sockfd = parcel.sockfd;
    vector<fdcounts> *counterTable = parcel.counterTable;
    
    int outfd, infd;
    bool flag = false;
    bool err_cmd = false;

    strcpy(wholeCmd, cmd);
    int in_usr = extractUserOp(cmd, '<');
    
    vector<fdcounts>::iterator it = counterTable->begin();
    while (it != counterTable->end())
    {
        if (it->counter == 0)
        {
            flag = true;
            if (!isCmdExist(cmd))
            {
                writeErrcmdToSock(sockfd, cmd);
                err_cmd = true;
                break;
            }
            outfd = open(filename, WR_FILE_MODE, S_IWUSR|S_IRUSR);
            if (outfd == -1)
            {
                perror("open error");
                return -2;
            }
            close(it->outfd);
            int status = processCmd(cmd, it->infd, outfd);
            if (status == -1)
            {
                return -1;
            }
            close(it->infd);
            close(outfd);
            counterTable->erase(it);
        }
        else
        {
            it++;
        }
    }
    
    if (err_cmd)
    {
        recoverFromErrCmd(counterTable);
        return -2;
    }
    
    //Nothing pipe to this and nothing pipe to
    if (!flag)
    {
        if (!isCmdExist(cmd))
        {
            writeErrcmdToSock(sockfd, cmd);
            recoverFromErrCmd(counterTable);
            return -2;
        }

        infd = sockfd;
        if (in_usr)
        {
            infd = evalImport(in_usr);
            if (infd <= 0)
            {
                infd = sockfd;
            }
        }
        outfd = open(filename, WR_FILE_MODE, S_IWUSR|S_IRUSR);
        if (outfd == -1)
        {
            perror("open error");
            return -2;
        }
        
        //read from socket, write to socket
        int status = processCmd(cmd, infd, outfd);
        if (status == -1)
        {
            return -1;
        }
        if (infd != sockfd)
        {
            char buf[MAX_MSG];
            sprintf(buf, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", usrData[uid].nick,        
                uid, usrData[in_usr].nick, in_usr, wholeCmd);
            yell(buf);
            usrData[in_usr].send_tag[uid] = 0;
            close(infd);
        }
        close(outfd);
    }
    return 0;
}


/**
 * Return value
 *  0: success
 * -1: fatal failure
 * -2: non-fatal, no such cmd
 **/
int evalSingleCmd(envParcel parcel)
{
    //Extract data from parcel
    char wholeCmd[MAX_COMMAND_LENGTH];
    char *cmd = parcel.cmd;
    int sockfd = parcel.sockfd;
    vector<fdcounts> *counterTable = parcel.counterTable;
    
    bool flag = false;
    bool err_cmd = false;
    int infd = sockfd;
    int outfd = sockfd;
    int in_usr, out_usr;

    strcpy(wholeCmd, cmd);
    in_usr = extractUserOp(cmd, '<');
    out_usr = extractUserOp(cmd, '>');
    
    if (out_usr)
    {
        outfd = evalExport(out_usr);
        if (outfd <= 0)
        {
            outfd = sockfd;
        }
    }
    
    if (in_usr)
    {
        infd = evalImport(in_usr);
        if (infd <= 0)
        {
            infd = sockfd;
        }
    }

    vector<fdcounts>::iterator it = counterTable->begin();
    while (it != counterTable->end())
    {
        if (it->counter == 0)
        {
            flag = true;
            if (!isCmdExist(cmd))
            {
                writeErrcmdToSock(sockfd, cmd);
                err_cmd = true;
                break;
            }
            close(it->outfd);
            int status = processCmd(cmd, it->infd, outfd);
            if (status == -1)
            {
                return -1;
            }
            close(it->infd);
            counterTable->erase(it);
        }
        else
        {
            it++;
        }
    }
    
    if (err_cmd)
    {
        recoverFromErrCmd(counterTable);
        return -2;
    }
    
    //Nothing pipe to this and nothing pipe to
    if (!flag)
    {
        if (!isCmdExist(cmd))
        {
            writeErrcmdToSock(sockfd, cmd);
            recoverFromErrCmd(counterTable);
            return -2;
        }
        //read from socket, write to socket
        int status = processCmd(cmd, infd, outfd);
        if (status == -1)
        {
            return -1;
        }
        char buf[MAX_MSG];
        if (infd != sockfd)
        {
            sprintf(buf, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", usrData[uid].nick,        
                uid, usrData[in_usr].nick, in_usr, wholeCmd);
            yell(buf);
            usrData[in_usr].send_tag[uid] = 0;
            close(infd);
        }
        if (outfd != sockfd)
        {
            usrData[uid].send_tag[out_usr] = 1;
            sprintf(buf, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", usrData[uid].nick, 
                uid, wholeCmd, usrData[out_usr].nick, out_usr);
            yell(buf);
            close(outfd);
        }
    }

    return 0;
}

/**
 * Return
 *  -1 indicates unable to allocate share memory
 *   0 indicates OK
 **/
int initShm()
{
    int shmid = shmget(SHMKEY, sizeof(UserInfo)*40, IPC_CREAT | 0600);
    if (shmid < 0)
    {
        return -1;
    }
    
    if ( (usrData = (UserInfo*)shmat(shmid, NULL, 0)) < 0 )
    {
        return -1;
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        usrData[i].pid = -1;
        usrData[i].uid = -1;
        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            usrData[i].send_tag[j] = 0;
        }
    }

    return 0;
}

int exitOfUser()
{
    usrData[uid].pid = -1;
    usrData[uid].uid = -1;
    strcpy(usrData[uid].msg, " ");
}

int clientInit(struct in_addr in, unsigned short in_port)
{
    int i;
    //To find one available space for this user
    for (i = 1; i < MAX_CLIENTS; i++)
    {
        if (usrData[i].pid == -1)
        {
            char *ip_tmp;
            usrData[i].pid = getpid();
            usrData[i].uid = i;
            ip_tmp = inet_ntoa(in);
            strcpy(usrData[i].ip, ip_tmp);
            usrData[i].port = ntohs(in_port);
            strcpy(usrData[i].nick, "(no name)");
            break;
        }
    }
    return i;
}

void yell(char *msg)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (usrData[i].pid != -1)
        {
            strcpy(usrData[i].msg, msg);
            kill(usrData[i].pid, SIGALRM);
        }
    }
}

void tell(char *msg, int touid)
{
    if (usrData[touid].pid != -1)
    {
        strcpy(usrData[touid].msg, msg);
        kill(usrData[touid].pid, SIGALRM);
    }
}

bool isValidUsr(int uid)
{
    if (usrData[uid].pid != -1)
        return true;
    else
        return false;
}

int extractUserOp(char *cmd, char op)
{
    char strnum[3];
    int i = 0;
    int port = 0;
    while (i < MAX_COMMAND_LENGTH && !isReturn(cmd[i]))
    {
        if (cmd[i] == op)
        {
            cmd[i++] = ' ';
            int pos = 0;
            while (pos < 2 && !isSpace(cmd[i]))
            {
                strnum[pos++] = cmd[i];
                cmd[i++] = ' ';
            }
            strnum[pos] = '\0';
            port = atoi(strnum);
            if (port >= MAX_CLIENTS)
                port = 0;
        }
        i++;
    }
    return port;
}

int evalExport(int to)
{
    int outfd = 0;
    char buf[MAX_MSG];
    if (isValidUsr(to))
    {
        if (usrData[uid].send_tag[to] == 0)
        {
            mkdir("/u/cs/100/0016026/tmp", 0777);
            chdir("/u/cs/100/0016026/tmp");
            char file[7];
            sprintf(file, "%dto%d", uid, to);
            outfd = open(file, WR_FILE_MODE, 0777); 
            chdir("/u/cs/100/0016026/ras");
        }
        else
        {
            sprintf(buf, "*** Error: the pipe #%d->#%d already exists. ***\n", uid, to);
            write(newsockfd, buf, strlen(buf));
        }
    }
    else
    {
        sprintf(buf, "*** Error: user #%d does not exist yet. ***\n", to);
        write(newsockfd, buf, strlen(buf));
    }
    return outfd;
}

int evalImport(int from)
{
    int infd = 0;
    char buf[MAX_MSG];
    if (isValidUsr(from))
    {
        if (usrData[from].send_tag[uid] == 1)
        {
            mkdir("/u/cs/100/0016026/tmp", 0777);
            chdir("/u/cs/100/0016026/tmp");
            char file[7];
            sprintf(file, "%dto%d", from, uid);
            infd = open(file, O_RDONLY, 0777); 
            chdir("/u/cs/100/0016026/ras");
        }
        else
        {
            sprintf(buf, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", from, uid);
            write(newsockfd, buf, strlen(buf));
        }
    }
    else
    {
        sprintf(buf, "*** Error: user #%d does not exist yet. ***\n", from);
        write(newsockfd, buf, strlen(buf));
    }
    return infd;
}


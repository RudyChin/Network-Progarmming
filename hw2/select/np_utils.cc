//
//  np_utils.cpp
//  
//
//  Created by Rudy on 2014/11/15.
//
//

#include "np_utils.h"

extern UserInfo usrData[MAX_CLIENTS];
extern int uid;
extern fd_set afds;

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
    usrData[uid].is_send_to = 0;
    usrData[uid].is_receive_from = 0;
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
        else if (inputBuff[i] == '>')
        {
            if (isDigit(inputBuff[i+1]))
            {
                sign = TRANS_USER;
                char suid[3];
                int j = 0;
                while (isDigit(inputBuff[++i]))
                {
                    suid[j++] = inputBuff[i];
                }
                suid[j] = '\0';
                if (j == 0)
                    usrData[uid].is_send_to = 0;
                else
                    usrData[uid].is_send_to = atoi(suid);
            }
            else
            {
                sign = EXPORT_FILE;
                
                while (isSpace(inputBuff[++i]));
                
                while (!isSpace(inputBuff[i]) && !isReturn(inputBuff[i]))
                {
                    filename[posOfFile++] = inputBuff[i++];
                }
            }
        }
        else if (inputBuff[i] == '<')
        {
            if (isDigit(inputBuff[i+1]))
            {
                sign = TRANS_USER;
                char suid[3];
                int j = 0;
                while (isDigit(inputBuff[++i]))
                {
                    suid[j++] = inputBuff[i];
                }
                suid[j] = '\0';
                if (j == 0)
                    usrData[uid].is_receive_from = 0;
                else
                    usrData[uid].is_receive_from = atoi(suid);
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
    char path[MAX_PATH_LENGTH], *token;
    char tmp[MAX_COMMAND_LENGTH];
    char cpyCmd[MAX_COMMAND_LENGTH];
    char *pathlist[MAX_COMMAND_LENGTH];
    int pos = 0;

    strcpy(path, usrData[uid].env_val[0]);

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
        if (arglist[1] != NULL)
        {
            for (int i = 0; i < usrData[uid].env_count; i++)
            {
                if (!strcmp(usrData[uid].env[i], arglist[1]))
                {
                    char path[MAX_PATH_LENGTH];
                    sprintf(path, "%s=%s\n", usrData[uid].env[i], usrData[uid].env_val[i]);
                    write(usrData[uid].fd, path, strlen(path));
                    break;
                }
            }
        }
        else
        {
            //0 indicates PATH
            char path[MAX_PATH_LENGTH];
            sprintf(path, "%s=%s\n", usrData[uid].env[0], usrData[uid].env_val[0]);
            write(usrData[uid].fd, path, strlen(path));
        }
        return 0;
    }
    else if (strcmp(arglist[0], "setenv") == 0)
    {
        if (arglist[1] == NULL)
        {
            return 0;
        }
        bool flag = false;
        for (int i = 0; i < usrData[uid].env_count; i++)
        {
            if (!strcmp(usrData[uid].env[i],arglist[1]))
            {
                flag = true;
                strcpy(usrData[uid].env_val[i], arglist[2]);
                break;
            }
        }
        if (!flag)
        {
            int &count = (usrData[uid].env_count);
            strcpy(usrData[uid].env[count], arglist[1]);
            strcpy(usrData[uid].env_val[count], arglist[2]);
            count++;
        }
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

        if (usrData[touid].fd != -1)
        {
            sprintf(tellmsg, "*** %s told you ***: %s\n", usrData[uid].nick, tmp_msg);
        }
        else
        {
            sprintf(tellmsg, "*** Error: user #%d does not exist yet. ***\n", touid); 
            write(usrData[uid].fd, tellmsg, strlen(tellmsg));
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
        write(usrData[uid].fd, buf, strlen(buf));
        for (int i = 1; i < MAX_CLIENTS; i++)
        {
            if (usrData[i].fd != -1)
            {
                sprintf(buf, "%d\t%s\t%s/%d", usrData[i].id, usrData[i].nick, usrData[i].ip, usrData[i].port);
                write(usrData[uid].fd, buf, strlen(buf));
                if (usrData[i].id == uid)
                {
                    sprintf(buf, "\t\t%s\n", "<--me");
                }
                else
                {
                    sprintf(buf, "\n");
                }
                write(usrData[uid].fd, buf, strlen(buf));
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

        //redirect errmsg to socket
        if (dup2(usrData[uid].fd, 2) == -1)
        {
            perror("change stderr to sock error");
            exit(-1);
        }
        
        //EXEC
        //env_val[0] is PATH
        setenv(usrData[uid].env[0], usrData[uid].env_val[0], 1);
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
    char *cmd = parcel.cmd;
    int sockfd = parcel.sockfd;
    vector<fdcounts> *counterTable = parcel.counterTable;
    
    //Execute whose counters reach 0
    int outfd;
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
        
        //socket pipe to this
        int status = processCmd(cmd, sockfd, outfd);
        if (status == -1)
        {
            return -1;
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
    char *cmd = parcel.cmd;
    int sockfd = parcel.sockfd;
    vector<fdcounts> *counterTable = parcel.counterTable;
    
    int outfd;
    bool flag = false;
    bool err_cmd = false;
    
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
        outfd = open(filename, WR_FILE_MODE, S_IWUSR|S_IRUSR);
        if (outfd == -1)
        {
            perror("open error");
            return -2;
        }
        
        //read from socket, write to socket
        int status = processCmd(cmd, sockfd, outfd);
        if (status == -1)
        {
            return -1;
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
int evalTransUser(envParcel parcel)
{
    //Extract data from parcel
    char *cmd = parcel.cmd;
    int sockfd = parcel.sockfd;
    vector<fdcounts> *counterTable = parcel.counterTable;
 
    mkdir("/u/cs/100/0016026/tmp", 0777);
    chdir("/u/cs/100/0016026/tmp");
    char file[7];
    int outfd = sockfd, infd = sockfd;
    if (usrData[uid].is_receive_from)
    {   
        sprintf(file, "%dto%d", usrData[uid].is_receive_from, uid);
        infd = open(file, O_RDONLY, 0777); 
    }
    if (usrData[uid].is_send_to)
    {
        usrData[uid].send_tag[usrData[uid].is_send_to] = 1;
        sprintf(file, "%dto%d", uid, usrData[uid].is_send_to);
        outfd = open(file, WR_FILE_MODE, 0777);
    }
    chdir("/u/cs/100/0016026/ras");
       
    bool flag = false;
    bool err_cmd = false;
    
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
        close(outfd);
        return -2;
    }
    
    //Nothing pipe to this and nothing pipe to
    if (!flag)
    {
        if (!isCmdExist(cmd))
        {
            writeErrcmdToSock(sockfd, cmd);
            recoverFromErrCmd(counterTable);
            close(outfd);
            return -2;
        }
        //read from socket, write to socket
        int status = processCmd(cmd, infd, outfd);
        if (status == -1)
        {
            return -1;
        }
    }
    if (outfd != sockfd)
    {
        close(outfd);
    }
    if (infd != sockfd)
    {
        usrData[usrData[uid].is_receive_from].send_tag[uid] = 0;
        close(infd); 
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
    char *cmd = parcel.cmd;
    int sockfd = parcel.sockfd;
    vector<fdcounts> *counterTable = parcel.counterTable;
    
    bool flag = false;
    bool err_cmd = false;
    
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
            int status = processCmd(cmd, it->infd, sockfd);
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
        int status = processCmd(cmd, sockfd, sockfd);
        if (status == -1)
        {
            return -1;
        }
    }
    return 0;
}

void init(int uid)
{
    usrData[uid].fd = -1;
    usrData[uid].id = -1;
    strcpy(usrData[uid].nick, "(no name)");
    usrData[uid].is_send_to = 0;
    usrData[uid].is_receive_from = 0;
    strcpy(usrData[uid].ip, " ");
    strcpy(usrData[uid].env[0], "PATH");
    strcpy(usrData[uid].env_val[0], "bin:.");
    usrData[uid].env_count = 1;
    usrData[uid].port = 0;
    usrData[uid].counterTable.clear();
}

void yell(char *msg)
{
    for (int i = 1; i < MAX_CLIENTS; i++)
    {
        if (usrData[i].fd != -1)
        {
            write(usrData[i].fd, msg, strlen(msg));
        }
    }
}

void tell(char *msg, int touid)
{
    if (usrData[touid].fd != -1)
    {
        write(usrData[touid].fd, msg, strlen(msg));
    }
}

bool isValidUsr(int uid)
{
    if (usrData[uid].fd != -1)
        return true;
    else
        return false;
}


int getUserId()
{
    for (int i = 1; i < MAX_CLIENTS; i++)
    {
        if (usrData[i].fd == -1)
            return i;
    }
    return 0;
}

void prompt()
{
    write(usrData[uid].fd, "% ", 2);
}

void usr_exit()
{
    char buf[MAX_MSG];
    sprintf(buf, "*** User '%s' left. ***\n", usrData[uid].nick);
    FD_CLR(usrData[uid].fd, &afds);
    close(usrData[uid].fd);
    init(uid);
    yell(buf);
}

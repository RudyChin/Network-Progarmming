//
//  np_utils.h
//  
//
//  Created by Rudy on 2014/11/15.
//
//

#ifndef _np_utils_h


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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>


#define MAX_INPUT_LENGTH 10001
#define MAX_MSG 1025
#define MAX_PIPE_LENGTH 5
#define MAX_COMMAND_LENGTH 257
#define MAX_PATH_LENGTH 257
#define MAX_CLIENTS 30
#define TELNET_TCP_PORT 20013
#define SHMKEY 989 
#define ORIGINAL_PATH "bin:."
#define WR_FILE_MODE (O_CREAT|O_WRONLY|O_TRUNC)

using namespace std;

static const char *nativeCmds[] = {
    "printenv",
    "setenv",
    "exit",
    "yell",
    "name",
    "who",
    "tell"
};

struct UserInfo
{
    int pid;
    int uid;
    char nick[20];
    char ip[20];
    char msg[MAX_MSG];
    unsigned short port;
    int send_tag[30];
};

/* Data structure that the pipe counter needs */
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

/* Distinguish between |, >, empty */
enum Sign
{
    PIPE,
    EXPORT_FILE,
    TRANS_USER,
    NONE
};

/* The data structure flows throught diff stages of this program */
struct envParcel
{
    vector<fdcounts> *counterTable;
    char *cmd;
    int sockfd;
    
    envParcel(vector<fdcounts> *counterTable, char *cmd, int sockfd)
    {
        this->counterTable = counterTable;
        this->cmd = cmd;
        this->sockfd = sockfd;
    }
};

bool isReturn(char c);

bool isDigit(char c);

bool isSpace(char c);

/* Print welcome text to sockfd */
void welcomeTxt(int sockfd);

/* Check whether cmd contains '/' */
bool containSlash(char *cmd);

/* Unknown cmd message */
void writeErrcmdToSock(int sockfd, char *cmd);

/**
 * Description
 *  To parse input buffer into command (and filename). Fill up other attributes needed later.
 *
 * Input
 *  inputBuff: the raw input string
 *
 * Output
 *  i: current position of inputBuff
 *  cmd: parsed command
 *  hasPipeSign: whether there is a '|' in command
 *  n: the number behind '|' sign
 **/
void parseCmd(int &i, char *inputBuff, char *cmd, char *filename, Sign &sign, int &n);

bool isCmdExist(char *cmd);

/**
 * Description
 *  To increase all the counter by 1.
 **/
void recoverFromErrCmd(vector<fdcounts> *counterTable);

/**
 * Description
 *  To fork and exec the command
 *
 * Return
 *     0 indicates OK
 *     -1: fatal failure
 **/
int processCmd(char *cmd, int infd, int outfd);

/**
 * Description
 *  Evaluate the attributes of command with pipes and trigger processCmd
 *
 * Return value
 *  0: success
 * -1: fatal failure
 * -2: non-fatal, no such cmd
 **/
int evalCmdOfPipe(envParcel parcel, bool pipeToSame, int pipefd[2], int pipeN, vector<fdcounts>::iterator pipeIt);

/**
 * Description
 *  Evaluate the attributes of command with '>' and trigger processCmd
 *
 * Return value
 *  0: success
 * -1: fatal failure
 * -2: non-fatal, no such cmd
 **/
int evalCmdOfExport(envParcel parcel, char *filename);

/**
 * Description
 *  Evaluate the attributes single command and trigger processCmd
 *
 * Return value
 *  0: success
 * -1: fatal failure
 * -2: non-fatal, no such cmd
 **/
int evalSingleCmd(envParcel parcel);


int evalTransUser(envParcel parcel);

bool isValidUsr(int uid);

bool isNativeCmd(char *cmd);

int initShm();

int exitOfUser();

int clientInit(struct in_addr in, unsigned short in_port);

void yell(char *msg);

void tell(char *msg, int touid);

int extractUserOp(char *cmd, char op);

int evalExport(int to);

int evalImport(int from);
#define _np_utils_h


#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <vector>

#define MAX_BUF_LEN 10001
#define SHMKEY 989 


using namespace std;

int readline(int fd, char* ptr, int maxlen);

//SOCK4_REQUEST
const unsigned char * getDomainName(const unsigned char * buf);

int connectTCP(unsigned ip, unsigned port);

int passiveTCP(unsigned port);

void redirectData(int ssock, int rsock);

bool checkFirewall(vector<unsigned int>& permit_C, vector<unsigned int>& permit_B, vector<unsigned int>& deny_C, vector<unsigned int>& deny_B, const char * filename ,  unsigned char CD , unsigned int ip);

void child_term_handler(int no);
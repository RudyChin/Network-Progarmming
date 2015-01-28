#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <windows.h>
#include <list>
using namespace std;

#include "resource.h"

#define SERVER_PORT 7799
#define WM_SOCKET_NOTIFY (WM_USER + 1)
#define HOSTS_SOCKET_NOTIFY 888
#define MAX_HOST 6
#define EXIT_STR "exit\r\n"
#define MAX_BUFFER_LENGTH 10001


struct hostinfo
{
	FILE *file_fd;
	char ip[20];
	int port;
	int write_enable;
	int unsend;
	bool available;
	SOCKET client_fd;

	hostinfo() {
		available = true;
		write_enable = 0;
		unsend = 0;
	}
};

struct Request
{
	char cmd[128];
	char file[128];
	char arg[MAX_BUFFER_LENGTH];
	char protocol[128];
};


struct Request request;
struct hostinfo params[MAX_HOST];
char buffer[MAX_BUFFER_LENGTH], buf[MAX_BUFFER_LENGTH];
char msg_buf[MAX_HOST][MAX_BUFFER_LENGTH];

static SOCKET msock, ssock;
static struct sockaddr_in client_sin;
struct hostent *he;


int extractRequest(char *buffer, Request &request)
{
	char *tok, *tmp;
	int i = 0;
	for (tmp = strtok(buffer, " ?\n"); tmp; tmp = strtok(NULL, " ?\n"))
	{
		if (i == 0)
		{
			strcpy(request.cmd, tmp);
		}
		else if (i == 1)
		{
			strcpy(request.file, tmp + 1);
		}
		else if (i == 2)
		{
			strcpy(request.arg, tmp);
		}
		else if (i == 3)
		{
			strcpy(request.protocol, tmp);
		}
		i++;
	}

	if (i == 4)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

int contain_prompt(char* line)
{
	int i, prompt = 0;
	for (i = 0; line[i]; ++i) {
		switch (line[i]) {
		case '%': prompt = 1; break;
		case ' ': if (prompt) return 1;
		default: prompt = 0;
		}
	}
	return 0;
}

int readline(FILE *fd, char *ptr, int maxlen)
{
	int n;
	char c;
	*ptr = 0;
	for (n = 1; n < maxlen; n++)
	{
		c = fgetc(fd);
		if (c != EOF)
		{
			*ptr++ = c;
			if (c == '\n')  break;
		}
		else
		{
			if (n == 1)     return 0;
			else         break;
		}
	}
	return n;
}

void printHtml(char *msg, int to, bool bold)
{
	char buffer[MAX_BUFFER_LENGTH];
	sprintf(buffer, "<script>document.all['m%d'].innerHTML +=\"", to);
	send(ssock, buffer, strlen(buffer), 0);
	int i = 0;
	if (bold)
		send(ssock, "<b>", 3, 0);
	while (msg[i] != '\0')
	{
		switch (msg[i])
		{
		case '<':
			send(ssock, "&lt", 3, 0);
			break;
		case '>':
			send(ssock, "&gt", 3, 0);
			break;
		case ' ':
			send(ssock, "&nbsp;", 6, 0);
			break;
		case '\r':
			if (msg[i + 1] == '\n')
			{
				send(ssock, "<br>", 4, 0);
				i++;
			}
			break;
		case '\n':
			send(ssock, "<br>", 4, 0);
			break;
		case '\\':
			send(ssock, "&#039", 5, 0);
			break;
		case '\"':
			send(ssock, "&quot;", 6, 0);
			break;
		default:
			sprintf(buffer, "%c", msg[i]);
			send(ssock, buffer, 1, 0);
			break;
		}
		i++;
	}
	if (bold)
		send(ssock, "</b>", 4, 0);
	send(ssock, "\";</script>\n", strlen("\";</script>\n"), 0);
}

int recv_msg(int userno, SOCKET from)
{
	char buf[3000], *tmp;
	int len, i;

	len = recv(from, buf, sizeof(buf)-1, 0);
	if (len < 0) return -1;

	buf[len] = 0;
	if (len > 0)
	{
		for (tmp = strtok(buf, "\n"); tmp; tmp = strtok(NULL, "\n"))
		{
			if (contain_prompt(tmp))
			{
				params[userno].write_enable = 1;
				printHtml(tmp, userno, false);
			}
			else
			{
				char lineFeed[3000];
				strcpy(lineFeed, tmp);
				strcat(lineFeed, "\n");
				printHtml(lineFeed, userno, false);
			}

		}
	}
	fflush(stdout);
	return len;
}


void send_header()
{
	send(ssock,
		"<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />\
				        <title>Network Programming Homework 3</title></head><body bgcolor=#336699>\
										        <font face=\"Courier New\" size=2 color=#FFFF99>",
												strlen("<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />\
													   							           <title>Network Programming Homework 3</title></head><body bgcolor=#336699>\
																						   									           <font face=\"Courier New\" size=2 color=#FFFF99>"), 0);
}

void send_end()
{
	send(ssock, "</font></body></html>", strlen("</font></body></html>"), 0);
}

void parseArg(char *env)
{
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
				strcpy(params[index].ip, token + 3);
				if (!strcmp(params[index].ip, ""))
				{
					params[index].available = false;
				}
			}
			else if (token[0] == 'p')
			{
				char port[6];
				strcpy(port, token + 3);
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
				strcpy(batch_file, token + 3);
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
				}
			}

			token = strtok(NULL, "&");
		}
	}
}

void sendCGI()
{
	char buffer[MAX_BUFFER_LENGTH];
	send_header();
	send(ssock, "<table width=\"800\" border=\"1\">\n", strlen("<table width=\"800\" border=\"1\">\n"), 0);
	send(ssock, "<tr>\n", strlen("<tr>\n"), 0);
	for (int i = 0; i < MAX_HOST; i++)
	{
		if (params[i].available)
		{
			sprintf(buffer, "<td>%s</td>\n", params[i].ip);
			send(ssock, buffer, strlen(buffer), 0);
		}
	}
	send(ssock, "</tr>\n", strlen("</tr>\n"), 0);
	send(ssock, "<tr>\n", strlen("<tr>\n"), 0);
	for (int i = 0; i < MAX_HOST; i++)
	{
		if (params[i].available)
		{
			sprintf(buffer, "<td valign=\"top\" id=\"m%d\"></td>\n", i);
			send(ssock, buffer, strlen(buffer), 0);
		}
	}
	send(ssock, "</tr>\n", strlen("</tr>\n"), 0);
	send(ssock, "</table>\n", strlen("</table>\n"), 0);
	send_end();
}

void send_http_header()
{
	send(ssock, "HTTP/1.1 200 OK\r\n", 17, 0);
	send(ssock, "Server: rudyhttpd\r\n", 19, 0);
	send(ssock, "Content-Type: text/html\r\n", 25, 0);
	send(ssock, "\r\n", 2, 0);
}

void connect_hosts()
{
	for (int i = 1; i < MAX_HOST; i++)
	{
		if (params[i].available)
		{
			he = gethostbyname(params[i].ip);
			params[i].client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			client_sin.sin_family = AF_INET;
			client_sin.sin_addr = *((struct in_addr *)he->h_addr);
			client_sin.sin_port = htons((u_short)params[i].port);

			if (connect(params[i].client_fd, (struct sockaddr *)&client_sin, sizeof(client_sin)) == -1)
			{
				perror("connect");
			}
		}
	}
}

BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int EditPrintf(HWND, TCHAR *, ...);
//=================================================================
//	Global Variables
//=================================================================
list<SOCKET> Socks;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	return DialogBox(hInstance, MAKEINTRESOURCE(ID_MAIN), NULL, MainDlgProc);
}

BOOL CALLBACK MainDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	WSADATA wsaData;

	static HWND hwndEdit;
	static struct sockaddr_in sa;

	int err;
	int i, len;
	bool skip;


	switch (Message)
	{
	case WM_INITDIALOG:
		hwndEdit = GetDlgItem(hwnd, IDC_RESULT);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_LISTEN:

			WSAStartup(MAKEWORD(2, 0), &wsaData);

			//create master socket
			msock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

			if (msock == INVALID_SOCKET) {
				EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
				WSACleanup();
				return TRUE;
			}

			err = WSAAsyncSelect(msock, hwnd, WM_SOCKET_NOTIFY, FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE);

			if (err == SOCKET_ERROR) {
				EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
				closesocket(msock);
				WSACleanup();
				return TRUE;
			}

			//fill the address info about server
			sa.sin_family = AF_INET;
			sa.sin_port = htons(SERVER_PORT);
			sa.sin_addr.s_addr = INADDR_ANY;

			//bind socket
			err = bind(msock, (LPSOCKADDR)&sa, sizeof(struct sockaddr));

			if (err == SOCKET_ERROR) {
				EditPrintf(hwndEdit, TEXT("=== Error: binding error ===\r\n"));
				WSACleanup();
				return FALSE;
			}

			err = listen(msock, 2);

			if (err == SOCKET_ERROR) {
				EditPrintf(hwndEdit, TEXT("=== Error: listen error ===\r\n"));
				WSACleanup();
				return FALSE;
			}
			else {
				EditPrintf(hwndEdit, TEXT("=== Server START ===\r\n"));
			}

			break;
		case ID_EXIT:
			EndDialog(hwnd, 0);
			break;
		};
		break;

	case WM_CLOSE:
		EndDialog(hwnd, 0);
		break;

	case WM_SOCKET_NOTIFY:
		switch (WSAGETSELECTEVENT(lParam))
		{
		case FD_ACCEPT:
			ssock = accept(msock, NULL, NULL);
			Socks.push_back(ssock);
			EditPrintf(hwndEdit, TEXT("=== Accept one new client(%d), List size:%d ===\r\n"), ssock, Socks.size());
			break;
		case FD_READ:
			//Write your code for read event here.
			err = recv(ssock, buffer, sizeof(buffer), 0);
			if (err == SOCKET_ERROR)
			{
				EditPrintf(hwndEdit, TEXT("receive failed: %d\r\n"), WSAGetLastError());
			}
			else
			{
				EditPrintf(hwndEdit, TEXT("buffer:[%s]\r\n"), buffer);
				extractRequest(buffer, request);
				parseArg(request.arg);
				for (int i = 1; i < MAX_HOST; i++)
				{
					if (params[i].available)
					{
						EditPrintf(hwndEdit, TEXT("IP:[%s]\r\n"), params[i].ip);
					}
				}

				send_http_header();
				sendCGI();

				connect_hosts();
				for (int i = 1; i < MAX_HOST; i++)
				{
					if (params[i].available)
					{
						err = WSAAsyncSelect(params[i].client_fd, hwnd, HOSTS_SOCKET_NOTIFY, FD_READ | FD_WRITE);

						if (err == SOCKET_ERROR) {
							EditPrintf(hwndEdit, TEXT("=== Error: select error for host [%d]===\r\n"), i);
							closesocket(params[i].client_fd);
						}
					}
				}
			}
			break;
		case FD_WRITE:
			//Write your code for write event here
			break;
		case FD_CLOSE:
			break;
		};
		break;

	case HOSTS_SOCKET_NOTIFY:
		switch (WSAGETSELECTEVENT(lParam))
		{
		case FD_READ:
			//Write your code for read event here.
			for (i = 1; i < MAX_HOST; i++)
			{
				if (params[i].available && params[i].client_fd == wParam)
					break;
			}
			EditPrintf(hwndEdit, TEXT("=== Host FD_READ begin i:[%d]===\r\n"), i);

			if (recv_msg(i, params[i].client_fd) < 0)
			{
				EditPrintf(hwndEdit, TEXT("=== Host FD_READ failed! i:[%d]===\r\n"), i);
				closesocket(params[i].client_fd);
				params[i].available = false;
			}
			EditPrintf(hwndEdit, TEXT("=== Host FD_READ end i:[%d]===\r\n"), i);

			EditPrintf(hwndEdit, TEXT("=== Host FD_WRITE begin i:[%d]===\r\n"), i);

			if (!params[i].unsend)
			{
				//送meesage
				skip = false;
				len = readline(params[i].file_fd, msg_buf[i], sizeof(msg_buf[i])-1);
				while (len == sizeof(msg_buf[i])-1)
				{
					skip = true;
					len = readline(params[i].file_fd, msg_buf[i], sizeof(msg_buf[i])-1);
				}
				if (skip)
					len = readline(params[i].file_fd, msg_buf[i], sizeof(msg_buf[i])-1);
				msg_buf[i][len] = '\0';
				EditPrintf(hwndEdit, TEXT("=== Host get cmd [%s]===\r\n"), msg_buf[i]);
				fflush(stdout);
			}

			params[i].unsend = 0;
			if (!strncmp(msg_buf[i], "exit", 4))  // exit all
			{
				if (params[i].write_enable)
				{
					EditPrintf(hwndEdit, TEXT("=== Host exit begin i:[%d]===\r\n"), i);
					if (send(params[i].client_fd, EXIT_STR, 6, 0) != 6)
					{
						params[i].available = false;
						break;
					}

					params[i].write_enable = 0;
					printHtml(EXIT_STR, i, true);
					while (recv_msg(i, params[i].client_fd) > 0);
					closesocket(params[i].client_fd);
					fclose(params[i].file_fd);
					params[i].available = false;

					bool running = false;
				    for (int j = 1; j < MAX_HOST; j++)
				    {
				        running = running | params[j].available;
				    }
				    if (!running)
				    {
				    	closesocket(ssock);
				    }
					EditPrintf(hwndEdit, TEXT("=== Host exit end i:[%d]===\r\n"), i);
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
					if (send(params[i].client_fd, msg_buf[i], strlen(msg_buf[i]), 0) != strlen(msg_buf[i]))
					{
						//handle write error
					}
					printHtml(msg_buf[i], i, true);
					EditPrintf(hwndEdit, TEXT("=== Host send [%s]===\r\n"), msg_buf[i]);
					params[i].write_enable = 0;
				}
				else
				{
					EditPrintf(hwndEdit, TEXT("=== Host send write_enable=0 [%d]===\r\n"), i);
					params[i].unsend = 1;
				}
			}

			EditPrintf(hwndEdit, TEXT("=== Host FD_WRITE end i:[%d]===\r\n"), i);
			break;

		case FD_WRITE:
			//Write your code for write event here
			break;
		};
		break;

	default:
		return FALSE;

	};

	return TRUE;
}

int EditPrintf(HWND hwndEdit, TCHAR * szFormat, ...)
{
	TCHAR   szBuffer[1024];
	va_list pArgList;

	va_start(pArgList, szFormat);
	wvsprintf(szBuffer, szFormat, pArgList);
	va_end(pArgList);

	SendMessage(hwndEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
	SendMessage(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)szBuffer);
	SendMessage(hwndEdit, EM_SCROLLCARET, 0, 0);
	return SendMessage(hwndEdit, EM_GETLINECOUNT, 0, 0);
}
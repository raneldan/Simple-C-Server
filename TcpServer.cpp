#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>

#define DEFUALT_BUFFER_SIZE 2047

#define KEY_OFFSET 2
#define ROUTE_OFFSET 2

#define CARRIGE_RETURN '\r'
#define NEW_LINE '\n'
#define STRING_TERMINATOR '\0'


enum state {EMPTY = 0 , LISTEN,  RECEIVE , IDLE, SEND};
enum options {EXIT = 0, GET, POST, PUT, DELET, HEAD, TRACE, OPTIONS};


struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	int sendSubType;	// Sending sub-type
	char buffer[DEFUALT_BUFFER_SIZE];
	char sendBuff[DEFUALT_BUFFER_SIZE];
	int len;
};


const int TIME_PORT = 27015;
const int MAX_SOCKETS = 60;

const char* LANG = "lang";
const char SEPERATOR = '%';
const char EQUAL = '=';
const char SPACE = ' ';

// log levels
const char* DEBUG = "DEBUG";
const char* ALERT = "ALERT";
const char* WARN = "WARN";
const char* ERR = "ERROR";

bool addSocket(SOCKET, int);
void removeSocket(int);
void acceptConnection(int);
void receiveMessage(int);
void sendMessage(int);
int getIndexOfChar(char*, char);
char* findPageOnDB(char*);

void logMessage(const char*, const char*);
int findCRLF(char* req);

int getRequestedMethod(char*);
char* getRequsetedRoute(char* req);
char* getHeaderValue(char*, const char*);
char* getMessageBody(char*, int);

// handle method requests
void handleGetRequest(int);
void handlePostRequest(int);
void handlePutRequest(int);
void handleDeleteRequest(int);
void handleHeadRequest(int);
void handleTraceRequest(int);
void handleOptionsRequest(int);
void handleUnkownRequest(int);

struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;


void main() {

	WSAData wsaData;

	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Time Server: Error at WSAStartup()\n";
		return;
	}

	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Time Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

	sockaddr_in serverService;
	// Address family (must be AF_INET - Internet address family).
	serverService.sin_family = AF_INET;

	serverService.sin_addr.s_addr = INADDR_ANY;

	serverService.sin_port = htons(TIME_PORT);

	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Time Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Time Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	addSocket(listenSocket, LISTEN);

	// Accept connections and handles them one by one.
	while (true)
	{

		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++) {
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE)) {
				FD_SET(sockets[i].id, &waitRecv);
			}
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++) {
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}

		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, NULL);
		if (nfd == SOCKET_ERROR) {
			cout << "Time Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i);
					break;

				case RECEIVE:
					receiveMessage(i);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++) {
			if (FD_ISSET(sockets[i].id, &waitSend)) {
				nfd--;
				switch (sockets[i].send) {
					case SEND:
						sendMessage(i);
						break;
				}
			}
		}
	}

	cout << "Time Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

bool addSocket(SOCKET id, int what) {
	for (int i = 0; i < MAX_SOCKETS; i++) {
		if (sockets[i].recv == EMPTY) {
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index) {
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	socketsCount--;
}

void acceptConnection(int index) {
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*) & from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Time Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Time Server: Client "  << ":" << ntohs(from.sin_port) << " is connected." << endl;

	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Time Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void receiveMessage(int index) {
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv) {
		cout << "Time Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	if (bytesRecv == 0) {
		closesocket(msgSocket);
		removeSocket(index);
		return;
	} else {
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "Time Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].len += bytesRecv;	

		if (sockets[index].len > 0) {
			int option = getRequestedMethod(sockets[index].buffer);
			switch (option) {
			case GET:
				handleGetRequest(index);
				break;
			case POST:
				handlePostRequest(index);
				break;
			case PUT:
				handlePutRequest(index);
				break;
			case DELET:
				handleDeleteRequest(index);
				break;
			case HEAD:
				handleHeadRequest(index);
				break;
			case TRACE:
				handleTraceRequest(index);
				break;
			case OPTIONS:
				handleOptionsRequest(index);
				break;
			default:
				handleUnkownRequest(index);
				break;
			}
		}
	}
}

void sendMessage(int index) {
	int bytesSent = 0;

	SOCKET msgSocket = sockets[index].id;
	// Answer client's request by the current time string.


	bytesSent = send(msgSocket, sockets[index].sendBuff,
		(int)strlen(sockets[index].sendBuff), 0);
	if (SOCKET_ERROR == bytesSent) {
		cout << "Time Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "Time Server: Sent: " << bytesSent << "\\" <<
		strlen(sockets[index].sendBuff) << " bytes of \"" <<
		sockets[index].sendBuff << "\" message.\n";

	sockets[index].send = IDLE;
}

int getIndexOfChar(char* req, char charToSearch) {
	if (strchr(req, charToSearch) != NULL) {
		return (int)(strchr(req, charToSearch) - req);
	}
	return strlen(req);
}

char* findPageOnDB(char* page) {
	char result[DEFUALT_BUFFER_SIZE];
	if (rand() > 0.5) {
		strcpy(result, "PAGE NOT FOUND");
	} else {
		strcpy(result, "really intresting page");
	}
	return result;
}

int getRequestedMethod(char* req) {
	int index = getIndexOfChar(req, SPACE);
	char method[DEFUALT_BUFFER_SIZE];
	strncpy(method, req, index);
	method[index] = '\0';
	if (!strcmp(method, "GET")) {
		return GET;
	}
	if (!strcmp(method, "POST")) {
		return POST;
	}
	if (!strcmp(method, "PUT")) {
		return POST;
	}
	if (!strcmp(method, "DELETE")) {
		return DELET;
	}
	if (!strcmp(method, "HEAD")) {
		return HEAD;
	}
	if (!strcmp(method, "TRACE")) {
		return TRACE;
	}
	if (!strcmp(method, "OPTIONS")) {
		return OPTIONS;
	}
	return 0;
}

//void handleGetRequest(int index) {
//	char* req = sockets[index].buffer;
//	int endOfReqIndex = getIndexOfChar(req, SEPERATOR);
//	char page[DEFUALT_BUFFER_SIZE];
//	memcpy(page, req + 1, endOfReqIndex - 1);
//	page[endOfReqIndex - 1] = '\0';
//	strcpy(sockets[index].sendBuff, findPageOnDB(page));
//	sockets[index].send = SEND;
//}


void handleGetRequest(int index) {
	char* lang = (char*) malloc(3);
	lang = getHeaderValue(sockets[index].buffer, "lang");
	cout << findPageOnDB(getRequsetedRoute(sockets[index].buffer));
}

void handlePostRequest(int index) {
	int bodyLength = atoi(getHeaderValue(sockets[index].buffer, "Content-Length"));
	cout << getMessageBody(sockets[index].buffer, bodyLength) << NEW_LINE;
}

char* getRequsetedRoute(char* req) {
	char route[DEFUALT_BUFFER_SIZE];
	int startOfRoute = getIndexOfChar(req, SPACE);
	req += startOfRoute + ROUTE_OFFSET;
	int endOfRoute = getIndexOfChar(req, SPACE);
	strncpy(route, req, endOfRoute);
	route[endOfRoute] = STRING_TERMINATOR;
	return route;
}

char* getMessageBody(char* req, int length) {
	char* result = (char*) malloc(length + sizeof(STRING_TERMINATOR));
	int index = findCRLF(req);
	strncpy(result, req + index, length);
	result[length] = STRING_TERMINATOR;
	return result;
}

char* getHeaderValue(char* req, const char* key) {
	char* result = (char*)malloc(DEFUALT_BUFFER_SIZE);
	char* point = strstr(req, key);
	if (point != NULL) {
		point += strlen(key) + KEY_OFFSET;
		int index = getIndexOfChar(point, CARRIGE_RETURN);
		strncpy(result, point, index);
		result[index] = STRING_TERMINATOR;
	} else {
		result = NULL;
		logMessage("Header not found! \n", WARN);
	}
	return result;
}

void handlePutRequest(int index) {

}

void handleDeleteRequest(int index) {

}

void handleHeadRequest(int index) {

}

void handleTraceRequest(int index) {

}

void handleOptionsRequest(int index) {

}

void handleUnkownRequest(int index) {
	logMessage("Got unkown request \n" , WARN);
}

void logMessage(const char* message, const char* level) {
	cout << "Server " << level << ':' << NEW_LINE << message << NEW_LINE;
}


int findCRLF(char* req) {
	char* search = req;
	int index = 0;
	while (*search != STRING_TERMINATOR) {
		if (*search == NEW_LINE) {
			search++;
			index++;
		}
		if (*search == CARRIGE_RETURN) {
			if (*(search + 1) == NEW_LINE) {
				if (*(search + 2) == CARRIGE_RETURN) {
					if (*(search + 3) == NEW_LINE) {
						return index + 4;
					}
				}
			}
			else {
				search++;
				index++;
			}
		}
		search++;
		index++;
	}
	return index;
}

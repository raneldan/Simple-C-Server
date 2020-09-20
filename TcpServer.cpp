#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib, "Ws2_32.lib")
#include <iostream>
#include <winsock2.h>
#include <string.h>
#include <string>
#include <time.h>
#include <vector>
using namespace std;


#define DEFUALT_BUFFER_SIZE 4096
#define SMALL_BUFFER_SIZE 3
#define CONNECTION_TIMEOUT 10

#define KEY_OFFSET 2
#define ROUTE_OFFSET 1
#define PARAM_OFFSET 1
#define NOT_EXIST -1

#define CARRIGE_RETURN '\r'
#define NEW_LINE '\n'
#define STRING_TERMINATOR '\0'

#define PROTOCOL_VERSION "HTTP/1.1"

#define EMPTY_BODY ""

enum state {EMPTY = 0 , LISTEN,  RECEIVE , IDLE, SEND};
enum options {EXIT = 0, GET, POST, PUT, DELET, HEAD, TRACE, OPTIONS};

class Page {
public:
	string name;
	string content;
	string lang;

	Page(string name, string content="", string lang="en") {
		this->name = name;
		this->content = content;
		this->lang = lang;
	}
};

struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	int sendSubType;	// Sending sub-type
	char buffer[DEFUALT_BUFFER_SIZE];
	char sendBuff[DEFUALT_BUFFER_SIZE];
	int len;
	time_t lastActivity;
};

const int TIME_PORT = 27015;
const int MAX_SOCKETS = 60;

// status code
const char* OK = " 200 OK \r\n";
const char* CREATED = " 201 Created \r\n";
const char* NO_CONTENT = " 204 No Content \r\n";
const char* BAD_REQUEST = " 400 Bad Request \r\n";
const char* NOT_FOUND = " 404 Not Found \r\n";
const char* INTERNAL_SERVER_ERROR = " 500 Internal Server Error \r\n";
const char* NOT_IMPLEMENTED = " 501 Not Implemented \r\n";

// chars 
const char* LANG = "lang";
const char* DEFUALT_ROUTE = "";
const char SEPERATOR = '%';
const char EQUAL = '=';
const char SPACE = ' ';
const char QMARK = '?';
const char SLASH = '/';

const char* EN = "en";
const char* HE = "he";

// log levels
const char* INFO = "INFO";
const char* DEBUG = "DEBUG";
const char* ALERT = "ALERT";
const char* WARN = "WARN";
const char* ERR = "ERROR";

// DB api
void initDB();
bool addPageToDB(Page);
bool removePageFromDB(Page);
int isPageExistInDB(Page);
bool editPageOnDB(Page, Page);
string getAllPages();
string getAllPagesInLang(char*);
string formatPage(vector<Page>::iterator);
string getPageByName(const char*);
string getPageByNameAndLang(const char*, char*);

// connection handling
bool addSocket(SOCKET, int);
void removeSocket(int);
void acceptConnection(int);
void receiveMessage(int);
void sendMessage(int);

// util
int getIndexOfChar(char*, char);
void logMessage(const char*, const char*);
int findCRLF(char* req);

// formatting
void formatResponse(int, const char*, const char*, options);
void formatGetResponse(int, const char*, const char*);\
void formatPostResponse(int, const char*, const char*);
void formatHeadResponse(int, const char*, const char*);
void formatOptionsResponse(int, char*, const char*);
void formatTraceResponse(int, char*, const char*);
void formatDeleteResponse(int, const char*, const char*);
void formatPutResponse(int, const char*, const char*);
void formatUnkownRequestResponse(int, const char*, const char*);
void formatStatusLine(int, const char*);

// headers
void addHeaders(int, const char*, options);
void addServerHeader(int);
void addContetntTypeHeader(int, options);
void addDateHeader(int);
void addLengthHeader(int, int);
void addAllowHeader(int);

// message 
int calculateBodyLength(const char*, options);
const char* calculateStatusCode(int);
int getRequestedMethod(char*);
bool checkIfParamExist(char*);
char* getRequsetedRoute(char*);
char* getRequestParamValue(char*);
char* getHeaderValue(char*, const char*);
char* getMessageBody(char*, int);
int getRequestBodyLength(int);

// handle method requests
void handleGetRequest(int);
void handlePostRequest(int);
void handlePutRequest(int);
void handleDeleteRequest(int);
void handleHeadRequest(int);
void handleTraceRequest(int);
void handleOptionsRequest(int);
string handlePageRequest(int);
void handleUnkownRequest(int);


vector<Page> pagesDB;
struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;

void main() {
	initDB();
	WSAData wsaData;

	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		logMessage("Time Server: Error at WSAStartup()\n", ERR);
		return;
	}

	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (INVALID_SOCKET == listenSocket)
	{
		logMessage("Time Server: Error at socket()", ERR);
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
		logMessage("Time Server: Error at bind(): " ,ERR);
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		logMessage("Time Server: Error at listen(): " , ERR);
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
			logMessage("Time Server: Error at select(): ", ERR);
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
	for (int i = 0; i < MAX_SOCKETS; i++) {
		time_t currentTime = time(NULL);
		if (sockets[i].lastActivity > 0 && currentTime - sockets[i].lastActivity > CONNECTION_TIMEOUT) {
			logMessage("Client is IDLE for too long.. Closing connection", INFO);
			closesocket(sockets[i].id);
			removeSocket(i);
		}
	}

	logMessage("Time Server: Closing Connection.\n", INFO);
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
			sockets[index].len -= bytesRecv;
			sockets[index].send = SEND;
			sockets[index].lastActivity = time(NULL);
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


int getRequestedMethod(char* req) {
	int index = getIndexOfChar(req, SPACE);
	char method[DEFUALT_BUFFER_SIZE];
	strncpy(method, req, index);
	method[index] = STRING_TERMINATOR;
	if (!strcmp(method, "GET")) {
		return GET;
	}
	if (!strcmp(method, "POST")) {
		return POST;
	}
	if (!strcmp(method, "PUT")) {
		return PUT;
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

void handleGetRequest(int index) {
	const char* statusCode = calculateStatusCode(index);
	char res[DEFUALT_BUFFER_SIZE];
	strcpy(res, handlePageRequest(index).c_str());
	formatGetResponse(index, res ,statusCode);
}

void handleHeadRequest(int index) {
	const char* statusCode = calculateStatusCode(index);
	char res[DEFUALT_BUFFER_SIZE];
	strcpy(res, handlePageRequest(index).c_str());
	formatHeadResponse(index, res, statusCode);
}

void handlePostRequest(int index) {
	cout << getMessageBody(sockets[index].buffer, getRequestBodyLength(index)) << NEW_LINE;
	formatPostResponse(index, EMPTY_BODY, OK);
}

void handlePutRequest(int index) {
	Page pageToEdit(getRequsetedRoute(sockets[index].buffer));
	Page newPage(getRequsetedRoute(sockets[index].buffer), getMessageBody(sockets[index].buffer, getRequestBodyLength(index)));
	const char* statusCode;
	if (isPageExistInDB(pageToEdit) != NOT_EXIST) {
		editPageOnDB(pageToEdit, newPage);
		statusCode = NO_CONTENT;
	} else {
		addPageToDB(newPage);
		statusCode = CREATED;
	}
	formatPutResponse(index, EMPTY_BODY, statusCode);
}

void handleDeleteRequest(int index) {
	Page pageToRemove (getRequsetedRoute(sockets[index].buffer));
	if (removePageFromDB(pageToRemove)) {
		formatDeleteResponse(index, EMPTY_BODY, NO_CONTENT);
	} else {
		formatDeleteResponse(index, EMPTY_BODY, NOT_FOUND);
	}
}

void handleTraceRequest(int index) {
	formatTraceResponse(index, sockets[index].buffer, OK);
}

void handleOptionsRequest(int index) {
	formatOptionsResponse(index, sockets[index].buffer, NO_CONTENT);
}

string handlePageRequest(int index) {
	if (checkIfParamExist(sockets[index].buffer)) {
		char lang[SMALL_BUFFER_SIZE];
		strcpy(lang, getRequestParamValue(sockets[index].buffer));
		return getPageByNameAndLang(getRequsetedRoute(sockets[index].buffer), lang);
	}
	return getPageByName(getRequsetedRoute(sockets[index].buffer));
}

void handleUnkownRequest(int index) {
	logMessage("Got unkown request \n", WARN);
	formatUnkownRequestResponse(index, EMPTY_BODY ,NOT_IMPLEMENTED);
}

void formatResponse(int index, const char* res, const char* statusCode, options request) {
	formatStatusLine(index, statusCode);
	addHeaders(index, res, request);
	strcat(sockets[index].sendBuff, "\r\n");
}


void formatGetResponse(int index, const char* res, const char* statusCode) {
	formatResponse(index, res, statusCode, GET);
	strcat(sockets[index].sendBuff, res);
}

void formatPostResponse(int index, const char* res, const char* statusCode) {
	formatResponse(index, res, statusCode, POST);
}

void formatHeadResponse(int index, const char* res, const char* statusCode) {
	formatResponse(index, res, statusCode, HEAD);
}

void formatTraceResponse(int index, char* res, const char* statusCode) {
	formatResponse(index, res, statusCode, TRACE);
	strcat(sockets[index].sendBuff, res);
}

void formatOptionsResponse(int index, char* res, const char* statusCode) {
	formatResponse(index, res, statusCode, OPTIONS);
	strcat(sockets[index].sendBuff, res);
}

void formatDeleteResponse(int index, const char* res, const char* statusCode) {
	formatResponse(index, res, statusCode, DELET);
}

void formatPutResponse(int index, const char* res, const char* statusCode) {
	formatResponse(index, res, statusCode, PUT);
}

void formatUnkownRequestResponse(int index, const char* res, const char* statusCode) {
	formatResponse(index, res, statusCode, EXIT);
}

void formatStatusLine(int index, const char* statusCode) {
	strcpy(sockets[index].sendBuff, PROTOCOL_VERSION);
	strcat(sockets[index].sendBuff, statusCode);
}

void addHeaders(int index, const char* res, options request) {
	addServerHeader(index);
	addContetntTypeHeader(index, request);
	addDateHeader(index);
	addLengthHeader(index, calculateBodyLength(res, request));
	if (request == OPTIONS) {
		addAllowHeader(index);
	}
}

void addAllowHeader(int index) {
	strcat(sockets[index].sendBuff, "Allow: OPTIONS, GET, POST, PUT, DELETE, HEAD, TRACE \r\n");
}

void addServerHeader(int index) {
	strcat(sockets[index].sendBuff, "Server: Ran Eldan \r\n");
}

void addContetntTypeHeader(int index, options request) {
	if (request == TRACE) {
		strcat(sockets[index].sendBuff, "Content-Type: message/http \r\n");
	} else {
		strcat(sockets[index].sendBuff, "Content-Type: text/plain \r\n");
	}
}

void addDateHeader(int index) {
	time_t timer;
	time(&timer);
	strcat(sockets[index].sendBuff, "Date: ");
	strcat(sockets[index].sendBuff, ctime(&timer));
	sockets[index].sendBuff[strlen(sockets[index].sendBuff) - 1] = STRING_TERMINATOR;
	strcat(sockets[index].sendBuff, "\r\n");
}

void addLengthHeader(int index, int length) {
	strcat(sockets[index].sendBuff, "Content-Length: ");
	char lenStr[10];
	sprintf(lenStr, "%d", length);
	strcat(sockets[index].sendBuff, lenStr);
	strcat(sockets[index].sendBuff, "\r\n");
}

const char* calculateStatusCode(int index) {
	return isPageExistInDB(Page(getRequsetedRoute(sockets[index].buffer))) != NOT_EXIST
		|| !strcmp(getRequsetedRoute(sockets[index].buffer), DEFUALT_ROUTE) ? OK : NOT_FOUND;
}

int getRequestBodyLength(int index) {
	return atoi(getHeaderValue(sockets[index].buffer, "Content-Length"));
}

int calculateBodyLength(const char* res, options method) {
	return method == GET || method == HEAD || method == TRACE? 
		strlen(res) : 0;
}

char* getRequsetedRoute(char* req) {
	char route[DEFUALT_BUFFER_SIZE];
	int startOfRoute = getIndexOfChar(req, SLASH);
	req += startOfRoute + ROUTE_OFFSET;
	int endOfRoute = checkIfParamExist(req) ? getIndexOfChar(req, QMARK) : getIndexOfChar(req, SPACE);
	strncpy(route, req, endOfRoute);
	route[endOfRoute] = STRING_TERMINATOR;
	return route;
}

char* getRequestParamValue(char* req) {
	char paramValue[DEFUALT_BUFFER_SIZE];
	int startIndex = getIndexOfChar(req, EQUAL) + PARAM_OFFSET;
	int endIndex = startIndex + getIndexOfChar(req, SPACE) - PARAM_OFFSET;
	int paramValueLength = endIndex - startIndex;
	strncpy(paramValue, req + startIndex, paramValueLength);
	paramValue[paramValueLength] = STRING_TERMINATOR;
	return paramValue;
}

bool checkIfParamExist(char* req) {
	return !(getIndexOfChar(req, QMARK) == strlen(req));
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


void initDB() {
	pagesDB.push_back({ "home", "Welcome to the best server in the world!", "en" });
	pagesDB.push_back({ "home", "ברוכים הבאים לשרת הכי טוב בעולם!", "he" });
	pagesDB.push_back({ "index", "<html>Ran Eldan</html>", "en" });
	pagesDB.push_back({ "contact", "Ran Eldan - raneeldan@gmail.com", "en" });
	cout << getAllPages();
}

bool addPageToDB(Page page) {
	int existingPage = isPageExistInDB(page);
	if (existingPage != NOT_EXIST && !page.lang.compare(pagesDB.at(existingPage).lang)) {
		return false;
	}
	pagesDB.push_back(page);
	return true;
}

bool removePageFromDB(Page page) {
	int positionToDelete = isPageExistInDB(page);
	if (positionToDelete != NOT_EXIST) {
		pagesDB.erase(pagesDB.begin() + positionToDelete);
		return true;
	}
	logMessage("Unable to complete remove opertion - page not found", WARN);
	return false;
}

int isPageExistInDB(Page page) {
	int pos = 0;
	for (auto it = begin(pagesDB); it != end(pagesDB); ++it, ++pos) {
		if (!it->name.compare(page.name)) {
			return pos;
		}
	}
	return NOT_EXIST;
}

int isPageExistInDBByLang(Page page) {
	int pos = 0;
	for (auto it = begin(pagesDB); it != end(pagesDB); ++it, ++pos) {
		if (!it->name.compare(page.name) && !it->lang.compare(page.lang)) {
			return pos;
		}
	}
	return NOT_EXIST;
}

bool editPageOnDB(Page oldPage, Page newPage) {
	int indexToEdit = isPageExistInDB(oldPage);
	if (indexToEdit != NOT_EXIST) {
		pagesDB.at(indexToEdit) = newPage;
		return true;
	}
	return false;
}

string getAllPages() {
	string result = "";
	for (auto it = begin(pagesDB); it != end(pagesDB); ++it) {
		result += formatPage(it) + "\n";
	}
	return result;
}

string getAllPagesInLang(char* lang) {
	string result = "";
	string langStr(lang);
	for (auto it = begin(pagesDB); it != end(pagesDB); ++it) {
		if (!it->lang.compare(langStr)) {
			result += formatPage(it) + "\n";
		}
	}
	return result;
}

string getPageByName(const char* pageName) {
	string name(pageName);
	Page pageToFind(name);
	int pageIndex = isPageExistInDB(pageToFind);
	if (!name.compare(DEFUALT_ROUTE)) {
		return(getAllPages());
	}
	if (pageIndex != NOT_EXIST) {
		return (formatPage(pagesDB.begin() + pageIndex));
	}
	return "";
}

string getPageByNameAndLang(const char* pageName, char* lang) {
	string name(pageName);
	Page pageToFind(name, EMPTY_BODY ,lang);
	int pageIndex = isPageExistInDBByLang(pageToFind);
	if (!name.compare(DEFUALT_ROUTE)) {
		return(getAllPagesInLang(lang));
	}
	if (pageIndex != NOT_EXIST) {
		return (formatPage(pagesDB.begin() + pageIndex));
	}
	return "";
}

string formatPage(vector<Page>::iterator page) {
	return page->name + ": " + "(lang: " + page->lang + ")\n" + page->content + "\n";
}


void logMessage(const char* message, const char* level) {
	cout << "Server " << level << ':' << NEW_LINE << message << NEW_LINE;
}

int getIndexOfChar(char* req, char charToSearch) {
	if (strchr(req, charToSearch) != NULL) {
		return (int)(strchr(req, charToSearch) - req);
	}
	return strlen(req);
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
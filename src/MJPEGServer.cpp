#include "MJPEGServer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/tcp.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

#define MAX_CLIENT 32


MjpegServer *MjpegServer::m_pInstance =NULL;

MjpegImageData::MjpegImageData()
{
	m_pImageData = NULL;
	m_nImageDataLen = 0;
}

MjpegImageData::~MjpegImageData()
{
	if (m_pImageData != NULL) {
		free(m_pImageData);
		m_pImageData = NULL;
	}
}

int MjpegImageData::SetImageData(char *ptr, int nLen)
{
	if (ptr == NULL)
		return -1;

	m_pImageData = (char *)malloc(nLen);
	if (m_pImageData == NULL)  {
		return -2;
	}
	memcpy(m_pImageData, ptr, nLen);
	m_nImageDataLen = nLen;
	return 0;
}


MjpegClient::MjpegClient(){

}

MjpegClient::~MjpegClient(){

}

int MjpegClient::Parse(char *buf){
	char *ptr, *saveptr0, *saveptr1;
	std::string strKey, strValue;
	char *ptr_key, *ptr_value;
	char szBufMethod[64], szBufUrlPath[64], szBufHttpVer[64];

	//ptr = strtok_r(buf, "\n", &saveptr0);
	if (strstr(buf, "GET")==NULL) {
		printf("only support Get\n");
		return -1;
	}

	sscanf(buf, "%s %s %s", szBufMethod, szBufUrlPath, szBufHttpVer);
	m_strMethod = szBufMethod;
	m_strUrlPath = szBufUrlPath;
	m_strHttpVer = szBufHttpVer;
	printf("m_strMethod %s m_strUrlPath %s m_strHttpVer %s \n",szBufMethod, szBufUrlPath, szBufHttpVer);
	if (m_strUrlPath == "/video.cgi")  {
		m_bParse = true;
		m_nClientType = MJPEG_CLIENT_TYPE_VIDEO;
	}
	else if (m_strUrlPath == "/jpeg.cgi" || m_strUrlPath == "/picture.cgi" 
		|| m_strUrlPath == "/picture.jpg") {
		m_bParse = true;
		m_nClientType = MJPEG_CLIENT_TYPE_NORMAL;
	}
	else {
		m_nClientType = MJPEG_CLIENT_TYPE_UNKNOW;
		printf("not support url path:%s\n", m_strUrlPath.c_str());
		return -2;
	}
	return 0;
}


MjpegServer::MjpegServer(){
	nClientNum = 0;
	pthread_mutex_init(&m_mutexClient, NULL);
	pthread_mutex_init(&m_mutexImageData, NULL);
}

MjpegServer::~MjpegServer(){

}

MjpegServer *MjpegServer::GetInstance(){
	if(m_pInstance == NULL){
		m_pInstance = new MjpegServer();
	}
	return m_pInstance;
}
void MjpegServer::SetStatus(bool mRun){
	m_bRun = mRun;
}

void* MjpegServer::http_thread(void *data){
	MjpegServer::GetInstance()->OnServerConnect();
	return NULL;
}

void* MjpegServer::client_thread(void *data){
	MjpegServer::GetInstance()->OnClientSend();
}

void MjpegServer::AddCameraImg(MjpegImageData *pImage){
	if(pImage == NULL)
		return;
	pthread_mutex_lock(&m_mutexImageData);
	if (m_mapClient.size()!=0) 
		m_queueImageData.push(pImage);
	else 
		delete pImage;
	pthread_mutex_unlock(&m_mutexImageData);
}

void MjpegServer::OnClientSend(){
	MjpegImageData *pMjpegImageData = NULL;
	int nTimeStart;
	m_bRun = true;
	while(m_bRun) {

		if (m_queueImageData.size()==0) {
			// 
			//printf("Not Image Data in queue\n");
			continue;
		}
		// printf("m_queueImageData size:%d client_num:%d\n", m_queueImageData.size(), m_mapClient.size());
		pthread_mutex_lock(&m_mutexImageData);
		pMjpegImageData = m_queueImageData.front();
		if (pMjpegImageData==NULL) {
			pthread_mutex_unlock(&m_mutexImageData);
			printf("Not data to send\n");
			continue;
		}
		m_queueImageData.pop();
		pthread_mutex_unlock(&m_mutexImageData);
		pthread_mutex_lock(&m_mutexClient);
		// send client data
		std::map<int, MjpegClient *>::iterator it;
		for (it=m_mapClient.begin(); it!=m_mapClient.end(); it++) {
			//SendData(it->second, pMjpegImageData);
			printf("Send video data fd %d\n", it->first);
			SendVideo(it->second, pMjpegImageData);
		}
		pthread_mutex_unlock(&m_mutexClient);
		if (pMjpegImageData) {
			delete pMjpegImageData;
		}

	}
}

void MjpegServer::OnServerConnect(){
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	socklen_t client_addr_size;
	struct epoll_event srv_ev, cli_ev, events_array[MAX_CLIENT];
	struct timeval tv;
	int epollfd, fd, srv_sock, client_sock, event_num;
	unsigned int ret;
	nClientNum = 0;
	char szBuf[254];
	sprintf(szBuf, "<label>Hello MJPEGServer</label>\n");
	//Init epoll fd
	epollfd = epoll_create(MAX_CLIENT);
	if (epollfd<0) {
		printf("epoll_create fail\n");
		return ;
	}
	m_nEpollFD = epollfd;
	srv_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (srv_sock < 0) {
		printf("server socket error\n");
		return;
	}
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET; //IPV4
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(MJPEG_SERVER_HTTP_PORT);
	memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));
	// 重用
	int reuse_addr = 1;
	setsockopt(srv_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse_addr, sizeof(reuse_addr));
	//bind addr to socket
	int res = bind(srv_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (res < 0) {
		printf("bind error\n");
		return;
	}
	listen(srv_sock, MAX_CLIENT);
	srv_ev.events = EPOLLIN;
	srv_ev.data.fd = srv_sock;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, srv_sock, &srv_ev);//Register IO Event
	m_bRun = true;
	while(m_bRun){
		event_num = epoll_wait(epollfd, events_array, MAX_CLIENT, 1000);
		if (event_num>0)
			printf("Get socket event_num:%d\n", event_num);
		if (event_num==0) {
			// Check Timeout Client, Remove Here
			printf("Get socket event_num time out\n");
			continue;
		}
		else if (event_num<0) {
			printf("epoll_wait error\n");
			continue;
		}
		for(int i = 0; i < event_num; i++){
			ret = events_array[i].events;
			if (ret & EPOLLIN)
				printf("EVENT EPOLLIN\n");
			if (ret & EPOLLHUP)
				printf("EVENT EPOLLHUP\n");
			if (ret & EPOLLERR)
				printf("EVENT EPOLLERR\n");
			if (ret == 0) 
				continue;

			fd = events_array[i].data.fd;
			//Server Sockets Event
			if (fd == srv_sock) {
				printf("New Connect..\n");
				// New Connect
				client_addr_size = sizeof(struct sockaddr_in);
				client_sock = accept(srv_sock, (struct sockaddr*)&client_addr, &client_addr_size);
				printf("client_sock:%d IP:%s port:%d\n", client_sock, inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
				if (client_sock>0) {
					MjpegClient *pMjpegClient = new MjpegClient();
					pMjpegClient->SetSock(client_sock);
					pMjpegClient->SetClientID(nClientNum++);
					//设置超时
					tv.tv_sec = 1;
					tv.tv_usec = 0;
					setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
					cli_ev.data.fd = client_sock;
					cli_ev.data.ptr = pMjpegClient;
					cli_ev.events = EPOLLIN | EPOLLET |EPOLLERR | EPOLLHUP | EPOLLPRI;
					epoll_ctl(epollfd, EPOLL_CTL_ADD, client_sock, &cli_ev);
					pthread_mutex_lock(&m_mutexClient);
					AddClientData(pMjpegClient->GetClientID(), pMjpegClient);
					pthread_mutex_unlock(&m_mutexClient);
					//SendData(client_sock, szBuf, strlen(szBuf));
				}
			}else{
				MjpegClient *pMjpegClient = (MjpegClient*)events_array[i].data.ptr;
				fd = pMjpegClient->GetSock();
				printf("Client ID %d socket ID %d..\n",
					pMjpegClient->GetClientID(), pMjpegClient->GetSock());
				if (ret & EPOLLERR) {
					printf("unknow ... EPOLLERR..\n");
					pthread_mutex_lock(&m_mutexClient);
					RemoveClientData(pMjpegClient->GetClientID());
					pthread_mutex_unlock(&m_mutexClient);
				}
				else if (ret & EPOLLHUP) {
					printf("unknow hung up ...\n");
					pthread_mutex_lock(&m_mutexClient);
					RemoveClientData(pMjpegClient->GetClientID());
					pthread_mutex_unlock(&m_mutexClient);
				}
				else if (ret & EPOLLIN){
					printf("client event sock %d...\n", fd);
					char recvbuf[254];
					int revret = read(fd, recvbuf, sizeof(recvbuf));
					if (revret>0){
						if(pMjpegClient->Parse(recvbuf) != 0){
							memset(szBuf, 0x0, sizeof(szBuf));
							sprintf(szBuf, "HTTP/1.1 404 Not Found\r\n\r\n");
							send(pMjpegClient->GetSock(), szBuf, strlen(szBuf), 0);
							pthread_mutex_lock(&m_mutexClient);
							RemoveClientData(pMjpegClient->GetClientID());	
							pthread_mutex_unlock(&m_mutexClient);					
						}
					}
				}
			}
		}
	}

}

void MjpegServer::RemoveClientData(int nID)
{
	MjpegClient *pMjpegClientData;
	struct epoll_event ev;

	std::map<int, MjpegClient *>::iterator it;

	it = m_mapClient.find(nID);
	if (it == m_mapClient.end()) {
		// not found
	}else {
		pMjpegClientData = it->second;
		m_mapClient.erase(it);
		ev.events = EPOLLIN | EPOLLET |EPOLLERR | EPOLLHUP | EPOLLPRI;
		epoll_ctl(m_nEpollFD, EPOLL_CTL_DEL, pMjpegClientData->GetSock(), &ev);
		shutdown(pMjpegClientData->GetSock(), SHUT_WR);
		close(pMjpegClientData->GetSock());
		delete pMjpegClientData;
	}
}

void MjpegServer::AddClientData(int nID, MjpegClient *pMjpegClientData)
{
	m_mapClient[nID] = pMjpegClientData;
}

void MjpegServer::SendData(int fd, char *buf, int len)
{
	// static const char szHttpJpegImageHeader[] = "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nAccept-Ranges: bytes\r\nContent-Length: %d\r\nServer: Easy Jpeg Server\r\n\r\n";
	static const char szHttpJpegImageHeader[] = 
	"HTTP/1.0 200 OK\r\n"
	"Access-Control-Allow-Origin: *\r\n"
	"Connection: close\r\n"
	"Server: Easy Jpeg Server\r\n"
	"Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n"
	"Pragma: no-cache\r\n"
	"Content-Type: text/html\r\n"
	"Content-Length: %d\r\n\r\n";
	char szBuf[256] = {0};
	int ret;

	printf("==%s %d\n", __FILE__, __LINE__);

	sprintf(szBuf, szHttpJpegImageHeader, len);
	ret = send(fd, szBuf, strlen(szBuf), 0);
	if (ret<0)
		return;
	send(fd, buf, len , 0);
}

void MjpegServer::SendVideo(MjpegClient *pMjpegClientData, MjpegImageData *pMjpegImageData)
{
	if (!pMjpegClientData->GetParse()) {
		printf("ignore:: invalid url\n");
		return;
	}
	// static const char szHttpVideoImageHeader[] = ("HTTP/1.1 200 OK\r\nCache-Control: no-cache\r\nContent-Type: multipart/x-mixed-replace;boundary=boundarydonotcross\r\nAccept-Ranges: bytes\r\nServer: Easy Mjpeg Server\r\n\r\n");
	static const char szHttpVideoImageHeader[] = 
	"HTTP/1.0 200 OK\r\n"
	"Access-Control-Allow-Origin: *\r\n"
	"Connection: close\r\n"
	"Server: Easy Jpeg Server\r\n"
	"Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n"
	"Pragma: no-cache\r\n"
	"Content-Type: multipart/x-mixed-replace;boundary=boundarydonotcross\r\n\r\n";
	static const char szHttpBoundaryHeader[] = 
	"--boundarydonotcross\r\n"
	"Content-Type:image/jpg\r\n"
	"Content-Length:%d\r\n\r\n";
	char szBuf[2048] = {0};
	int ret = 0;

	if (!pMjpegClientData->GetSendHeader())  {
		sprintf(szBuf, szHttpVideoImageHeader);
		ret = send(pMjpegClientData->GetSock(), szBuf, strlen(szBuf), 0);
		pMjpegClientData->SetSendHeader(true);
	}

	memset(szBuf, 0x0, sizeof(szBuf));
	sprintf(szBuf, szHttpBoundaryHeader, pMjpegImageData->GetLength());
	ret = send(pMjpegClientData->GetSock(), szBuf, strlen(szBuf), 0);
	if (ret<0) {
		pMjpegClientData->SetParase(false);
		// RemoveClientData(pMjpegClientData->GetClientNum());
		return;
	}
	printf("send client sock %d data len %d\n", pMjpegClientData->GetSock(),  pMjpegImageData->GetLength());
	ret = send(pMjpegClientData->GetSock(), pMjpegImageData->GetPtr(), pMjpegImageData->GetLength(), 0);
	if (ret<0) {
		printf("Send failed\n");
		pMjpegClientData->SetParase(false);
		// RemoveClientData(pMjpegClientData->GetClientNum());
		return;
	}
	sprintf(szBuf, "\r\n");
	send(pMjpegClientData->GetSock(), szBuf, 2, 0);
}


void MjpegServer::StartServer(){
	printf("MjpegServer start\n");
	pthread_create(&m_tidHttpServer, NULL, http_thread, NULL);
	pthread_create(&m_tidClient, NULL, client_thread, NULL);
}

void MjpegServer::StopServer()
{
	m_bRun = false;
	pthread_join(m_tidHttpServer, NULL);
}

#ifndef _MJPEG_SERVER_H_
#define _MJPEG_SERVER_H_
#define MJPEG_SERVER_HTTP_PORT 8080
#include <pthread.h>
#include <string>
#include <map>
#include <queue>
using namespace std;
#define MJPEG_CLIENT_TYPE_NORMAL 0
#define MJPEG_CLIENT_TYPE_VIDEO 1
#define MJPEG_CLIENT_TYPE_UNKNOW -3
class MjpegImageData {
public:
	MjpegImageData();
	~MjpegImageData();

	int SetImageData(char *ptr, int nLen);

	char *GetPtr() { return m_pImageData; }
	int GetLength() { return m_nImageDataLen; }

public:
	char *m_pImageData;
	int m_nImageDataLen;
};

class MjpegClient{
public:
	MjpegClient();
	~MjpegClient();

	void SetSock(int fd) {m_nClientFD = fd;}
	int GetSock() {return m_nClientFD;}
	int Parse(char *buf);
	bool GetParse() { return m_bParse; }
	void SetParase(bool bParse) { m_bParse = bParse; }
	std::string GetHttpUrlPath() {return m_strUrlPath;}
	int GetClientType() { return m_nClientType; }
	bool GetSendHeader() { return m_bSendHeader; }
	void SetSendHeader(bool bValue) { m_bSendHeader = bValue; }
	void SetClientID(int nNum) { m_nClientNum = nNum; }
	int GetClientID() { return m_nClientNum; }
	int AddTimeouTick() { return m_nTimeoutTick++;}
private:
	std::map<std::string, std::string> m_mapHeader;// http header map
	std::string m_strMethod, m_strUrlPath, m_strHttpVer;
	int m_nClientFD, m_nClientType, m_nClientNum;
	bool m_bSendHeader;
	bool m_bParse;
	int m_nTimeoutTick;
};

class MjpegServer{
public:
	MjpegServer();
	~MjpegServer();
	void StartServer();
	void StopServer();
	void SetStatus(bool bRun);
	void OnServerConnect();
	void OnClientSend();
	void AddClientData(int nID, MjpegClient *pMjpegClient);
	void RemoveClientData(int nID);
	void SendData(int fd, char *buf, int len);
	void SendVideo(MjpegClient *pMjpegClientData, MjpegImageData *pMjpegImageData);
	void AddCameraImg(MjpegImageData *pImage);
	static void *http_thread(void *data);
	static void *client_thread(void *data);
	static MjpegServer *GetInstance();
private:
	static MjpegServer *m_pInstance;
	pthread_t m_tidHttpServer, m_tidClient;
	pthread_mutex_t m_mutexClient, m_mutexImageData;
	std::map<int, MjpegClient *> m_mapClient;
	std::queue<MjpegImageData *> m_queueImageData;
	int m_nEpollFD, nClientNum;
	bool m_bRun;
};

#endif
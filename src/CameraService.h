#ifndef __CAMERASERVICE_H__
#define __CAMERASERVICE_H__
#include <stdint.h>
#include <unistd.h>
#include <pthread.h> 
typedef void (*CAMERA_SERVICE_DATA_CB_FUNC)(void *data);
#define CAMERA_DEV "/dev/video0"
typedef struct {  
  uint8_t* start;  
  size_t length;  
} buffer_t;  
  
typedef struct {  
  int fd;  
  uint32_t width;  
  uint32_t height;  
  size_t buffer_count;  
  buffer_t* buffers;  
  buffer_t head;  
} camera_t;  

class CameraService{
public: 
	CameraService();
	~CameraService();
	int CameraInit(camera_t *camera);
	void StartService();
	void StopCamera();
	int SetCamera(int nWidth, int nHeight);
	static CameraService *GetInstance();
private:
	CAMERA_SERVICE_DATA_CB_FUNC m_pJpegEncodecb;
	static void *start_camera(void *arg);
	static CameraService *m_instance;
	camera_t m_camera;
	pthread_t m_tid;
	static bool bRunFlag;
};
#endif
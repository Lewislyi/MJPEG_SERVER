#include <stdint.h>  
#include <stdlib.h>  
#include <stdio.h>  
#include <string.h>  
#include <errno.h>  
#include <fcntl.h>  
#include <sys/ioctl.h>  
#include <sys/mman.h>  
#include <asm/types.h>    
#include <sys/time.h>  
#include <sys/types.h>  
#include <unistd.h>
#include <linux/videodev2.h> 
#include <jpeglib.h>
#include "CameraService.h"
#include "MJPEGServer.h"


static void jpeg_encode(uint8_t* rgb, uint8_t *outbuffer, long unsigned int out_size, uint32_t width, uint32_t height, int quality)  
{  
  JSAMPARRAY image;  
  image = (JSAMPARRAY)calloc(height, sizeof (JSAMPROW));  
  for (size_t i = 0; i < height; i++) {  
    image[i] = (JSAMPROW)calloc(width * 3, sizeof (JSAMPLE));  
    for (size_t j = 0; j < width; j++) {  
      image[i][j * 3 + 0] = rgb[(i * width + j) * 3 + 0];  
      image[i][j * 3 + 1] = rgb[(i * width + j) * 3 + 1];  
      image[i][j * 3 + 2] = rgb[(i * width + j) * 3 + 2];  
    }  
  }  
  
  struct jpeg_compress_struct compress;  
  struct jpeg_error_mgr error;  
  compress.err = jpeg_std_error(&error);  
  jpeg_create_compress(&compress);  
  //jpeg_stdio_dest(&compress, dest); 
  //jpeg_mem_dest JPP((j_compress_ptr cinfo,unsigned char ** outbuffer,unsigned long * outsize));
  jpeg_mem_dest(&compress, &outbuffer, &out_size);
  compress.image_width = width;  
  compress.image_height = height;  
  compress.input_components = 3;  
  compress.in_color_space = JCS_RGB;  
  jpeg_set_defaults(&compress);  
  jpeg_set_quality(&compress, quality, TRUE);  
  jpeg_start_compress(&compress, TRUE);  
  jpeg_write_scanlines(&compress, image, height);  
  jpeg_finish_compress(&compress);  
  jpeg_destroy_compress(&compress);  
  
  for (size_t i = 0; i < height; i++) {  
    free(image[i]);  
  }  
  free(image);  
}  
  
  
int minmax(int min, int v, int max)  
{  
  return (v < min) ? min : (max < v) ? max : v;  
}  
  
uint8_t* yuyv2rgb(uint8_t* yuyv, uint32_t width, uint32_t height)  
{  
  uint8_t* rgb = (uint8_t*)calloc(width * height * 3, sizeof (uint8_t));  
  for (size_t i = 0; i < height; i++) {  
    for (size_t j = 0; j < width; j += 2) {  
      size_t index = i * width + j;  
      int y0 = yuyv[index * 2 + 0] << 8;  
      int u = yuyv[index * 2 + 1] - 128;  
      int y1 = yuyv[index * 2 + 2] << 8;  
      int v = yuyv[index * 2 + 3] - 128;  
      rgb[index * 3 + 0] = minmax(0, (y0 + 359 * v) >> 8, 255);  
      rgb[index * 3 + 1] = minmax(0, (y0 + 88 * v - 183 * u) >> 8, 255);  
      rgb[index * 3 + 2] = minmax(0, (y0 + 454 * u) >> 8, 255);  
      rgb[index * 3 + 3] = minmax(0, (y1 + 359 * v) >> 8, 255);  
      rgb[index * 3 + 4] = minmax(0, (y1 + 88 * v - 183 * u) >> 8, 255);  
      rgb[index * 3 + 5] = minmax(0, (y1 + 454 * u) >> 8, 255);  
    }  
  }  
  return rgb;  
}  


CameraService *CameraService::m_instance = NULL;
bool CameraService::bRunFlag = true;
CameraService::CameraService()
{

}

CameraService::~CameraService()
{

}


CameraService* CameraService::GetInstance()
{
	if(m_instance == NULL)
		m_instance = new CameraService();
	return m_instance;
}

void *CameraService::start_camera(void *arg)
{
	camera_t* camera = (camera_t*) arg;
  if(camera == NULL)
		return NULL;
	int ret;
	fd_set fds;  
  	struct v4l2_buffer buf;
  	struct timeval timeout;  
	timeout.tv_sec = 1;  
	timeout.tv_usec = 0;
	int count = 0;     
  	while(bRunFlag){
  		FD_ZERO(&fds);  
  		FD_SET(camera->fd, &fds);  
  		ret = select(camera->fd + 1, &fds, 0, 0, &timeout);  
  		if(ret < 0){
  			usleep(100*100);
  			continue;
  		}
  		if(FD_ISSET(camera->fd, &fds)){
	  		memset(&buf, 0, sizeof buf);  
	  		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
	  		buf.memory = V4L2_MEMORY_MMAP;
	  		//DQBUF to user spaces 
	  		//Applications call the VIDIOC_DQBUF ioctl to dequeue a filled (capturing) or 
	  		//displayed (output) buffer from the driver's outgoing queue 
	  		if(ioctl(camera->fd, VIDIOC_DQBUF, &buf) == -1)
	  			break; 
			  printf("read camera buffer index %d, byteused %d, count %d\n", buf.index, buf.bytesused, count);
			  //TODO:: JpegEncoder Call Back  
        //FILE *pfile = fopen("preview.yuv", "w");
#if 1
        unsigned char* rgb = yuyv2rgb(CameraService::GetInstance()->m_camera.buffers[buf.index].start, 
        CameraService::GetInstance()->m_camera.width, CameraService::GetInstance()->m_camera.height);
        //jpeg_encode(rgb, pfile, 0, camera->width, camera->height, 100);
				//fwrite(CameraService::GetInstance()->m_camera.buffers[buf.index].start, buf.bytesused, 1, pfile);
				//fclose(pfile);
        MjpegImageData *pImgData = new MjpegImageData();
        unsigned char buffer[700 * 1000];
        long unsigned int out_size = buf.bytesused;
        jpeg_encode(rgb, buffer, out_size, camera->width, camera->height, 60);
        pImgData->SetImageData((char*)buffer, out_size);
        MjpegServer::GetInstance()->AddCameraImg(pImgData);
			  free(rgb);
#endif
			  //Applications call the VIDIOC_QBUF ioctl to enqueue an empty (capturing) 
	  	  //or filled (output) buffer in the driver's incoming queue.
			 if(ioctl(camera->fd, VIDIOC_QBUF, &buf) == -1)  
			  	break;
			++count;
		}
		sleep(1);
  	}
    return NULL;
}

int CameraService::CameraInit(camera_t *camera)
{
  struct v4l2_capability cap;  
  if (ioctl(camera->fd, VIDIOC_QUERYCAP, &cap) == -1){ //obtain carmera Info
  	perror("failed to get camera cap info");
  	return -1;
  }
  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) //capture video
  	return -1;
  if (!(cap.capabilities & V4L2_CAP_STREAMING)) //capture stream
  	return -1;
#if 0
  struct v4l2_cropcap cropcap;  
  memset(&cropcap, 0, sizeof cropcap);  
  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
  if (xioctl(camera->fd, VIDIOC_CROPCAP, &cropcap) == 0) {  
    struct v4l2_crop crop;  
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    crop.c = cropcap.defrect;  
    if (xioctl(camera->fd, VIDIOC_S_CROP, &crop) == -1) {  
      // cropping not supported  
    }  
  }  
#endif
  //Set Video format
  struct v4l2_format format;  
  memset(&format, 0, sizeof format);  
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
  format.fmt.pix.width = camera->width;  
  format.fmt.pix.height = camera->height;  
  format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  
  //format.fmt.pix.field = V4L2_FIELD_NONE;
  format.fmt.pix.field = V4L2_FIELD_INTERLACED;  
  if (ioctl(camera->fd, VIDIOC_S_FMT, &format) == -1){
  	perror("Fail to Set Video Format");
  	return -1;
  }

  struct v4l2_streamparm streamparm;
  streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  streamparm.parm.capture.timeperframe.numerator = 1;
  streamparm.parm.capture.timeperframe.denominator = 15;
  if(-1 == ioctl(camera->fd, VIDIOC_S_PARM, &streamparm)){
    printf("Fail to Set Video streamparm rate :%d\n",streamparm.parm.capture.timeperframe.denominator);
    return -1;
  }
  printf("Set Video streamparm rate :%d\n",streamparm.parm.capture.timeperframe.denominator);
  /*功能：
   * 请求V4L2驱动分配视频缓冲区（申请V4L2视频驱动分配内存），
   * V4L2是视频设备的驱动层，位于内核空间，所以通过VIDIOC_REQBUFS控制命令字申请的内存位于内核空间，
   * 应用程序不能直接访问，需要通过调用mmap内存映射函数把内核空间内存映射到用户空间后，
   * 应用程序通过访问用户空间地址来访问内核空间。
   */
  struct v4l2_requestbuffers req;  
  memset(&req, 0, sizeof req);  
  req.count = 1; 
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
  req.memory = V4L2_MEMORY_MMAP;  
  if (ioctl(camera->fd, VIDIOC_REQBUFS, &req) == -1){
  	perror("Fail to Request Video Buffer");
  	return -1;
  }  
  camera->buffer_count = req.count;  
  camera->buffers = (buffer_t*)calloc(req.count, sizeof (buffer_t));  
  
  //把VIDIOC_REQBUFS中分配的数据缓存转换成物理地址
  size_t buf_max = 0;  
  for (size_t i = 0; i < camera->buffer_count; i++) {  
    struct v4l2_buffer buf;  
    memset(&buf, 0, sizeof buf);  
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    buf.memory = V4L2_MEMORY_MMAP;  
    buf.index = i; 

 	//Query Buffer info 
    if (ioctl(camera->fd, VIDIOC_QUERYBUF, &buf) == -1){
    	perror("Get Video Buffer error");
    	free(camera->buffers);
    	return -1;
    }  
    if (buf.length > buf_max) 
    	buf_max = buf.length;  
    camera->buffers[i].length = buf.length;  
    camera->buffers[i].start =  
      (uint8_t*)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,  
           camera->fd, buf.m.offset);//map v4l2 driver kernerl space to user space 
    if (camera->buffers[i].start == MAP_FAILED){
    	perror("Get V4L2 Buffer error");
    	free(camera->buffers);
    	return -1;
    } 
  } 
  return 0; 
  //camera->head.start = (uint8_t*)malloc(buf_max); 
}

int CameraService::SetCamera(int nWidth, int nHeight)
{ 
  int fd = open(CAMERA_DEV, O_RDWR | O_NONBLOCK, 0);  
  if (fd == -1)
  	return -1; 
  m_camera.fd = fd;  
  m_camera.width = nWidth;  
  m_camera.height = nHeight;  
  m_camera.buffer_count = 0;  
  m_camera.buffers = NULL;  
  m_camera.head.length = 0;  
  m_camera.head.start = NULL;  
}

void CameraService::StartService()
{
	//SetCamera(640,480);
  SetCamera(320,240);
	CameraInit(&m_camera);
	for (size_t i = 0; i < m_camera.buffer_count; i++){  
	    struct v4l2_buffer buf;  
	    memset(&buf, 0, sizeof buf);  
	    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
	    buf.memory = V4L2_MEMORY_MMAP;  
	    buf.index = i;  
	    if(ioctl(m_camera.fd, VIDIOC_QBUF, &buf) == -1)
	    	return; 
	}
	bRunFlag = true;  
  	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  	if (ioctl(m_camera.fd, VIDIOC_STREAMON, &type) != -1)
      pthread_create(&m_tid, NULL, start_camera, &m_camera);
   		//start_camera(&m_camera);   
}

void CameraService::StopCamera()
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  bRunFlag = false;  
  ioctl(m_camera.fd, VIDIOC_STREAMOFF, &type);
}

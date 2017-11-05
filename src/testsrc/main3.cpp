    /*============================================================================= 
    #     FileName: read_device.c 
    #         Desc: use ffmpeg read a frame data from v4l2  
    #       Author: licaibiao 
    #   LastChange: 2017-03-15  
    =============================================================================*/  
    #include <stdio.h>  
    #include <string.h>  
    #include <stdlib.h>  
    #include <unistd.h> 
    #ifdef __cplusplus  
extern "C"  
    {  
#endif 
    #include <libavformat/avformat.h>  
    #include <libavcodec/avcodec.h>  
    #include <libavdevice/avdevice.h> 
#ifdef __cplusplus  
    };  
#endif   
    char* input_name= "video4linux2";  
    char* file_name = "/dev/video0";  
    char* out_file  = "test.jpeg";  
      
    void captureOneFrame(void){  
        AVFormatContext *fmtCtx = NULL;      
        AVPacket *packet;   
        AVInputFormat *inputFmt;  
        FILE *fp;   
        int ret;  
      
      
        inputFmt = av_find_input_format (input_name);      
         
        if (inputFmt == NULL)    {          
            printf("can not find_input_format\n");          
            return;      
        }      
      
        if (avformat_open_input ( &fmtCtx, file_name, inputFmt, NULL) < 0){  
            printf("can not open_input_file\n");         return;      
        }  
        /* print device information*/  
        av_dump_format(fmtCtx, 0, file_name, 0);  
      
        packet = (AVPacket *)av_malloc(sizeof(AVPacket));      
        av_read_frame(fmtCtx, packet);   
        printf("data length = %d\n",packet->size);     
      
        fp = fopen(out_file, "wb");      
        if (fp < 0)    {          
            printf("open frame data file failed\n");          
            return ;      
        }      
          
        fwrite(packet->data, 1, packet->size, fp);      
      
        fclose(fp);      
        av_free_packet(packet);      
        avformat_close_input(&fmtCtx);  
     }   
      
    int main(void){      
        avcodec_register_all();      
        avdevice_register_all();      
        captureOneFrame();      
        return 0;  
    }  
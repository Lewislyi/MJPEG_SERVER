#include <stdlib.h>  
#include <stdio.h>  
#include <string.h>  
#include <errno.h>  
#include <sys/time.h>  
#include <sys/types.h>  
#include <unistd.h>
#include <string>
#include <iostream>
#include "CameraService.h"
#include "MJPEGServer.h"
using namespace std;
int main(int argc, char* argv[])
{
	CameraService::GetInstance()->StartService();
	MjpegServer::GetInstance()->StartServer();
	string input;
	while(cin >>input){
		if("quit" == input || "exit" == input)
			break;
		sleep(1)	;
	}
	return 0;
}
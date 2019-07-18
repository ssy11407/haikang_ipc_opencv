﻿#include "pch.h"
#include <opencv\cv.h>
#include <opencv\highgui.h>
#include <opencv2\opencv.hpp>
#include <iostream>
#include <time.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <windows.h>
#include "HCNetSDK.h"
#include "PlayM4.h"
#include "plaympeg4.h"



//#include "global.h"
//#include "readCamera.h"

#define USECOLOR 1

using namespace cv;
using namespace std;

//--------------------------------------------
int iPicNum = 0;//Set channel NO.
LONG nPort = -1;
HWND hWnd = NULL;
CRITICAL_SECTION g_cs_frameList;
list<Mat> g_frameList;
LONG lUserID;
NET_DVR_DEVICEINFO_V30 struDeviceInfo;

void yv12toYUV(char *outYuv, char *inYv12, int width, int height, int widthStep)
{
	int col, row;
	unsigned int Y, U, V;
	int tmp;
	int idx;

	//printf("widthStep=%d.\n",widthStep);

	for (row = 0; row < height; row++)
	{
		idx = row * widthStep;
		int rowptr = row * width;

		for (col = 0; col < width; col++)
		{
			//int colhalf=col>>1;
			tmp = (row / 2)*(width / 2) + (col / 2);
			//         if((row==1)&&( col>=1400 &&col<=1600))
			//         { 
			//          printf("col=%d,row=%d,width=%d,tmp=%d.\n",col,row,width,tmp);
			//          printf("row*width+col=%d,width*height+width*height/4+tmp=%d,width*height+tmp=%d.\n",row*width+col,width*height+width*height/4+tmp,width*height+tmp);
			//         } 
			Y = (unsigned int)inYv12[row*width + col];
			U = (unsigned int)inYv12[width*height + width * height / 4 + tmp];
			V = (unsigned int)inYv12[width*height + tmp];
			//         if ((col==200))
			//         { 
			//         printf("col=%d,row=%d,width=%d,tmp=%d.\n",col,row,width,tmp);
			//         printf("width*height+width*height/4+tmp=%d.\n",width*height+width*height/4+tmp);
			//         return ;
			//         }
			if ((idx + col * 3 + 2) > (1200 * widthStep))
			{
				//printf("row * widthStep=%d,idx+col*3+2=%d.\n",1200 * widthStep,idx+col*3+2);
			}
			outYuv[idx + col * 3] = Y;
			outYuv[idx + col * 3 + 1] = U;
			outYuv[idx + col * 3 + 2] = V;
		}
	}
	//printf("col=%d,row=%d.\n",col,row);
}



//解码回调 视频为YUV数据(YV12)，音频为PCM数据
void CALLBACK DecCBFun(long nPort, char * pBuf, long nSize, FRAME_INFO * pFrameInfo, long nReserved1, long nReserved2)
{
	long lFrameType = pFrameInfo->nType;

	if (lFrameType == T_YV12)
	{
#if USECOLOR
		//int start = clock();
		static IplImage* pImgYCrCb = cvCreateImage(cvSize(pFrameInfo->nWidth, pFrameInfo->nHeight), 8, 3);//得到图像的Y分量  
		yv12toYUV(pImgYCrCb->imageData, pBuf, pFrameInfo->nWidth, pFrameInfo->nHeight, pImgYCrCb->widthStep);//得到全部RGB图像
		static IplImage* pImg = cvCreateImage(cvSize(pFrameInfo->nWidth, pFrameInfo->nHeight), 8, 3);
		cvCvtColor(pImgYCrCb, pImg, CV_YCrCb2RGB);
		//int end = clock();
#else
		static IplImage* pImg = cvCreateImage(cvSize(pFrameInfo->nWidth, pFrameInfo->nHeight), 8, 1);
		memcpy(pImg->imageData, pBuf, pFrameInfo->nWidth*pFrameInfo->nHeight);
#endif
		//printf("%d\n",end-start);

	//	Mat frametemp(pImg), frame;
//		IplImage* temp = cvLoadImage("1.jpg", 1);
		//Mat img3 = cv::cvarrToMat(temp);
		Mat frametemp = cv::cvarrToMat(pImg);
		Mat frame;


		//frametemp.copyTo(frame);
		//      cvShowImage("IPCamera",pImg);
		//      cvWaitKey(1);
		EnterCriticalSection(&g_cs_frameList);
		g_frameList.push_back(frametemp);
		LeaveCriticalSection(&g_cs_frameList);

#if USECOLOR
		//      cvReleaseImage(&pImgYCrCb);
		//      cvReleaseImage(&pImg);
#else
		/*cvReleaseImage(&pImg);*/
#endif
		//此时是YV12格式的视频数据，保存在pBuf中，可以fwrite(pBuf,nSize,1,Videofile);
		//fwrite(pBuf,nSize,1,fp);
	}
	/***************
	else if (lFrameType ==T_AUDIO16)
	{
	//此时是音频数据，数据保存在pBuf中，可以fwrite(pBuf,nSize,1,Audiofile);

	}
	else
	{

	}
	*******************/

}


///实时流回调
void CALLBACK fRealDataCallBack(LONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, void *pUser)
{
	DWORD dRet;
	switch (dwDataType)
	{
	case NET_DVR_SYSHEAD:    //系统头
		if (!PlayM4_GetPort(&nPort)) //获取播放库未使用的通道号
		{
			break;
		}
		if (dwBufSize > 0)
		{
			if (!PlayM4_OpenStream(nPort, pBuffer, dwBufSize, 1024 * 1024))
			{
				dRet = PlayM4_GetLastError(nPort);
				break;
			}
			//设置解码回调函数 只解码不显示
			if (!PlayM4_SetDecCallBack(nPort, DecCBFun))
			{
				dRet = PlayM4_GetLastError(nPort);
				break;
			}

			//设置解码回调函数 解码且显示
			//if (!PlayM4_SetDecCallBackEx(nPort,DecCBFun,NULL,NULL))
			//{
			//  dRet=PlayM4_GetLastError(nPort);
			//  break;
			//}

			//打开视频解码
			if (!PlayM4_Play(nPort, hWnd))
			{
				dRet = PlayM4_GetLastError(nPort);
				break;
			}

			//打开音频解码, 需要码流是复合流
			//          if (!PlayM4_PlaySound(nPort))
			//          {
			//              dRet=PlayM4_GetLastError(nPort);
			//              break;
			//          }       
		}
		break;

	case NET_DVR_STREAMDATA:   //码流数据
		if (dwBufSize > 0 && nPort != -1)
		{
			BOOL inData = PlayM4_InputData(nPort, pBuffer, dwBufSize);
			while (!inData)
			{
				Sleep(10);
				inData = PlayM4_InputData(nPort, pBuffer, dwBufSize);
				OutputDebugString(L"PlayM4_InputData failed \n");
			}
		}
		break;
	}
}

void CALLBACK g_ExceptionCallBack(DWORD dwType, LONG lUserID, LONG lHandle, void *pUser)
{
	char tempbuf[256] = { 0 };
	switch (dwType)
	{
	case EXCEPTION_RECONNECT:    //预览时重连
		printf("----------reconnect--------%d\n", (int)time(NULL));
		break;
	default:
		break;
	}
}

bool OpenCamera()
{
	//lUserID = NET_DVR_Login_V30((char*)"10.1.2.111", 8000, (char*)"admin", (char*)"shijue666", &struDeviceInfo);
	lUserID = NET_DVR_Login_V30((char*)"192.168.100.38", 8000, (char*)"admin", (char*)"aaaaaaa.", &struDeviceInfo);
	if (lUserID == 0)
	{
		cout << "Log in success!" << endl;
		return TRUE;
	}
	else
	{
		printf("Login error, %d\n", NET_DVR_GetLastError());
		NET_DVR_Cleanup();
		return FALSE;
	}
}
DWORD WINAPI ReadCamera(LPVOID IpParameter)
{
	//---------------------------------------
	//设置异常消息回调函数
	NET_DVR_SetExceptionCallBack_V30(0, NULL, g_ExceptionCallBack, NULL);


	//cvNamedWindow("IPCamera");
	//---------------------------------------
	//启动预览并设置回调数据流 
	NET_DVR_CLIENTINFO ClientInfo;
	ClientInfo.lChannel = 1;        //Channel number 设备通道号
	ClientInfo.hPlayWnd = NULL;     //窗口为空，设备SDK不解码只取流
	ClientInfo.lLinkMode = 0;       //Main Stream
	ClientInfo.sMultiCastIP = NULL;

	LONG lRealPlayHandle;

	lRealPlayHandle = NET_DVR_RealPlay_V30(lUserID, &ClientInfo, fRealDataCallBack, NULL, TRUE);
	if (lRealPlayHandle < 0)
	{
		printf("NET_DVR_RealPlay_V30 failed! Error number: %d\n", NET_DVR_GetLastError());
		return -1;
	}
	else
		cout << "码流回调成功！" << endl;

	Sleep(-1);

	//fclose(fp);
	//---------------------------------------
	//关闭预览
	if (!NET_DVR_StopRealPlay(lRealPlayHandle))
	{
		printf("NET_DVR_StopRealPlay error! Error number: %d\n", NET_DVR_GetLastError());
		return 0;
	}
	//注销用户
	NET_DVR_Logout(lUserID);
	NET_DVR_Cleanup();

	return 0;
}


int main()
{
	HANDLE hThread;
	//LPDWORD threadID;
	Mat frame1;
	//---------------------------------------
	// 初始化
	NET_DVR_Init();
	//设置连接时间与重连时间
	NET_DVR_SetConnectTime(2000, 3);
	NET_DVR_SetReconnect(10000, true);
	if (OpenCamera())
	{
		InitializeCriticalSection(&g_cs_frameList);
		hThread = ::CreateThread(NULL, 0, ReadCamera, NULL, 0, 0);
		while (1) {
			
			if (0 == g_frameList.size()) {
				Sleep(100);
				continue;
			}
			EnterCriticalSection(&g_cs_frameList);
			if (g_frameList.size())
			{
				list<Mat>::iterator it;
				it = g_frameList.end();
				it--;
				Mat dbgframe = (*(it));
				//imshow("frame from camera",dbgframe);
				//dbgframe.copyTo(frame1);
				//dbgframe.release();
				(*g_frameList.begin()).copyTo(frame1);
				frame1 = dbgframe;
				g_frameList.pop_front();
			}
			g_frameList.clear(); // 丢掉旧的帧
			LeaveCriticalSection(&g_cs_frameList);
		}
		::CloseHandle(hThread);
	}
	else
	{
		cout << "打开相机失败！！";
		Sleep(2000);
	}
	
	return 0;
}
#include "Decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>


const int BUFFSIZE = 1280*720*3/2;

FILE *fp =NULL;
int main(int argc,char** argv)
{
	if(argc < 2)
	{
		printf("error param need input url\n");
		return -1;
	}
	int iwidth = 0;
	int iHeight = 0;
	//const char* filename = "/home/ky/rsm-yyd/DecoderTs/1.ts";
	TSDecoder_Instance* pInstance =  new TSDecoder_Instance();
	TSDecoderParam param;

	param.hight = 720;
	param.width = 1280;

	param.shm_key = 1001;
	if(argv[2])
		param.width = atoi(argv[2]);
	if(argv[3])
		param.hight = atoi(argv[3]);
	printf("input url=%s,w=%d,h=%d\n",argv[1],param.width,param.hight);
	strcpy(param.strURL,argv[1]);
	int ret = pInstance->init_Decoder(param);
	if(ret <= 0)
	{
		printf("--init error return \n");
		return -1;
	}

	int output_video_size = BUFFSIZE;
	unsigned char *output_video_yuv420 = new unsigned char[BUFFSIZE];

	int input_audio_size = 1024*100;
	unsigned char *output_audio_data = new unsigned char[1024*100];
	fp = fopen("test.yuv","wb+");
	{
		if(NULL == fp)
			return -1;
	}

//	FILE *fpaudio = fopen("/home/ky/rsm-yyd/DecoderTs/overlay/audio.pcm","wb+");
//	if(NULL == fpaudio)
//		return -1;
	int m_shm_id;
	char* m_yuv_data;
	void *m_shm_addr;
	unsigned int m_shm_size;

	{
		//先创建yuv输出共享内存
		m_shm_size = sizeof(av_shm_head) + param.width*param.hight*3/2;
	
		m_shm_id = shmget((key_t)param.shm_key,m_shm_size, 0666|IPC_CREAT);
		if(m_shm_id == -1)
		{
			fprintf(stderr, "init decoder shmget failed...\n");
			return -2;
		}
	
		m_shm_addr = shmat(m_shm_id,NULL, 0);
		if(m_shm_addr == (void*)-1)
		{
			fprintf(stderr, "init decoder  shmat failed...\n");
			return -3;
		}
		m_yuv_data = (char*)malloc(param.hight*param.width*2);//缓存
	
	}

	
	unsigned long ulpts = 0;
	unsigned long audio_pts = 0;
	int iloop = 25*20000;
	int x,y,x1,y1,w,h,len;
	len = param.width*param.hight*2;
	while(1)
		{
			usleep(39*1000);

			//static inline int shmhdr_get_data(void *shm_addr,int*x,int*y,int*w,int*h,int*x1,int*y1,char* pyuv,int *pLen)
			int iret = shmhdr_get_data(m_shm_addr,&w,&h,m_yuv_data,&len);
			if(iret > 0)
			{
				//get data 
				printf("get data len %d \n",len);
			//	fwrite(m_yuv_data,1,len,fp);
			}
				
/*			output_video_size = BUFFSIZE;
			int ret = get_Video_data(pInstance,output_video_yuv420,&output_video_size,&iwidth,&iHeight,&ulpts);
			if(--iloop < 0){
				Set_tsDecoder_stat(pInstance,true);
				
			}
			if(ret ==0)
			{
				//do
				//{
					ret = get_Audio_data(pInstance,output_audio_data,&input_audio_size,&audio_pts);
					//fwrite(output_audio_data,1,input_audio_size,fpaudio);
					printf("----audio outputsize=%d,pts =%d,ret=%d\n",input_audio_size,audio_pts,ret);
				//}while(ret ==0);
				
				fprintf(stderr,"video outputsize=%d ,w=%d,h=%d audiopts=%d ,videopts=%d\n",output_video_size,iwidth,iHeight,audio_pts,ulpts);
			//fwrite(output_video_yuv420,1,output_video_size,fp);
				iwidth=0;
				iHeight=0;
			}
*/
		}
	return 0;
	}

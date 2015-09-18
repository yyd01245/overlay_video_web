#include "Decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <getopt.h>


const int BUFFSIZE = 1280*720*3/2;
const char VERSION[128] = "1.0.1";

const int Audio_Buff_size = 4608;

char inputUrl[1024] = {0};
int iwidth = 0;
int iheight = 0;
int audiokey=0;
int videokey = 0;
char audiofifo[1024] = {0};
int idecoder_id = 0;

int iflag = 0;
TSDecoder_Instance* pInstance=NULL;
TSDecoderParam param;

#define PA_LIKELY(x) (__builtin_expect(!!(x),1))
#define PA_UNLIKELY(x) (__builtin_expect(!!(x),0))


#define PA_CLAMP_UNLIKELY(x, low, high)                                 \
    __extension__ ({                                                    \
            typeof(x) _x = (x);                                         \
            typeof(low) _low = (low);                                   \
            typeof(high) _high = (high);                                \
            (PA_UNLIKELY(_x > _high) ? _high : (PA_UNLIKELY(_x < _low) ? _low : _x)); \
        })


short  remix(short buffer1,short buffer2)  
{  
    int value = buffer1 + buffer2;  
	PA_CLAMP_UNLIKELY(value , -0x8000, 0x7FFF);
   // return (short)(value/2);  
   return value;
}


int operation_audio_pcm(unsigned char *pSrcAudio1,int iSrcLen1,unsigned char *pSrcAudio2,int iSrclen2,
			unsigned char *pDstAudio,int *pDstLen)
{
	//srclen1 == srclen2
	//16s 2channel
	
	int iIndex = 0;
	short* pSrc1 = (short*)pSrcAudio1;
	short* pSrc2 = (short*)pSrcAudio2;
	short* pDst = (short*)pDstAudio;
	while(iIndex < iSrcLen1/2)
	{
		short sSrc1 = *(short*)(pSrc1+iIndex);
		short sSrc2 = *(short*)(pSrc2+iIndex);
		*pDst = remix(sSrc1,sSrc2);
		pDst++;
		iIndex++;
	}
	*pDstLen = iSrcLen1;
	return 0;
}
FILE *fp =NULL;

int test_output_new()
{
		int output_video_size = BUFFSIZE;
		unsigned char *output_video_yuv420 = new unsigned char[BUFFSIZE];
	
		int input_audio_size = 1024*100;
		unsigned char *output_audio_data = new unsigned char[1024*100];
		fp = fopen("test.yuv","wb+");
		{
			if(NULL == fp)
				return -1;
		}
	
		int m_shm_id;
		char* m_yuv_data;
		void *m_shm_addr;
		unsigned int m_shm_size;

		int m_audioshm_id;
		void *m_audioshm_addr;
		unsigned int m_audioshm_size;
	
		{
			//先创建yuv输出共享内存
			m_shm_size = sizeof(videosurf) + param.width*param.hight*3/2;
			
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
			//videosurf* hdr = (videosurf*)m_shm_addr;
			//char *shm_yuv = (char*)(hdr->data);

			m_yuv_data = (char*)malloc(param.hight*param.width*2);//缓存
		
		}

			
		{
			//先创建输出共享内存
			m_audioshm_size= sizeof(audiofrag) + Audio_Buff_size;
			
			m_audioshm_id = shmget((key_t)param.audio_key,m_audioshm_size, 0666|IPC_CREAT);
			if(m_audioshm_id == -1)
			{
				fprintf(stderr, "init decoder audio shmget failed...\n");
				return -2;
			}
			
			m_audioshm_addr = shmat(m_audioshm_id,NULL, 0);
			if(m_audioshm_addr == (void*)-1)
			{
				fprintf(stderr, "init decoder audio  shmat failed...\n");
				return -3;
			}

		
		}
		FILE *fpaudio=NULL;
		fpaudio = fopen("test.pcm","wb+");
		{
			if(NULL == fpaudio)
				return -1;
		}
#if 0
		//operation pcm
		FILE* fp_op = NULL;
		fp_op = fopen("op.pcm","wb");
		if(NULL == fp_op)
			return -1;

		int src_audiofd = -1;
		src_audiofd = open("src.pcm",O_RDONLY);
		if(src_audiofd < 0)
			return -1;

		unsigned char *src_audio_buff = new unsigned char[1024*10];
		unsigned char *dst_audio_buff = new unsigned char[1024*10];
		int iDst_len = 0;

#endif		
		unsigned long ulpts = 0;
		unsigned long audio_pts = 0;
		unsigned long last_audio_pts = 0;
		
		int iloop = 25*20000;
		int x,y,x1,y1,w,h,len;
		len = param.width*param.hight*2; 
		int ivideo = 1;
		const int AudioBuffSize = 4608;
		while(pInstance->m_iThreadid != 0)
			{
				usleep(5*1000);
				//get audio
				{
					memset(output_audio_data,0,AudioBuffSize);
					len = AudioBuffSize;
					//static inline int shmhdr_get_data(void *shm_addr,int*x,int*y,int*w,int*h,int*x1,int*y1,char* pyuv,int *pLen)
					int iret = shmaudio_get((audiofrag*)m_audioshm_addr,output_audio_data,&len,&audio_pts);
					if(iret >= 0 && last_audio_pts!=audio_pts)
					{
						//get data 
						
						printf("get audio data len %d audio_pts=%ld last_audio_pts=%ld\n",len,audio_pts,last_audio_pts);
						last_audio_pts = audio_pts;
						fwrite(output_audio_data,1,len,fpaudio);
					}
					//len = iret;
				}
#if 0
				{
					int ireadlen = read(src_audiofd,src_audio_buff,len);
					printf("--ireadlen=%d \n",ireadlen);
					if(ireadlen < len)
					{	
						lseek(src_audiofd,SEEK_SET,0);
						ireadlen = read(src_audiofd,src_audio_buff,len);
					}
					operation_audio_pcm(output_audio_data,len,src_audio_buff,len,dst_audio_buff,&iDst_len);
					printf("operate audio data len %d \n",iDst_len);
					fwrite(dst_audio_buff,1,iDst_len,fp_op);

					
				}
#else
				//get video
				//if(++ivideo >= 2)
				if(0)
				{
					len = BUFFSIZE;
					//static inline int shmhdr_get_data(void *shm_addr,int*x,int*y,int*w,int*h,int*x1,int*y1,char* pyuv,int *pLen)
					int iret = shmvideo_get((videosurf*)m_shm_addr,&w,&h,(unsigned char*)m_yuv_data,&len);
					if(iret >= 0)
					{
						//get data 
						printf("get video data len %d \n",len);
						fwrite(m_yuv_data,1,len,fp);
					}
					ivideo = 0;
				}
#endif					
			}


}


int test_output()
{
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

		//get audio data pcm
		int audiofd;
		if((audiofd = open(param.strAudiofifo, O_RDONLY)) < 0) 
		{          	
			perror("init_audiofifo: open");          	
		}
		FILE *fpaudio=NULL;
		fpaudio = fopen("test.pcm","wb+");
		{
			if(NULL == fpaudio)
				return -1;
		}
#if 1
		//operation pcm
		FILE* fp_op = NULL;
		fp_op = fopen("op.pcm","wb");
		if(NULL == fp_op)
			return -1;

		int src_audiofd = -1;
		src_audiofd = open("src.pcm",O_RDONLY);
		if(src_audiofd < 0)
			return -1;

		unsigned char *src_audio_buff = new unsigned char[1024*10];
		unsigned char *dst_audio_buff = new unsigned char[1024*10];
		int iDst_len = 0;

#endif		
		unsigned long ulpts = 0;
		unsigned long audio_pts = 0;
		int iloop = 25*20000;
		int x,y,x1,y1,w,h,len;
		len = param.width*param.hight*2; 
		int ivideo = 1;
		const int AudioBuffSize = 4608;
		while(pInstance->m_iThreadid != 0)
			{
				//usleep(22*1000);
				//get audio
				{
					len = AudioBuffSize;
					//static inline int shmhdr_get_data(void *shm_addr,int*x,int*y,int*w,int*h,int*x1,int*y1,char* pyuv,int *pLen)
					int iret = read(audiofd,output_audio_data,len);
					if(iret > 0)
					{
						//get data 
						printf("get audio data len %d \n",iret);
						fwrite(output_audio_data,1,iret,fpaudio);
					}
					len = iret;
				}
#if 1
				{
					int ireadlen = read(src_audiofd,src_audio_buff,len);
					printf("--ireadlen=%d \n",ireadlen);
					if(ireadlen < len)
					{	
						lseek(src_audiofd,SEEK_SET,0);
						ireadlen = read(src_audiofd,src_audio_buff,len);
					}
					operation_audio_pcm(output_audio_data,len,src_audio_buff,len,dst_audio_buff,&iDst_len);
					printf("operate audio data len %d \n",iDst_len);
					fwrite(dst_audio_buff,1,iDst_len,fp_op);

					
				}
#else
				//get video
				if(++ivideo >= 2)
				{
					len = BUFFSIZE;
					//static inline int shmhdr_get_data(void *shm_addr,int*x,int*y,int*w,int*h,int*x1,int*y1,char* pyuv,int *pLen)
					int iret = shmhdr_get_data(m_shm_addr,&w,&h,m_yuv_data,&len);
					if(iret > 0)
					{
						//get data 
						printf("get video data len %d \n",len);
						fwrite(m_yuv_data,1,len,fp);
					}
					ivideo = 0;
				}
#endif					
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


}

int main(int argc,char** argv)
{

	const char *short_options = "i:w:h:k:a:v:c:d:?";
	struct option long_options[] = {
		{"help",	no_argument,	   NULL, '?'},
		{"inputurl",	required_argument, NULL, 'i'},
		{"width",	required_argument, NULL, 'w'},
		{"height",	required_argument, NULL, 'h'},
		{"videokey",	required_argument, NULL, 'k'},
		{"audiokey",required_argument, NULL, 'a'},
		{"version", required_argument, NULL, 'v'},
		{"coreid", required_argument, NULL, 'c'},
		{"debug", required_argument, NULL, 'd'},
		{NULL,		0,				   NULL, 0},
	};
	int c = -1;
	int idebug = 0;

	while(1)
	{
		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if(c == -1)
		{
			if(argc == 1)
			{
				fprintf(stderr ,"decoder Version %s Copyright (c) the bbcvsion developers.\n",VERSION);
				return 0;
			}
			break;
		}

		switch(c)
		{
			case '?':
				printf("Usage:%s [options] iputurl width height ....\n",argv[0]);
				printf("decoder version %s\n",VERSION);
				printf("-? --help 		Display this usage information.\n");
				printf("-i --inputurl 	Set the inputurl.\n");
				printf("-w --width 		Set the width.\n");
				printf("-h --height		Set the height.\n");
				printf("-k --videokey	Set the videokey.\n");
				printf("-a --auidoley	Set the auidokey.\n");
				printf("Example: %s --inputurl udp://@225.0.0.1:12345 --width 600 --height 360 --videokey 1001 --audiokey 1002.\n",argv[0]);
				return -1;
			case 'i':
				memset(inputUrl,0,sizeof(inputUrl));
				strncpy(inputUrl,optarg,sizeof(inputUrl));
				iflag |= 0x01;
				printf("get inputurl %s \n",inputUrl);
				break;
			case 'w':
				iwidth = atoi(optarg);
				iflag |= 0x02;
				break;
			case 'h':
				iheight = atoi(optarg);
				iflag |= 0x04;
				break;
			case 'k':
				videokey = atoi(optarg);
				iflag |= 0x08;
				break;
			case 'a':
				//memset(audiofifo,0,sizeof(audiofifo));
				//strncpy(audiofifo,optarg,sizeof(audiofifo));
				audiokey = atoi(optarg);
				iflag |= 0x10;
				break;
			case 'd':
				idebug = atoi(optarg);
				printf("--open dump yuv\n");
				break;
			case 'c':
				idecoder_id= atoi(optarg);
				printf("--core id =%d\n",idecoder_id);
				break;	
			case 'v':
				{
					if(argc == 2)
					{
						fprintf(stderr ,"decoder Version %s Copyright (c) the bbcvsion developers.\n",VERSION);
						return 0;
					}
				}
			}	
	}

	if(iflag != 0x1F)
	{
		fprintf(stderr,"decoder param is error ,please use right call :\n");
		fprintf(stderr,"Example: %s --inputurl udp://@:12345 --width 1280 --height 720 --videokey 1001 --audiofile /tmp/audiofifo.\n",argv[0]);
		return -1;
	}
	
	
	//const char* filename = "/home/ky/rsm-yyd/DecoderTs/1.ts";
	pInstance =  new TSDecoder_Instance();

	
	memset(&param,0,sizeof(param));
	param.width = iwidth;
	param.hight = iheight;
	param.shm_key = videokey;
	param.idecoder_id = idecoder_id;
	strcpy(param.strURL,inputUrl);
	param.audio_key = audiokey;
	//strcpy(param.strAudiofifo,audiofifo);
	
	printf("input url=%s,w=%d,h=%d videokey=%d, audiokey=%d\n",param.strURL,param.width,param.hight,param.shm_key,param.audio_key);
	
	int ret = pInstance->init_Decoder(param);
	if(ret <= 0)
	{
		fprintf(stderr,"--init decoder error return \n");
		return -1;
	}

	if(idebug > 0)
		test_output_new();

	
	while(pInstance->m_iThreadid != 0)
	{
		usleep(1*1000*1000); // 1sec
	}
		
	return 0;
}

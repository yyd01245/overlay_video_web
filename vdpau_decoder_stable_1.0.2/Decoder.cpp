#include "Decoder.h"
#include <math.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#define USE_OVER_GPOFT_DEGUB 0


#define USEFFMPEG_BBCV 0

#define sws_flags SWS_FAST_BILINEAR
//#define pix_fmt AV_PIX_FMT_YUV420P
//#define pix_fmt PIX_FMT_VDPAU
//AV_PIX_FMT_BGRA
enum AVPixelFormat pix_fmt_dst = AV_PIX_FMT_YUV420P;
enum AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P; 


const int BUFFSIZE = 1920*1080*3/2;
#define BUF_SIZE	4096*5
#ifndef AVCODEC_MAX_AUDIO_FRAME_SIZE
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
#endif

const int Queue_size = 15;
const int AudioQue_Size = 10;
const int Video_Buff_size = 1280*720*3/2;
const int Audio_Buff_size = 4608;

typedef unsigned char uint8_t;
const int Recv_Queue_size = 1024*1024;

const int Use_ffmpeg_recv = 0; // 1为使用ffmpeg接收，0为自己接收数据。	


TSDecoder_Instance::TSDecoder_Instance()
{
	m_pVideoCodec = NULL;
	m_pAudioCodec = NULL;
	m_pFmt = NULL;
	m_pVideoCodecCtx = NULL;
	m_pAudioCodecCtx = NULL;
	m_pframe = NULL;
	m_iHeight = 0;
	m_iWidht = 0;
	m_videoindex = -1;
	m_audioindex = -1;

	m_iThreadid = -1;
	m_bstop = false;
	m_ifileType = localfile;
	m_bDecoderFlag = false;
	m_bFirstDecodeSuccess = false;
	m_bBeginPush = false;
}

TSDecoder_Instance::~TSDecoder_Instance()
{
	int iloop = 5;
	while(iloop-- && m_iThreadid !=0)
	{
		stopDecoder(true);
		usleep(1000);
	}
	
	uninit_TS_Decoder();
}



#if 1
void* TSDecoder_Instance::output_data_threadFun(void *param)
{
	TSDecoder_Instance* this0 = (TSDecoder_Instance*)param;

	int output_video_size = BUFFSIZE;
	unsigned char *output_video_yuv420 = new unsigned char[BUFFSIZE];

	int iwidth,iHeight;
	unsigned long ulpts;

	
	struct timeval tv1;
	long start_time,now_time;
	const int FrameInterval = 40;
	bool bStart = true;
	
	unsigned long videopts=0;
	unsigned long audiopts=0;

	unsigned long seekfirst = AV_NOPTS_VALUE;

	gettimeofday(&tv1,NULL);
	int iret = -1;
	int ret = -1;
	start_time = tv1.tv_sec*1000+tv1.tv_usec/1000;
	bool bFirsttime = true;
	int iloop = 10;
	int iloop_getdata = 0;
	do{
		output_video_size = BUFFSIZE;
		if(!this0->m_bGetData){
			usleep(5*1000);
			continue;
		}
#if 0
		if(bFirsttime)
		{ //this0->m_usedQueue.size() <10 && 
#if 0		
			if(iloop-- > 0)
			{
				usleep(100*1000);
				do{
					ret = this0->get_Video_data(output_video_yuv420,&output_video_size,&iwidth,&iHeight,&ulpts);
					if(ret  < 0){
						break;
					}	
				}while(1);
				
				continue;
			}
		
			do{
					ret = this0->get_Video_data(output_video_yuv420,&output_video_size,&iwidth,&iHeight,&ulpts);
					if(ret  < 0){
						break;
					}	
			}while(1);
#endif
#if 0
			ret = this0->get_Video_data(output_video_yuv420,&output_video_size,&iwidth,&iHeight,&ulpts);
			if(ret  >= 0){
				++iloop_getdata;
			}
			if(iloop_getdata < 10)
				continue;
#endif			
		}
		bFirsttime = false;
#endif

#if 0
		if(this0->m_usedQueue.size() <3)
		{
			usleep(5*1000);
			continue;
		}
		
#endif
		
		ret = this0->get_Video_data(output_video_yuv420,&output_video_size,&iwidth,&iHeight,&ulpts);
		if(ret  < 0){
			usleep(5*1000);
			continue;
		}
		if(videopts == 0 || abs(videopts-ulpts) > 3600*2 )
			videopts = ulpts;
		do
		{
			gettimeofday(&tv1,NULL);
			now_time= tv1.tv_sec*1000+tv1.tv_usec/1000;
		
			iret = (ulpts - videopts)/90 - (now_time - start_time ); //???????
			
			iret = iret*9/10;
			
			if(this0->m_usedQueue.size() > Queue_size*4/5){
				iret = iret*3/10;
				fprintf(stderr,"++++++que siez > 4/5 need acc\n");
			}
			
			if(iret > 0){
			//	printf("--------------sleep=%d\n",iret*1000);
				usleep(iret*1000);
			}		
		}while(iret > 0);
		
		start_time = now_time;
		videopts = ulpts;
		if(output_video_size > 0)
			this0->Push_Video_Data_new(iwidth,iHeight,(char*)output_video_yuv420);
		//output_video_size = 0;
	}while(!this0->m_bstop);
	fprintf(stderr,"*******get video Thread over!\n");
	this0->m_iGetdataThreadid = 0;

}

#endif

void* TSDecoder_Instance::get_data_threadFun(void *param)
{
	TSDecoder_Instance* this0 = (TSDecoder_Instance*)param;

	int output_video_size = BUFFSIZE;
	unsigned char *output_video_yuv420 = new unsigned char[BUFFSIZE];

	int iwidth,iHeight;
	unsigned long ulpts;

	
	struct timeval tv1;
	long start_time,now_time;
	const int FrameInterval = 40;
	bool bStart = true;
	
	unsigned long videopts=0;
	unsigned long audiopts=0;

	unsigned long seekfirst = AV_NOPTS_VALUE;

	gettimeofday(&tv1,NULL);
	//int iret;
	start_time = tv1.tv_sec*1000+tv1.tv_usec/1000;

	
	AVPicture pic;
	//printf("codec w=%d h=%d\n",pVideoCodecCtx->width,pVideoCodecCtx->height);
	avpicture_alloc(&pic,pix_fmt,this0->m_pVideoCodecCtx->width,this0->m_pVideoCodecCtx->height);
	int iret;
	//start_time = tv1.tv_sec*1000+tv1.tv_usec/1000;
	VDPAUContext* vd_ctx = (VDPAUContext*)this0->m_pVideoCodecCtx->opaque;
	iwidth = this0->m_pVideoCodecCtx->width;
	iHeight = this0->m_pVideoCodecCtx->height;
	this0->m_iWidht = iwidth;
	this0->m_iHeight = iHeight;

	bool bFirstTime = true;
	this0->m_ulTimebase = start_time;
	//usleep(1000*100);
#if 0
	//printf("--get surface data begin \n");
	do{
		if(!this0->m_bGetData){
			usleep(5*1000);
			continue;
		}
		gettimeofday(&tv1,NULL);
		start_time = tv1.tv_sec*1000+tv1.tv_usec/1000;
		if(vd_ctx->get_surface_data(&pic,&ulpts) >=0 )
		{
			videopts = start_time;
			this0->m_ulTimebase_video = ulpts;

			//this0->Push_Video_Data(iwidth,iHeight,&pic,videopts);
			//videopts += 40;
			//push to shm
			this0->Push_Video_Data_shm(iwidth,iHeight,&pic,videopts);
					
		}
		else{
			usleep(1000);
			continue;
		}		
		gettimeofday(&tv1,NULL);
		now_time= tv1.tv_sec*1000+tv1.tv_usec/1000;
		iret = 40- (now_time - start_time);


		if(iret>0)
		{
			pthread_mutex_lock(&vd_ctx->m_locker);
			if(vd_ctx->m_usedQueue.size() > SURFACE_QUE_SIZE-3){
				iret = iret/2;
				fprintf(stderr,"need accelet time surface size=%d \n",vd_ctx->m_usedQueue.size());
			}
		//	else if(vd_ctx->m_usedQueue.size() < 1){
		//		iret = iret*8/5;
		//	}	
			pthread_mutex_unlock(&vd_ctx->m_locker);
			//printf("--sleep %d ms\n",iret);
			if(iret>0)
				usleep(iret*1000);
		}
	}while(!this0->m_bstop);
#else
	this0->m_ulTimebase = start_time;
	do{
		if(!this0->m_bGetData){
			usleep(5*1000);
			continue;
		}
	//	if(bFirstTime && vd_ctx->m_usedQueue.size() < 5)
	//	{
	//		usleep(1000);
	//		continue;
	//	}
		bFirstTime = false;
		if(vd_ctx->get_surface_data(&pic,&ulpts) >=0)// && this0->m_bBeginPush)
		{
			//videopts = start_time;
			//this0->Push_Video_Data(iwidth,iHeight,&pic,videopts);
			//videopts += 40;
			//push to shm
			this0->m_ulTimebase_video = ulpts;
			fprintf(stderr,"get video ulpts =%ld \n",ulpts);
			this0->Push_Video_Data_shm(iwidth,iHeight,&pic,ulpts);
					
		}
		else{
			usleep(1000);
			continue;
		}		

		//if(videopts==0)
		//	usleep(1000*40);
			
		if(videopts == 0 || abs(videopts-ulpts) > 40*2 )
			videopts = ulpts;
		do
		{
			gettimeofday(&tv1,NULL);
			now_time= tv1.tv_sec*1000+tv1.tv_usec/1000;
		
			iret = (ulpts - videopts) - (now_time - start_time ); //???????
			
			//iret = iret*9/10;
			if(iret>0)
			{
				pthread_mutex_lock(&vd_ctx->m_locker);
				if(vd_ctx->m_usedQueue.size() > SURFACE_QUE_SIZE-3){
					iret = iret/2;
					fprintf(stderr,"need accelet time surface size=%d \n",vd_ctx->m_usedQueue.size());
				}
			//	else if(vd_ctx->m_usedQueue.size() < 3){
			//		iret = iret*2;
			//	}	
				pthread_mutex_unlock(&vd_ctx->m_locker);
				//fprintf(stderr,"--get surface sleep %d ms\n",iret);
				if(iret>0)
					usleep(iret*1000);
			}	
		}while(iret > 0);
		
		start_time = now_time;
		this0->m_ulTimebase = start_time;
		videopts = ulpts;
	}while(!this0->m_bstop);
#endif
	
	fprintf(stderr,"*******get video Thread over!\n");
	this0->m_iGetdataThreadid = 0;

}


void* TSDecoder_Instance::Audio_output_data_threadFun(void *param)
{
	TSDecoder_Instance* this0 = (TSDecoder_Instance*)param;

	const int Audio_Size =  1024*100;
	int input_audio_size = Audio_Size;
	unsigned char *output_audio_data = new unsigned char[Audio_Size];

	int iwidth,iHeight;
	unsigned long ulpts;

	
	struct timeval tv1;
	long start_time,now_time;
	const int FrameInterval = 40;
	bool bStart = true;
	
	unsigned long videopts=0;
	unsigned long audiopts=0;

	unsigned long seekfirst = AV_NOPTS_VALUE;

	gettimeofday(&tv1,NULL);
	int iret;
	start_time = tv1.tv_sec*1000+tv1.tv_usec/1000;
	bool bFirstBase = true;
	//usleep(1000*100);
#if 0
	do{
		if(!this0->m_bGetData){
			usleep(5*1000);
			continue;
		}
		gettimeofday(&tv1,NULL);
		start_time = tv1.tv_sec*1000+tv1.tv_usec/1000;
		input_audio_size = Audio_Size;
		int ret = this0->get_Audio_data(output_audio_data,&input_audio_size,&ulpts);
		if(ret  < 0){
			usleep(5*1000);
			continue;
		}
		if(input_audio_size > 0)
			this0->Push_Audio_Data_shm(output_audio_data,input_audio_size,ulpts);
			//this0->Push_Audio_Data_new(output_audio_data,input_audio_size,ulpts);
		gettimeofday(&tv1,NULL);
		now_time= tv1.tv_sec*1000+tv1.tv_usec/1000;
		iret = 24- (now_time - start_time);

		if(iret>0)
		{
			//control time
			#if 1
			pthread_mutex_lock(&this0->m_audioLocker);
			if(this0->m_audioUsedQueue.size() > AudioQue_Size-3){
				iret = iret/2;
				fprintf(stderr,"need accel time audio size=%d \n",this0->m_audioUsedQueue.size());
			}
	//		else if(this0->m_audioUsedQueue.size() < AudioQue_Size/4){
	//			iret = iret*8/5;
	//		}	
			pthread_mutex_unlock(&this0->m_audioLocker);
			#endif
			//printf("--sleep %d ms\n",iret);
			if(iret>0)
				usleep(iret*1000);
		}
	}while(!this0->m_bstop);
#else
	//usleep(40*1000*10);
	do{
		if(!this0->m_bGetData ){
			usleep(5*1000);
			continue;
		}
		input_audio_size = Audio_Size;
		int ret = this0->get_Audio_data(output_audio_data,&input_audio_size,&ulpts);
		if(ret  < 0 || this0->m_ulTimebase_video==0){
			usleep(5*1000);
			continue;
		}
		//iret = ulpts - this0->m_ulTimebase_video;
	//	fprintf(stderr,"---audio wait %d, videotimebase =%ld,audiopts=%ld\n",
	//	iret,this0->m_ulTimebase_video,ulpts);
		
#if 0		
		while(iret >500)
		{
			iret = ulpts - this0->m_ulTimebase_video;
			if(iret > 500 ){

				usleep(1000*iret);

			}
		}
#endif
		//if(videopts==0)
		//	usleep(1000*80);

		fprintf(stderr,"get audio ulpts =%ld audioque size=%d\n",ulpts,this0->m_audioUsedQueue.size());
	//	if(videopts == 0 || abs(videopts-ulpts) > 40*2 )
	//		videopts = ulpts;
		do
		{
			gettimeofday(&tv1,NULL);
			now_time= tv1.tv_sec*1000+tv1.tv_usec/1000;
		
			iret = (ulpts - this0->m_ulTimebase_video) - (now_time - this0->m_ulTimebase); //???????

			if(this0->m_audioUsedQueue.size() > AudioQue_Size-3){
				iret = iret/2;
			//	fprintf(stderr,"need accel time audio size=%d \n",this0->m_audioUsedQueue.size());
			}
			//iret = iret*9/10;
			if(iret > 0){
				//printf("audio --------------sleep=%d\n",iret*1000);
				usleep(iret*1000);
			}		
		}while(iret > 0);
		
		//start_time = now_time;
		videopts = ulpts;
		if(input_audio_size > 0)
			this0->Push_Audio_Data_shm(output_audio_data,input_audio_size,ulpts);
			//this0->Push_Audio_Data_new(output_audio_data,input_audio_size,ulpts);

	}while(!this0->m_bstop);
#endif

	fprintf(stderr,"*******get audio Thread over!\n");
	this0->m_iGetdataThreadid = 0;

}




FILE *fpyuv=NULL;

void* TSDecoder_Instance::decoder_threadFun(void *param)
{
	TSDecoder_Instance* this0 = (TSDecoder_Instance*)param;

	AVCodec *pVideoCodec, *pAudioCodec;
	AVCodecContext *pVideoCodecCtx = NULL;
	AVCodecContext *pAudioCodecCtx = NULL;
	//AVIOContext * pb = NULL;
	//AVInputFormat *piFmt = NULL;
	AVFormatContext *pFmt = NULL;

	this0->m_iThreadid = pthread_self();
	int ret = this0->initdecoder();
	if(ret >=0 )
	{
		//新建yuv输出共享内存
		if(this0->init_shmaddr() < 0)
		{
			fprintf(stderr,"*******decoderThread uninit shmaddr over!\n");
			this0->m_iThreadid = 0;
			return NULL;
		}
		
		if(this0->init_audioshm() < 0)
		{
			fprintf(stderr,"*******decoderThread uninit shmaddr over!\n");
			this0->m_iThreadid = 0;
			return NULL;
		}
		#if 0
        //new audio fifo
		if(this0->init_audiofifo() < 0)
		{
			fprintf(stderr,"*******decoderThread uninit audiofifo over!\n");
			this0->m_iThreadid = 0;
			return NULL;
		}
		#endif
		this0->initQueue(Queue_size);
		this0->m_swr_ctx = NULL;
#if 0		
		//create resmpale audio
		//this0->m_audio_resample = avresample_alloc_context();
#endif

	}
	else
	{
		fprintf(stderr,"*******decoderThread uninitdecoder over!\n");
		this0->m_iThreadid = 0;
		return NULL;
	}

	//printf("begin decode thread\n");
	bool bInitDecodeFlag = false;

	pVideoCodec = this0->m_pVideoCodec;
	pAudioCodec = this0->m_pAudioCodec;
	pFmt = this0->m_pFmt;
	pVideoCodecCtx = this0->m_pVideoCodecCtx;
	pAudioCodecCtx = this0->m_pAudioCodecCtx;

	int videoindex = this0->m_videoindex;
	int audioindex = this0->m_audioindex;

	int got_picture;
	uint8_t samples[AVCODEC_MAX_AUDIO_FRAME_SIZE*3/2];
	AVFrame *pframe = avcodec_alloc_frame();
	AVPicture  *pframe2 = (AVPicture  *)av_mallocz(sizeof(AVPicture));
	avpicture_alloc(pframe2,pix_fmt_dst,this0->m_tsParam.width,this0->m_tsParam.hight);
	AVPicture pic;
	//printf("codec w=%d h=%d\n",pVideoCodecCtx->width,pVideoCodecCtx->height);
	avpicture_alloc(&pic,pix_fmt,pVideoCodecCtx->width,pVideoCodecCtx->height);

	this0->m_pframe = pframe2;
	AVPacket pkt;
	av_init_packet(&pkt);

	struct timeval tv1;
	long start_time,now_time,start_audio_time;
	const int FrameInterval = 40;
	bool bStart = true;
	
	unsigned long videopts=0;
	unsigned long audiopts=0;

	unsigned long seekfirst = AV_NOPTS_VALUE;

	gettimeofday(&tv1,NULL);
	start_time = tv1.tv_sec*1000+tv1.tv_usec/1000;
	start_audio_time = start_time;
	bool firstopen = true;
	int iKeyFrameTime = 0;
	printf("--start time %ld\n",start_time);

	//usleep(3*1000*1000);
	//fpyuv = fopen("test.pes","w+b");
	//FILE *fpvideo= fopen("video.pes","wb+");
	int iwritetime = 10;

	pthread_t Data_thread;
	pthread_create(&Data_thread, NULL, TSDecoder_Instance::get_data_threadFun, this0);
	pthread_detach(Data_thread);

	//FILE* fpout = fopen("audio.pcm", "wb");

	int inum = 0;
	VDPAUContext* vd_ctx = (VDPAUContext*)pVideoCodecCtx->opaque;
	do
	{
		int iret = av_read_frame(pFmt, &pkt);
		if (iret >= 0) 
		{

			//if(this0->m_bDecoderFlag)
				//fwrite(pkt.data,1,pkt.size,fpyuv);
			if (pFmt->streams[pkt.stream_index]->codec->codec_type == AVMEDIA_TYPE_VIDEO) 
			{
				//if(this0->m_bDecoderFlag)
					//fwrite(pkt.data,1,pkt.size,fpvideo);
			//	fprintf(stderr,"-----packet VIDEO dts =%ld pts=%ld\n",pkt.dts/90,pkt.pts/90);
			//	if(this0->m_ifileType != internetfile)
				{
					if(videopts == 0 || abs(videopts-pkt.dts) > 3600*2 )
						videopts = pkt.dts;
					int iret=0;
					do
					{
						gettimeofday(&tv1,NULL);
						now_time= tv1.tv_sec*1000+tv1.tv_usec/1000;
					
						iret = (pkt.dts - videopts)/90 - (now_time - start_time ); //适当的加快一点

						if(vd_ctx->m_usedQueue.size() < 2)
							iret = iret/2;
						if(iret > 0){
							//printf("--------------sleep=%d\n",iret*1000);
							usleep(iret*1000);
						}		
					}while(iret > 0);
					
					start_time = now_time;
					videopts = pkt.dts;
				}

				avcodec_decode_video2(pVideoCodecCtx, pframe, &got_picture, &pkt);
				if (got_picture) 
				{
					#if USE_OVER_GPOFT_DEGUB
						if(++inum > 1000)
							break;
					#endif

#if 0					
					gettimeofday(&tv1,NULL);
					now_time= tv1.tv_sec*1000+tv1.tv_usec/1000;
					
					if( !this0->m_bGetData && this0->m_ifileType == internetfile &&
						(lasttime ==0 || now_time-lasttime<5))
					{
						lasttime = now_time;
						fprintf(stderr,"-----drop packet success dts =%ld pts=%ld\n",pkt.dts/90,pkt.pts/90);
						vd_ctx->Push_render2freeque(pframe);
						continue;
					}
				

					gettimeofday(&tv1,NULL);
					now_time= tv1.tv_sec*1000+tv1.tv_usec/1000;
					
					if(!( !this0->m_bGetData && this0->m_ifileType == internetfile &&
						(lasttime ==0 || now_time-lasttime<5)))
					{
						this0->m_bBeginPush = true;
						
					}
					lasttime = now_time;
#endif	

					if(!this0->m_bGetData ){
						//avpicture_alloc(&pic,AV_PIX_FMT_BGRA,pVideoCodecCtx->width,pVideoCodecCtx->hight);
						printf("--decode time %ld\n",start_time);
						vd_ctx->m_bNeedRecorve = false;

					}
					this0->m_bGetData = true;
                #if USE_OVER_GPOFT_DEBUG  
					gettimeofday(&tv1,NULL);
					now_time= tv1.tv_sec*1000+tv1.tv_usec/1000;
					printf("decode time =%d ms\n",now_time-start_time);
                #endif   
									gettimeofday(&tv1,NULL);
					now_time= tv1.tv_sec*1000+tv1.tv_usec/1000;
					
					//fprintf(stderr,"-----push video to que data dts =%ld pts=%ld push video time=%ld\n",pkt.dts/90,pkt.pts/90,now_time);
					vd_ctx->Push_render2que(pframe,pkt.dts/90);
/*					{
						int iwidth = pVideoCodecCtx->width;
						int iHeight = pVideoCodecCtx->height;
						this0->m_iWidht = iwidth;
						this0->m_iHeight = iHeight;
						this0->Push_Video_Data(iwidth,iHeight,&pic,pkt.dts);
						//this0->Push_Video_Data_new(iwidth,iHeight,&pic,pkt.dts);
					}
*/					
#if USE_OVER_GPOFT_DEBUG 
					gettimeofday(&tv1,NULL);
					now_time= tv1.tv_sec*1000+tv1.tv_usec/1000;
					printf("all video time =%d ms\n",now_time-start_time);
#endif					
				}
				else{
					if(this0->m_bGetData == true)
						vd_ctx->Push_render2freeque(pframe);
				}
				
			}
			else if (pFmt->streams[pkt.stream_index]->codec->codec_type == AVMEDIA_TYPE_AUDIO) 
			{
				if(this0->m_bGetData == true) //只有视频解码成功了才写音频
				{
					int iget_audio =-1;
#if 1
				#if 0
					if(this0->m_ifileType != internetfile)
					{
						if(audiopts == 0 || abs(audiopts-pkt.dts) > 3600*2 )
							audiopts = pkt.dts;
						int iret=0;
						do
						{
							gettimeofday(&tv1,NULL);
							now_time= tv1.tv_sec*1000+tv1.tv_usec/1000;
						
							iret = (pkt.dts - audiopts)/90 - (now_time - start_audio_time ); //适当的加快一点
						
							//iret = iret*9/10;
							if(iret > 0){
								//printf("audio decode--------------sleep=%d\n",iret*1000);
								usleep(iret*1000);
							}		
						}while(iret > 0);
						
						start_audio_time = now_time;
						audiopts = pkt.dts;
					}
				#endif	
					gettimeofday(&tv1,NULL);
					now_time= tv1.tv_sec*1000+tv1.tv_usec/1000;
						
				//	fprintf(stderr,"-----AUDIO dts =%ld pts=%ld push audio time=%ld\n",pkt.dts/90,pkt.pts/90,now_time);
					avcodec_decode_audio4(pAudioCodecCtx,pframe,&iget_audio,&pkt);
					if(iget_audio)
					{
						int frame_size = 0;
#if 0
						int fmt_len = av_get_bytes_per_sample(pAudioCodecCtx->sample_fmt);
						if(fmt_len<0){
							fprintf(stderr,"Failed to calculate data size\n");
							exit(1);
						}
						
						memset(samples,0,sizeof(samples));
			            for (int i=0; i<pframe->nb_samples; i++)
			                for (int ch=0; ch<pAudioCodecCtx->channels; ch++){		                
								memcpy(samples+frame_size,pframe->data[ch]+fmt_len*i,fmt_len);
								
								//fwrite(pframe->data[ch]+fmt_len*i,1,fmt_len,fpout);	
								frame_size += fmt_len;
		                	}
#endif
						frame_size = this0->init_SwrConverAudio(pframe,samples);
						this0->Push_Audio_Data(samples,frame_size,pkt.dts/90);
						//this0->Push_Audio_Data(pframe->data[ch]+fmt_len*i,fmt_len,pkt.dts);		
					}
#endif						
#if 0
					int frame_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*3/2;
					if (avcodec_decode_audio3(pAudioCodecCtx, (int16_t *)samples, &frame_size, &pkt) >= 0) 
					{
						//this0->inputaudiotofifo(samples,frame_size,pkt.dts);
						printf("audio pts=%d dts =%d framesize=%d \n",pkt.pts,pkt.dts,frame_size);
				//this0->Push_Audio_Data_new(samples,frame_size,pkt.dts);
						this0->Push_Audio_Data(samples,frame_size,pkt.dts);
						//fprintf(stderr, "decode one audio frame!\r");
					}
#endif					
				}
			}
			av_free_packet(&pkt);
		}
		else
		{
			
			fprintf(stderr,"=========read frame failed %d\n",iret);
			char errbuf[50]={0};
			av_strerror(ret, errbuf, sizeof(errbuf));
        	fprintf(stderr, "read frame error: %s\n",errbuf);
			//if file need loop
			av_free_packet(&pkt);
			if(this0->m_ifileType == internetfile)
			{
				//udp stream
				break;
			}
			else
			{	
				//file or rtsp ,
				//just file
				usleep(1000);
				int ret = av_seek_frame(pFmt, -1, AV_NOPTS_VALUE, AVSEEK_FLAG_BACKWARD|AVSEEK_FLAG_ANY);
				//int ret = avformat_seek_file(pFmt, -1, INT64_MIN, seekfirst, seekfirst, 0);
				printf("--loop status %d \n",ret );
				if(ret < 0){
					fprintf(stderr,"--loop error %d \n",ret );
					break;
				}
			}
			
			//break; //quit
		}
	}while(!this0->m_bstop);
	fprintf(stderr,"*******decoderThread over!\n");
	this0->m_iThreadid = 0;
}

int TSDecoder_Instance::get_queue(uint8_t* buf, int size) 
{
	
	int ret = m_recvqueue.get_queue(buf,size);

	return ret;
}

FILE* fpreaddata= NULL;
int TSDecoder_Instance::read_data(void *opaque, uint8_t *buf, int buf_size) {
//	UdpParam udphead;
	TSDecoder_Instance* this0 = (TSDecoder_Instance*)opaque;
	int size = buf_size;
	int ret;
//	if(NULL == fpreaddata)
//		fpreaddata = fopen("readdata.pes","wb+");
//	printf("read data %d\n", buf_size);
	do {
		
		ret = this0->get_queue(buf, buf_size);
		if(ret < 0)
			usleep(50);
		//printf("-------read_data ret = %d size=%d\n",ret,buf_size);
		//size += ret;
	} while (ret < 0);
	//printf("read data Ok %d\n", buf_size);
//	if(this0->m_bDecoderFlag)
//		fwrite(buf,1,size,fpreaddata);
	return size;
}

bool TSDecoder_Instance::open_inputdata()
{
#if 0
	AVCodec *pVideoCodec, *pAudioCodec;
	AVInputFormat *piFmt = NULL;


	struct timeval tm;

	gettimeofday(&tm,NULL);
	printf("-----init time 1 =%ld\n",tm.tv_sec*1000+tm.tv_usec/1000);

	int iloop = 10;
	do
	{
		if (av_probe_input_buffer(m_pb, &piFmt, "", NULL, 0, 0) < 0) {
			fprintf(stderr, "probe failed!\n");
		} else {
			fprintf(stdout, "probe success!\n");
			fprintf(stdout, "format: %s[%s]\n", piFmt->name, piFmt->long_name);
			break;
		}
		//usleep(100);
	}while(iloop-- >=0);
	m_pFmt->pb = m_pb;

	if (avformat_open_input(&m_pFmt, "", piFmt, NULL) < 0) {
		fprintf(stderr, "avformat open failed.\n");
		return false;
	} else {
		fprintf(stdout, "open stream success!\n");
	}

	
	gettimeofday(&tm,NULL);
	printf("-----loop =%d	init time 2 =%ld\n",10-iloop,tm.tv_sec*1000+tm.tv_usec/1000);


	enum AVCodecID videoCodeID;
	enum AVCodecID audioCodeID;

	switch(m_vCodetype)
	{
		case CODE_HD_VIDEO:
		{
			videoCodeID = AV_CODEC_ID_H264;
			break;
		}
		case CODE_SD_VIDEO:
		{
			videoCodeID = AV_CODEC_ID_MPEG2VIDEO;
			break;
		}
		default:
		{
			videoCodeID = AV_CODEC_ID_MPEG2VIDEO;
			break;
		}
	}

	switch(m_aCodeType)
	{
		case CODE_AUIDO_MP2:
		{
			audioCodeID = AV_CODEC_ID_MP2;
			break;
		}
		case CODE_AUIDO_MP3:
		{
			audioCodeID = AV_CODEC_ID_MP3;
			break;
		}
		case CODE_AUDIO_AAC:
		{
			audioCodeID = AV_CODEC_ID_AAC;
			break;
		}
		default:
		{
			audioCodeID = AV_CODEC_ID_MP2;
			break;
		}
	}

	
	pVideoCodec = avcodec_find_decoder(videoCodeID);
	if (!pVideoCodec) {
		fprintf(stderr, "could not find video decoder!\n");
		return false;
	}
	if (avcodec_open(m_pVideoCodecCtx, pVideoCodec) < 0) {
		fprintf(stderr, "could not open video codec!\n");
		return false;
	}


	printf("**********vidoe eum=%d auido enum=%d*******video type=%d audiotype=%d \n",m_vCodetype,m_aCodeType,videoCodeID,audioCodeID);

	pAudioCodec = avcodec_find_decoder(audioCodeID);
	if (!pAudioCodec) {
		fprintf(stderr, "could not find audio decoder!\n");
		return false;
	}
	if (avcodec_open(m_pAudioCodecCtx, pAudioCodec) < 0) {
		fprintf(stderr, "could not open audio codec!\n");
		return false;
	}

	m_pVideoCodec = pVideoCodec;
	m_pAudioCodec = pAudioCodec;
	m_pframe = NULL;
#endif
	return true;
}

int TSDecoder_Instance::init_audiofifo()
{
	char *pipename =  m_tsParam.strAudiofifo;        
	//print("pipename is  %s \n",pipename);
	if((mkfifo(pipename, O_CREAT|O_RDWR|0777)) && (errno != EEXIST))  
	{
		fprintf(stderr,"init_audiofifo : mkfifo pipe failed: errno=%d\n",errno);
		return -2;
	}
	
	chmod(pipename,0777);
	
	//if((m_audiofd = open(pipename, O_RDWR|O_NONBLOCK)) < 0)   
	if((m_audiofd = open(pipename, O_RDWR)) < 0) 
	{          	
		perror("init_audiofifo: open");          	
		return -3;      	
	}
	//printf("init audio fifo success \n");
	return 0;
}


int TSDecoder_Instance::init_shmaddr()
{
	//先创建yuv输出共享内存
	m_shm_size = sizeof(videosurf) + m_tsParam.width*m_tsParam.hight*3/2;

	m_shm_id = shmget((key_t)m_tsParam.shm_key,m_shm_size, 0666|IPC_CREAT);
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
	videosurf* hdr = (videosurf*)m_shm_addr;
	char *shm_yuv = (char*)(hdr->data);
    //LOCK
    int itime = 10;
	memset(hdr,0,sizeof(videosurf));
	printf("--shm head flag =%d \n",hdr->flag);
	m_yuv_data = (char*)malloc(m_tsParam.hight*m_tsParam.width*2);//缓存
	printf("crate shmaddr over malloc len %d \n",m_tsParam.hight*m_tsParam.width*2);

	m_video_buff_data = new VideoData;
	m_video_buff_data->data = new unsigned char[Video_Buff_size];

	return 0;

}

int TSDecoder_Instance::init_audioshm()
{
	//先创建输出共享内存
	m_audioshm_size= sizeof(audiofrag) + Audio_Buff_size;

	m_audioshm_id = shmget((key_t)m_tsParam.audio_key,m_audioshm_size, 0666|IPC_CREAT);
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
	audiofrag* hdr = (audiofrag*)m_audioshm_addr;
	char *audioshm_pcm = (char*)(hdr->data);
    //LOCK
	memset(hdr,0,sizeof(audiofrag));
	printf("--audio shm head flag =%d \n",hdr->flag);
	
	return 0;

}


int TSDecoder_Instance::init_Decoder(TSDecoderParam param)
{
	memcpy(&m_tsParam,&param,sizeof(TSDecoderParam));
	int ret = 0; //int ret = initdecoder();

	//判断是否是udp网络数据
	char filehead[128] = {0};
	int iheadlen = 4;
	strncpy(filehead,m_tsParam.strURL,iheadlen);
	if(strncmp(filehead,"udp:",iheadlen) == 0 )
	{
		printf("*********udp stream data **********\n");
		m_ifileType = internetfile;
	}

	pthread_mutex_init(&m_locker,NULL);
	pthread_mutex_init(&m_audioLocker,NULL);

	//if(ret >= 0)
	{
		pthread_t Decoder_thread;
		pthread_create(&Decoder_thread, NULL, TSDecoder_Instance::decoder_threadFun, this);
		pthread_detach(Decoder_thread);

		pthread_t Data_thread;
		pthread_create(&Data_thread, NULL, TSDecoder_Instance::output_data_threadFun, this);
		pthread_detach(Data_thread);
		
		pthread_t audioData_thread;
		pthread_create(&audioData_thread, NULL, TSDecoder_Instance::Audio_output_data_threadFun, this);
		pthread_detach(audioData_thread);
	}
	while(!m_bGetData)
		usleep(5*1000); //5ms
	ret = m_bGetData;
	return ret;
}


int TSDecoder_Instance::init_SwrConverAudio(AVFrame * pAudioDecodeFrame,uint8_t * out_buf)
{
	int data_size = 0;	
	int ret = 0;  
	int64_t src_ch_layout = AV_CH_LAYOUT_STEREO; //初始化这样根据不同文件做调整  
	int64_t dst_ch_layout = AV_CH_LAYOUT_STEREO; //这里设定ok  
	int dst_nb_channels = 0;  
	int dst_linesize = 0;  
	int src_nb_samples = 0;  
	int dst_nb_samples = 0;  
	int max_dst_nb_samples = 0;  
	uint8_t **dst_data = NULL;	
	int resampled_data_size = 0; 
	SwrContext * swr_ctx = m_swr_ctx; 
	int out_channels =2;
	int out_sample_fmt = 1; //AV_SAMPLE_FMT_S16;
	int out_sample_rate = 48000;

	if(NULL == m_swr_ctx)
	{


		swr_ctx = swr_alloc();	
		if (!swr_ctx)  
		{  
		   printf("swr_alloc error \n");  
		   return -1;  
		}  
		src_ch_layout = (m_pAudioCodecCtx->channel_layout &&   
			  m_pAudioCodecCtx->channels ==   
			  av_get_channel_layout_nb_channels(m_pAudioCodecCtx->channel_layout)) ?	
			  m_pAudioCodecCtx->channel_layout :	
		  	  av_get_default_channel_layout(m_pAudioCodecCtx->channels);  
		printf("---src_ch_layout =%ld \n",src_ch_layout);	
		if (out_channels == 1)  
		{  
		  dst_ch_layout = AV_CH_LAYOUT_MONO;  
		}  
		else if(out_channels == 2)  
		{  
		  dst_ch_layout = AV_CH_LAYOUT_STEREO;	
		}  
		else	
		{  
		  //可扩展	
		}  
		if (src_ch_layout <= 0)  
		{  
		  printf("src_ch_layout error \n");  
		  return -1;  
		}  

		src_nb_samples = pAudioDecodeFrame->nb_samples;  
	    if (src_nb_samples <= 0)  
	    {  
	        printf("src_nb_samples error \n");  
	        return -1;  
	    }  
	  
	    /* set options */  
	    av_opt_set_int(swr_ctx, "in_channel_layout",    src_ch_layout, 0);  
	    av_opt_set_int(swr_ctx, "in_sample_rate",       m_pAudioCodecCtx->sample_rate, 0);  
	    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", m_pAudioCodecCtx->sample_fmt, 0);  
	  
	    av_opt_set_int(swr_ctx, "out_channel_layout",    dst_ch_layout, 0);  
	    av_opt_set_int(swr_ctx, "out_sample_rate",       out_sample_rate, 0);  
	    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", (AVSampleFormat)out_sample_fmt, 0);  
	    swr_init(swr_ctx);  
		m_swr_ctx = swr_ctx;
	}
	src_nb_samples = pAudioDecodeFrame->nb_samples;  
    if (src_nb_samples <= 0)  
    {  
        printf("src_nb_samples error \n");  
        return -1;  
    }  

	//printf("--src_nb_samples=%d,out_samplerate=%d \n",src_nb_samples, out_sample_rate);
	max_dst_nb_samples = dst_nb_samples =  
        av_rescale_rnd(src_nb_samples, out_sample_rate, m_pAudioCodecCtx->sample_rate, AV_ROUND_UP); 
	//printf("--src sample rate=%d ,dst_nb_samples=%d \n",m_pAudioCodecCtx->sample_rate,dst_nb_samples);
    if (max_dst_nb_samples <= 0)  
    {  
        printf("av_rescale_rnd error \n");  
        return -1;  
    }  
  
    dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);  
    ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,  
        dst_nb_samples, (AVSampleFormat)out_sample_fmt, 0);  
    if (ret < 0)  
    {  
        printf("av_samples_alloc_array_and_samples error \n");  
        return -1;  
    }  

    dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, m_pAudioCodecCtx->sample_rate) +  
        src_nb_samples, out_sample_rate, m_pAudioCodecCtx->sample_rate,AV_ROUND_UP);  
	//printf("dst_nb_samples = %d \n",dst_nb_samples);
	if (dst_nb_samples <= 0)  
    {  
        printf("av_rescale_rnd error \n");  
        return -1;  
    }  
    if (dst_nb_samples > max_dst_nb_samples)  
    {  
        av_free(dst_data[0]);  
        ret = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels,  
            dst_nb_samples, (AVSampleFormat)out_sample_fmt, 1);  
        max_dst_nb_samples = dst_nb_samples;  
    }  
  
    data_size = av_samples_get_buffer_size(NULL, m_pAudioCodecCtx->channels,  
        pAudioDecodeFrame->nb_samples,  
        m_pAudioCodecCtx->sample_fmt, 1);  
    if (data_size <= 0)  
    {  
        printf("av_samples_get_buffer_size error \n");  
        return -1;  
    }  
    resampled_data_size = data_size;  
	//printf("src data size=%d \n",data_size);

	if (swr_ctx)  
    {  
        ret = swr_convert(swr_ctx, dst_data, dst_nb_samples,   
            (const uint8_t **)pAudioDecodeFrame->data, pAudioDecodeFrame->nb_samples);  
        if (ret <= 0)  
        {  
            printf("swr_convert error \n");  
            return -1;  
        }  
  
        resampled_data_size = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,  
            ret, (AVSampleFormat)out_sample_fmt, 1);  
        if (resampled_data_size <= 0)  
        {  
            printf("av_samples_get_buffer_size error \n");  
            return -1;  
        }  
    }  
    else   
    {  
        printf("swr_ctx null error \n");  
        return -1;  
    }  
  	//printf("--swr convert len=%d\n",resampled_data_size);
    //将值返回去  
    memcpy(out_buf,dst_data[0],resampled_data_size);  
  
    if (dst_data)  
    {  
        av_freep(&dst_data[0]);  
    }  
    av_freep(&dst_data);  
    dst_data = NULL;  
  
    return resampled_data_size;  

}

int TSDecoder_Instance::initdecoder()
{
	
	AVCodec *pVideoCodec, *pAudioCodec;
	AVCodecContext *pVideoCodecCtx = NULL;
	AVCodecContext *pAudioCodecCtx = NULL;
	AVIOContext * pb = NULL;
//	AVInputFormat *piFmt = NULL;
	AVFormatContext *pFmt = NULL;


	AVFormatContext *pFmt2 = NULL;

//logfile
	char logfilename[1024]={0};
	sprintf(logfilename,"/tmp/decoder_%d_%d.log",m_tsParam.shm_key,m_tsParam.audio_key);

	m_fdlog = open(logfilename,O_CREAT|O_TRUNC|O_WRONLY,0666);
	if(m_fdlog < 0)
	{
		fprintf(stderr,"open logfile %s error \n",logfilename);
		return -1;
	}
	char tmp_txt[512]={0};
	pid_t pid_id = getpid();
	sprintf(tmp_txt,"decoder PID=%d,url:%s,w:%d,h:%d,videokey:%d,audiokey:%d \n",pid_id,m_tsParam.strURL,
			m_tsParam.width,m_tsParam.hight,m_tsParam.shm_key,m_tsParam.audio_key);
	write(m_fdlog,tmp_txt,strlen(tmp_txt));

	
	
	struct timeval tm;

	av_register_all();
	avformat_network_init();
	printf("-------------file =%s\n",m_tsParam.strURL);

	pFmt = avformat_alloc_context();
	//pFmt2 = avformat_alloc_context();
	//printf("allocc context \n");

	char strurl[1024]={0};
	sprintf(strurl,"%s?fifo_size=%d",m_tsParam.strURL,Recv_Queue_size);
	printf("---change fifisize: %s \n",strurl);
	if(avformat_open_input(&pFmt,strurl,NULL,NULL)!=0)
	{
		fprintf(stderr,"cannot open input avformat_open_input \n");
		return -1;
	}
	//printf("======max_analyze_duration=%d,probesize=%ld==== \n",pFmt->max_analyze_duration,pFmt->probesize);		
	//printf("======max_analyze_duration2=%d,probesize2=%ld==== \n",pFmt->max_analyze_duration2,pFmt->probesize2);

	pFmt->max_analyze_duration2  = 30*1000000;
	pFmt->probesize2 = 50000000;
	pFmt->max_analyze_duration  = 30*1000000;
	pFmt->probesize =  50000000;
	
	//printf("begin open input success\n");
	if(avformat_find_stream_info(pFmt,NULL)<0)
	{
		fprintf(stderr,"couldn't find stream information \n");
		return -2;
	}
	
	//printf("======max_analyze_duration=%d,probesize=%ld==== \n",pFmt->max_analyze_duration,pFmt->probesize);		
	//printf("======max_analyze_duration2=%d,probesize2=%ld==== \n",pFmt->max_analyze_duration2,pFmt->probesize2);
	//printf("begin find stream success\n");

	av_dump_format(pFmt, 0, "", 0);

	//printf("dump format success\n");
	int videoindex = -1;
	int audioindex = -1;
	for (int i = 0; i < pFmt->nb_streams; i++) 
	{
		if ( (pFmt->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) &&
				(videoindex < 0) ) {
			videoindex = i;
		}
		if ( (pFmt->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) &&
				(audioindex < 0) ) {
			audioindex = i;
		}
	}
	if (videoindex < 0 || audioindex < 0) {
		fprintf(stderr, "videoindex=%d, audioindex=%d\n", videoindex, audioindex);
		return -3;
	}

	//printf("find avstream success\n");

	AVStream *pVst,*pAst;
	pVst = pFmt->streams[videoindex];
	pAst = pFmt->streams[audioindex];

	
	pVideoCodecCtx = pVst->codec;
	pAudioCodecCtx = pAst->codec;

		//printf("begin open input success\n");
	pAudioCodec = avcodec_find_decoder(pAudioCodecCtx->codec_id);
	if (!pAudioCodec) {
		fprintf(stderr, "could not find audio decoder!\n");
		return -1;
	}
	if (avcodec_open2(pAudioCodecCtx, pAudioCodec,NULL) < 0) {
		fprintf(stderr, "could not open audio codec!\n");
		return -1;
	}

	printf("init vdpaucontext coreid=%d \n",m_tsParam.idecoder_id);
	VDPAUContext *vc = new VDPAUContext(m_tsParam.idecoder_id);
	pVideoCodecCtx->opaque=vc;
    pVideoCodec=vc->open_vdpau_codec(pVideoCodecCtx);

	if (avcodec_open2(pVideoCodecCtx, pVideoCodec,NULL) < 0) {
		fprintf(stderr, "could not open video codec!\n");
		return -1;
	}
	// pVst->codec = pVideoCodecCtx;
	  //avcodec_copy_context(ist->dec_ctx, dec);


	
	m_pVideoCodec = pVideoCodec;
	m_pAudioCodec = pAudioCodec;
	m_pFmt = pFmt;
	m_pVideoCodecCtx = pVideoCodecCtx;
	m_pAudioCodecCtx = pAudioCodecCtx;
	m_pframe = NULL;
	//m_iHeight = iHeight;
	//m_iWidht = iWidht;
	m_videoindex = videoindex;
	m_audioindex = audioindex;

	m_img_convert_ctx = NULL;
	
	printf("init decoder success\n");

	pthread_mutex_init(&m_locker,NULL);
	pthread_mutex_init(&m_audioLocker,NULL);

	memset(tmp_txt,0,sizeof(tmp_txt));
	strcpy(tmp_txt,"init decoder success\n");
	write(m_fdlog,tmp_txt,strlen(tmp_txt));
	return 0;
}




bool TSDecoder_Instance::initQueue(int isize)
{
#if 0
	for(int i=0;i<isize;++i)
	{
		VideoData* tmpbuff = new VideoData;
		tmpbuff->data = new unsigned char[Video_Buff_size];
		m_freeQueue.push_back(tmpbuff);
	}
#endif

	//音频多点缓存
	for(int i=0;i<AudioQue_Size;++i)
	{
		AudioData* audiobuff = new AudioData;
		audiobuff->data = new unsigned char[Audio_Buff_size];
		m_audioFreeQueue.push_back(audiobuff);
	}
	
	
	return true;
}

bool TSDecoder_Instance::freeQueue()
{

	pthread_mutex_lock(&m_locker);
	while(m_usedQueue.size() > 0)
	{
		VideoData* tmpbuff = m_usedQueue.front();
		m_usedQueue.pop_front();
		delete tmpbuff->data;
		tmpbuff->data = NULL;
		delete tmpbuff;
	}
	while(m_freeQueue.size() > 0)
	{
		VideoData* tmpbuff = m_freeQueue.front();
		m_freeQueue.pop_front();
		delete tmpbuff->data;
		delete tmpbuff;
		tmpbuff = NULL;
	}

	pthread_mutex_unlock(&m_locker);

	pthread_mutex_lock(&m_audioLocker);
	while(m_audioFreeQueue.size() > 0)
	{
		AudioData* audiobuff = m_audioFreeQueue.front();
		m_audioFreeQueue.pop_front();
		delete audiobuff->data;
		delete audiobuff;
		audiobuff = NULL;
	}
	while(m_audioUsedQueue.size() > 0)
	{
		AudioData* audiobuff = m_audioUsedQueue.front();
		m_audioUsedQueue.pop_front();
		delete audiobuff->data;
		delete audiobuff;
		audiobuff = NULL;
	}

	pthread_mutex_unlock(&m_audioLocker);
	return true;
}


FILE* fpYUV = NULL;

void TSDecoder_Instance::Push_Video_Data_new(int iwidth, int ihight, char *pInfram)
{	
//	printf("begin push data \n");


/*	
	if(fpYUV == NULL)
	{
		fpYUV = fopen("org.yuv", "w+b");
	}
	fwrite(buff, 1, iWidth*iHight*3/2, fpYUV);
*/
		if(pix_fmt_dst == AV_PIX_FMT_YUV420P)
		{
			shmhdr_input_data(m_shm_addr,m_tsParam.width,m_tsParam.hight,pInfram,iwidth*ihight*3/2);
		}
		else if(pix_fmt_dst == AV_PIX_FMT_BGRA)
		{
			shmhdr_input_data(m_shm_addr,m_tsParam.width,m_tsParam.hight,pInfram,iwidth*ihight*4);

		}
	

}


void TSDecoder_Instance::Push_Video_Data_new(int iwidth, int ihight, AVPicture *pInfram,unsigned long ulPTS)
{	
	//printf("begin push data \n");
	unsigned char *buff = (unsigned char*)m_yuv_data;
	if(m_yuv_data == NULL)
	{
		fprintf(stderr," yuv data buff null \n");
		return ;
	}
	AVPicture *pAVfram = pInfram;
	int iWidth = iwidth;
	int iHight = ihight;
	
	//scale
//	static struct SwsContext *img_convert_ctx;
	if(m_tsParam.width != iWidth || m_tsParam.hight != iHight)
	{
		if (m_img_convert_ctx == NULL) 
		{
        	m_img_convert_ctx = sws_getContext(iWidth, iHight,
	        	pix_fmt,
            	m_tsParam.width, m_tsParam.hight,
            	pix_fmt_dst,
            	sws_flags, NULL, NULL, NULL);
            if (m_img_convert_ctx == NULL) {
                fprintf(stderr,
                    "Cannot initialize the conversion context\n");
                    exit(1);
                }
			printf("create sws context \n");
        }
		//printf("srclinesize=%d dstlinesize=%d \n",pAVfram->linesize[0], m_pframe->linesize[0]);
		m_pframe->linesize[0] = m_tsParam.width;
		m_pframe->linesize[1] = m_tsParam.width/2;
		m_pframe->linesize[2] = m_tsParam.width/2;

		int nSrcStride[3];
		nSrcStride[0] = iwidth;
		nSrcStride[1] = iwidth/2;
		nSrcStride[2] = iwidth/2;

	//	printf("srclinesize=%d dstlinesize=%d srcw=%d srchight=%d dstw=%d dsthight=%d \n",pAVfram->linesize[0], 
	//			m_pframe->linesize[0],iwidth,ihight,m_tsParam.width,m_tsParam.hight);
        sws_scale(m_img_convert_ctx, pAVfram->data,pAVfram->linesize,
          0, iHight, m_pframe->data, m_pframe->linesize);
		//printf("line0:%d line1:%d line2:%d\n",m_pframe->linesize[0] ,m_pframe->linesize[1] ,m_pframe->linesize[2]);
		pAVfram = m_pframe;
		iWidth = m_tsParam.width;
		iHight = m_tsParam.hight;
		//printf("sws_scale  \n");
	}
	

		// no B frame
	for (int i = 0; i < iHight; i++)
	{
		memcpy(buff+i*iWidth, pAVfram->data[0]+i*pAVfram->linesize[0], iWidth);
	}
	for (int i=0; i < iHight/2; i++)
	{
		memcpy(buff+iHight*iWidth+i*iWidth/2, pAVfram->data[1]+i*pAVfram->linesize[1], iWidth/2);
	}
	for (int i=0; i < iHight/2; i++)
	{
		memcpy(buff+iHight*iWidth*5/4+i*iWidth/2, pAVfram->data[2]+i*pAVfram->linesize[2], iWidth/2);
	}
/*	
	if(fpYUV == NULL)
	{
		fpYUV = fopen("org.yuv", "w+b");
	}
	fwrite(buff, 1, iWidth*iHight*3/2, fpYUV);
*/

	shmhdr_input_data(m_shm_addr,m_tsParam.width,m_tsParam.hight,m_yuv_data,iWidth*iHight*3/2);

}


void TSDecoder_Instance::Push_Video_Data_shm(int iWidth, int iHight, AVPicture *pAVfram,unsigned long ulPTS)
{
	int iwidth = iWidth;
	int ihight = iHight;

		//scale
//	static struct SwsContext *img_convert_ctx;
	if(m_tsParam.width != iWidth || m_tsParam.hight != iHight)
	{
		if (m_img_convert_ctx == NULL) 
		{
        	m_img_convert_ctx = sws_getContext(iWidth, iHight,
	        	pix_fmt,
            	m_tsParam.width, m_tsParam.hight,
            	pix_fmt_dst,
            	sws_flags, NULL, NULL, NULL);
            if (m_img_convert_ctx == NULL) {
                fprintf(stderr,
                    "Cannot initialize the conversion context\n");
                    exit(1);
                }
			printf("create sws context \n");
        }
		
		int nSrcStride[3];
	//	printf("srclinesize=%d dstlinesize=%d \n",pAVfram->linesize[0], m_pframe->linesize[0]);

		if(pix_fmt_dst == AV_PIX_FMT_YUV420P)
		{
			m_pframe->linesize[0] = m_tsParam.width;
			m_pframe->linesize[1] = m_tsParam.width/2;
			m_pframe->linesize[2] = m_tsParam.width/2;

			nSrcStride[0] = iwidth;
			nSrcStride[1] = iwidth/2;
			nSrcStride[2] = iwidth/2;
		}
		else if(pix_fmt_dst == AV_PIX_FMT_BGRA)
		{
			m_pframe->linesize[0] = m_tsParam.width*4;
			m_pframe->linesize[1] = m_tsParam.width*4;
			m_pframe->linesize[2] = m_tsParam.width*4;

			nSrcStride[0] = iwidth*4;
			nSrcStride[1] = iwidth*4;
			nSrcStride[2] = iwidth*4;

		}
		else
		{
			fprintf(stderr,"decode pic pixformat not right\n");
			return ;
		}

	//	printf("srclinesize=%d dstlinesize=%d srcw=%d srchight=%d dstw=%d dsthight=%d \n",pAVfram->linesize[0], 
	//			m_pframe->linesize[0],iwidth,ihight,m_tsParam.width,m_tsParam.hight);
        sws_scale(m_img_convert_ctx, pAVfram->data,pAVfram->linesize,
          0, iHight, m_pframe->data, m_pframe->linesize);
		//printf("line0:%d line1:%d line2:%d\n",m_pframe->linesize[0] ,m_pframe->linesize[1] ,m_pframe->linesize[2]);
		pAVfram = m_pframe;
		iWidth = m_tsParam.width;
		iHight = m_tsParam.hight;
		//printf("sws_scale  \n");
	}

	VideoData* tempbuff = m_video_buff_data;
	tempbuff->iHeight = iHight;
	tempbuff->iWidth = iWidth;
	tempbuff->pts = ulPTS;

	unsigned char *buff = tempbuff->data;
	if(pix_fmt_dst == AV_PIX_FMT_YUV420P)
	{
		for (int i = 0; i < iHight; i++)
		{
			memcpy(buff+i*iWidth, pAVfram->data[0]+i*pAVfram->linesize[0], iWidth);
		}
		for (int i=0; i < iHight/2; i++)
		{
			memcpy(buff+iHight*iWidth+i*iWidth/2, pAVfram->data[1]+i*pAVfram->linesize[1], iWidth/2);
		}
		for (int i=0; i < iHight/2; i++)
		{
			memcpy(buff+iHight*iWidth*5/4+i*iWidth/2, pAVfram->data[2]+i*pAVfram->linesize[2], iWidth/2);
		}
	}
	else if(pix_fmt_dst == AV_PIX_FMT_BGRA)
	{
		for (int i = 0; i < iHight; i++)
		{
			memcpy(buff+i*iWidth, pAVfram->data[0]+i*pAVfram->linesize[0], pAVfram->linesize[0]);
		}

	}

	shmvideo_put((videosurf*)m_shm_addr,m_tsParam.width,m_tsParam.hight,buff,iHight*iWidth*3/2);
/*		
		if(fpYUV == NULL)
		{
			fpYUV = fopen("before.yuv", "w+b");
		}
		fwrite(buff, 1, iWidth*iHight*3/2, fpYUV);
*/		
		
}


void TSDecoder_Instance::Push_Video_Data(int iWidth, int iHight, AVPicture *pAVfram,unsigned long ulPTS)
{
	int iwidth = iWidth;
	int ihight = iHight;

		//scale
//	static struct SwsContext *img_convert_ctx;
	if(m_tsParam.width != iWidth || m_tsParam.hight != iHight)
	{
		if (m_img_convert_ctx == NULL) 
		{
        	m_img_convert_ctx = sws_getContext(iWidth, iHight,
	        	pix_fmt,
            	m_tsParam.width, m_tsParam.hight,
            	pix_fmt_dst,
            	sws_flags, NULL, NULL, NULL);
            if (m_img_convert_ctx == NULL) {
                fprintf(stderr,
                    "Cannot initialize the conversion context\n");
                    exit(1);
                }
			printf("create sws context \n");
        }
		
		int nSrcStride[3];
	//	printf("srclinesize=%d dstlinesize=%d \n",pAVfram->linesize[0], m_pframe->linesize[0]);

		if(pix_fmt_dst == AV_PIX_FMT_YUV420P)
		{
			m_pframe->linesize[0] = m_tsParam.width;
			m_pframe->linesize[1] = m_tsParam.width/2;
			m_pframe->linesize[2] = m_tsParam.width/2;

			nSrcStride[0] = iwidth;
			nSrcStride[1] = iwidth/2;
			nSrcStride[2] = iwidth/2;
		}
		else if(pix_fmt_dst == AV_PIX_FMT_BGRA)
		{
			m_pframe->linesize[0] = m_tsParam.width*4;
			m_pframe->linesize[1] = m_tsParam.width*4;
			m_pframe->linesize[2] = m_tsParam.width*4;

			nSrcStride[0] = iwidth*4;
			nSrcStride[1] = iwidth*4;
			nSrcStride[2] = iwidth*4;

		}
		else
		{
			fprintf(stderr,"decode pic pixformat not right\n");
			return ;
		}

	//	printf("srclinesize=%d dstlinesize=%d srcw=%d srchight=%d dstw=%d dsthight=%d \n",pAVfram->linesize[0], 
	//			m_pframe->linesize[0],iwidth,ihight,m_tsParam.width,m_tsParam.hight);
        sws_scale(m_img_convert_ctx, pAVfram->data,pAVfram->linesize,
          0, iHight, m_pframe->data, m_pframe->linesize);
		//printf("line0:%d line1:%d line2:%d\n",m_pframe->linesize[0] ,m_pframe->linesize[1] ,m_pframe->linesize[2]);
		pAVfram = m_pframe;
		iWidth = m_tsParam.width;
		iHight = m_tsParam.hight;
		//printf("sws_scale  \n");
	}

	pthread_mutex_lock(&m_locker);
	if(m_freeQueue.size() > 0)
	{
		VideoData* tempbuff = m_freeQueue.front();
		tempbuff->iHeight = iHight;
		tempbuff->iWidth = iWidth;
		tempbuff->pts = ulPTS;

		unsigned char *buff = tempbuff->data;
		if(pix_fmt_dst == AV_PIX_FMT_YUV420P)
		{
			for (int i = 0; i < iHight; i++)
			{
				memcpy(buff+i*iWidth, pAVfram->data[0]+i*pAVfram->linesize[0], iWidth);
			}
			for (int i=0; i < iHight/2; i++)
			{
				memcpy(buff+iHight*iWidth+i*iWidth/2, pAVfram->data[1]+i*pAVfram->linesize[1], iWidth/2);
			}
			for (int i=0; i < iHight/2; i++)
			{
				memcpy(buff+iHight*iWidth*5/4+i*iWidth/2, pAVfram->data[2]+i*pAVfram->linesize[2], iWidth/2);
			}
		}
		else if(pix_fmt_dst == AV_PIX_FMT_BGRA)
		{
			for (int i = 0; i < iHight; i++)
			{
				memcpy(buff+i*iWidth, pAVfram->data[0]+i*pAVfram->linesize[0], pAVfram->linesize[0]);
			}

		}
/*		
		if(fpYUV == NULL)
		{
			fpYUV = fopen("before.yuv", "w+b");
		}
		fwrite(buff, 1, iWidth*iHight*3/2, fpYUV);
*/		
		m_freeQueue.pop_front();
		m_usedQueue.push_back(tempbuff);
	}
	else if(m_usedQueue.size()>0)
	{
		VideoData* tempbuff = m_usedQueue.front();
		tempbuff->iHeight = iHight;
		tempbuff->iWidth = iWidth;
		tempbuff->pts = ulPTS;

		unsigned char *buff = tempbuff->data;
		if(pix_fmt_dst == AV_PIX_FMT_YUV420P)
		{
			for (int i = 0; i < iHight; i++)
			{
				memcpy(buff+i*iWidth, pAVfram->data[0]+i*pAVfram->linesize[0], iWidth);
			}
			for (int i=0; i < iHight/2; i++)
			{
				memcpy(buff+iHight*iWidth+i*iWidth/2, pAVfram->data[1]+i*pAVfram->linesize[1], iWidth/2);
			}
			for (int i=0; i < iHight/2; i++)
			{
				memcpy(buff+iHight*iWidth*5/4+i*iWidth/2, pAVfram->data[2]+i*pAVfram->linesize[2], iWidth/2);
			}
		}
		else if(pix_fmt_dst == AV_PIX_FMT_BGRA)
		{
			for (int i = 0; i < iHight; i++)
			{
				memcpy(buff+i*iWidth, pAVfram->data[0]+i*pAVfram->linesize[0], pAVfram->linesize[0]);
			}

		}

	/*	
		if(fpYUV == NULL)
		{
			fpYUV = fopen("befor.yuv", "w+b");
		}
		fwrite(buff, 1, iWidth*iHight*3/2, fpYUV);
	*/	
		m_usedQueue.pop_front();
		m_usedQueue.push_back(tempbuff);
		fprintf(stderr,"========full used front to back,usedque size=%d \n",m_usedQueue.size());
	}
	pthread_mutex_unlock(&m_locker);
}

int TSDecoder_Instance::get_video_param(int *iwidth,int *iheight)
{
	*iwidth = m_iWidht;
	*iheight = m_iHeight;
}

int TSDecoder_Instance::get_Video_data(unsigned char *output_video_yuv420,int *output_video_size,
	int* iwidth,int* iHeight,unsigned long* video_pts)
{
	//fprintf(stderr,"get video data\n");
/*	if(!m_bDecoderFlag && !m_recvqueue.m_bDelayFrame)
	{
		Clean_Video_audioQue();
		return -1;
	}*/
	pthread_mutex_lock(&m_locker);
/*	if(!m_bDecoderFlag && m_recvqueue.m_bDelayFrame)
	{
		//printf("-----delay frame time 1111\n");
		if(m_usedQueue.size() > 0 )
		{
			m_iDelayFrame++;
			printf("-----delay frame time 2222\n");
			VideoData* tempbuff = m_usedQueue.front();
			
			unsigned char *buff = tempbuff->data;
			*iwidth = tempbuff->iWidth;
			*iHeight = tempbuff->iHeight;
			if(video_pts)
				*video_pts = tempbuff->pts;
			
			int iyuvsize = tempbuff->iWidth * tempbuff->iHeight*3/2;
			if(*output_video_size < iyuvsize)
			{
				fprintf(stderr,"output_video_size is too small \n");
				pthread_mutex_unlock(&m_locker);
				return -1;
			}
			*output_video_size = iyuvsize;
			memcpy(output_video_yuv420,buff,iyuvsize);
			tempbuff->iWidth = 0;
			tempbuff->iHeight = 0;
			m_usedQueue.pop_front();
			m_freeQueue.push_back(tempbuff);
			
			pthread_mutex_unlock(&m_locker);

			struct timeval tm;

			gettimeofday(&tm,NULL);
			printf("-----video que size =%d ,get Video Time =%ld\n",m_usedQueue.size(),tm.tv_sec*1000+tm.tv_usec/1000);
			//printf("pts=%ld,w=%d video queue used size =%d \n",m_ulvideo_pts,*iwidth,m_usedQueue.size());
			return 0;
		}
		else if(m_usedQueue.size() > 0)
		{
			m_iDelayFrame++;
			//m_recvqueue.m_bDelayFrame = false;
			printf("-----clean que begin\n");
			while(m_usedQueue.size() >0)
			{
				VideoData* tempbuff = m_usedQueue.front();
				tempbuff->iWidth = 0;
				tempbuff->iHeight = 0;
				m_usedQueue.pop_front();
				m_freeQueue.push_back(tempbuff);
			
			}
			pthread_mutex_unlock(&m_locker);
			printf("----clean video que \n");

			pthread_mutex_lock(&m_audioLocker);
			while(m_audioUsedQueue.size() > 0)
			{
				AudioData* audiobuff = m_audioUsedQueue.front();
				m_audioUsedQueue.pop_front();
				m_audioFreeQueue.push_back(audiobuff);
			}
			printf("----clean audio que \n");
			pthread_mutex_unlock(&m_audioLocker);
			
			return -1;
		}
		else
		{
			m_iDelayFrame++;
			pthread_mutex_unlock(&m_locker);
			return -1;
		}

	}
*/	
	if(m_usedQueue.size() >0)
	{
		VideoData* tempbuff = m_usedQueue.front();
		
		unsigned char *buff = tempbuff->data;
		*iwidth = tempbuff->iWidth;
		*iHeight = tempbuff->iHeight;
		if(video_pts)
			*video_pts = tempbuff->pts;
		//m_ulvideo_pts = tempbuff->pts; //控制音视频同步
		
		int iyuvsize = tempbuff->iWidth * tempbuff->iHeight*3/2;
		if(pix_fmt_dst==AV_PIX_FMT_BGRA)
			iyuvsize = tempbuff->iWidth * tempbuff->iHeight*4;
		if(*output_video_size < iyuvsize)
		{
			fprintf(stderr,"output_video_size is too small \n");
			pthread_mutex_unlock(&m_locker);
			return -1;
		}
		*output_video_size = iyuvsize;
		memcpy(output_video_yuv420,buff,iyuvsize);
		tempbuff->iWidth = 0;
		tempbuff->iHeight = 0;
		m_usedQueue.pop_front();
		m_freeQueue.push_back(tempbuff);
		
		pthread_mutex_unlock(&m_locker);

		struct timeval tm;

		gettimeofday(&tm,NULL);
		//printf("-----video que size =%d ,get Video Time =%ld  video pts=%ld \n",m_usedQueue.size(),tm.tv_sec*1000+tm.tv_usec/1000,*video_pts);
	}
	else
	{
		pthread_mutex_unlock(&m_locker);
		return -1;
	}
	return 0;
}

void TSDecoder_Instance::Push_Audio_Data_shm(unsigned char *sample,int isize,unsigned long ulPTS)
{	
#if 0
	if(m_audiofd == NULL)
		return ;
			if(fpfifo == NULL)
		{
			fpfifo = fopen("fifo.pcm", "w+b");
		}
		fwrite(sample, 1, isize, fpfifo);
	printf("---write audio len = %d \n",isize);
#endif	

	shmaudio_put((audiofrag*)m_audioshm_addr,sample,isize,ulPTS);
	
}



FILE*fpfifo = NULL;
void TSDecoder_Instance::Push_Audio_Data_new(unsigned char *sample,int isize,unsigned long ulPTS)
{	
#if 0
	if(m_audiofd == NULL)
		return ;
			if(fpfifo == NULL)
		{
			fpfifo = fopen("fifo.pcm", "w+b");
		}
		fwrite(sample, 1, isize, fpfifo);
	printf("---write audio len = %d \n",isize);
#endif	
#if 1	
	int bytes_left;   
    int written_bytes;   
    char *ptr;   
    ptr=(char *)sample;   
    bytes_left=isize;   
    while(bytes_left>0)   
    {   
		int written_bytes = write(m_audiofd,ptr,bytes_left);
		if(written_bytes<=0) /* ???*/   
		{	
			if(errno==EINTR) /* ???? ?????*/	
			{   
				//printf("[SeanSend]error errno==EINTR continue\n");	
				continue; 
			}  
			else if(errno==EAGAIN) /* EAGAIN : Resource temporarily unavailable*/	
			{  
				usleep(1000);//??1ms ,??????????  
				//printf("[SeanSend]error errno==EAGAIN continue\n");  
				continue;  
				//return; 
				
			}  
			else /* ???? ????,????*/   
			{  
				//printf("[SeanSend]ERROR: errno = %d, strerror = %s \n"	
				//				, errno, strerror(errno));	
				return;  
			}  
		} 
		bytes_left-=written_bytes;   
        ptr+=written_bytes;/* ??????????? */  
    }
#endif	
}

FILE *fpaudio=NULL;
void TSDecoder_Instance::Push_Audio_Data(unsigned char *sample,int isize,unsigned long ulPTS)
{
/*		if(fpaudio == NULL)
		{
			fpaudio = fopen("push.pcm", "w+b");
		}
		fwrite(sample, 1, isize, fpaudio);
*/
	pthread_mutex_lock(&m_audioLocker);
	if(m_audioFreeQueue.size() > 0)
	{
		AudioData* audiobuff = m_audioFreeQueue.front();
		audiobuff->pts = ulPTS;
		audiobuff->size = isize;

		unsigned char *tmpbuf = audiobuff->data;
		memcpy(tmpbuf,sample,isize);

		m_audioFreeQueue.pop_front();
		m_audioUsedQueue.push_back(audiobuff);
	}
	else if(m_audioUsedQueue.size() > 0)
	{
		AudioData* audiobuff = m_audioUsedQueue.front();
		audiobuff->pts = ulPTS;
		audiobuff->size = isize;

		unsigned char* tmpbuf = audiobuff->data;
		memcpy(tmpbuf,sample,isize);

		m_audioUsedQueue.pop_front();
		m_audioUsedQueue.push_back(audiobuff);
	}
	
	pthread_mutex_unlock(&m_audioLocker);
}


int TSDecoder_Instance::get_Audio_data(unsigned char *output_audio_data,int* input_audio_size,
	unsigned long* audio_pts)
{
	pthread_mutex_lock(&m_audioLocker);
	if(m_audioUsedQueue.size() >0)
	{

		AudioData* tempbuff = m_audioUsedQueue.front();
		
		unsigned char *buff = tempbuff->data;
	
		if(audio_pts)
			*audio_pts= tempbuff->pts;
		
		if(*input_audio_size < tempbuff->size)
		{
			fprintf(stderr,"input_audio_size is too small \n");
			pthread_mutex_unlock(&m_audioLocker);
			return -1;
		}
		*input_audio_size= tempbuff->size;
		memcpy(output_audio_data,buff,tempbuff->size);
		tempbuff->size = 0;
		
		
		m_audioUsedQueue.pop_front();
		m_audioFreeQueue.push_back(tempbuff);
	
		pthread_mutex_unlock(&m_audioLocker);
				struct timeval tm;

		gettimeofday(&tm,NULL);
		//printf("-----audio que size =%d ,get audio Time =%ld  auido pts=%ld \n",m_usedQueue.size(),tm.tv_sec*1000+tm.tv_usec/1000,*audio_pts);
	}
	else
	{
		pthread_mutex_unlock(&m_audioLocker);
		return -1;
	}
	return 0;
}

void TSDecoder_Instance::stopDecoder(bool bstop)
{
	m_bstop = bstop;
}
#if 0
void av_free_packet(AVPacket *pkt)
{
    if (pkt) {
        if (pkt->destruct)
            pkt->destruct(pkt);
        pkt->data            = NULL;
        pkt->size            = 0;
        pkt->side_data       = NULL;
        pkt->side_data_elems = 0;
    }
}

void av_free(void *ptr)
{

    free(ptr);
}


void av_freep(void *arg)
{
    void **ptr= (void**)arg;
    av_free(*ptr);
    *ptr = NULL;
}

#endif
static void free_packet_buffer(AVPacketList **pkt_buf, AVPacketList **pkt_buf_end)
{
    while (*pkt_buf) {
        AVPacketList *pktl = *pkt_buf;
        *pkt_buf = pktl->next;
        av_free_packet(&pktl->pkt);
        av_freep(&pktl);
    }
    *pkt_buf_end = NULL;
}


bool TSDecoder_Instance::set_tsDecoder_stat(bool bstat)
{
#if 0
	if(!m_bDecoderFlag && bstat){
		Clean_Video_audioQue();
		m_iDelayFrame = 3;
		free_packet_buffer(&m_pFmt->parse_queue,&m_pFmt->parse_queue_end);
		free_packet_buffer(&m_pFmt->packet_buffer,&m_pFmt->packet_buffer_end);

		
		//free_packet_buffer(&m_pFmt->packet_buffer,&m_pFmt->packet_buffer_end);
/*		AVPacketList **pkt_buf = &m_pFmt->packet_buffer;
		AVPacketList **pkt_buf_end = &m_pFmt->packet_buffer_end;
	    while (*pkt_buf) {
	        AVPacketList *pktl = *pkt_buf;
	        *pkt_buf = pktl->next;
			AVPacket *pkt = &pktl->pkt;
		    if (pkt) {
		        if (pkt->destruct)
		            pkt->destruct(pkt);
		        pkt->data            = NULL;
		        pkt->size            = 0;
		        pkt->side_data       = NULL;
		        pkt->side_data_elems = 0;
				 }	
			void *arg = (void*)&pktl;
			void **ptr= (void**)arg;
		    free(*ptr);
		    *ptr = NULL;
	   }
	    *pkt_buf_end = NULL;
*/
	}
	m_recvqueue.set_tsDecoder_stat(bstat);
	//m_bFirstDecodeSuccess = false;
	m_bDecoderFlag = bstat;

	if(m_bDecoderFlag)
	{

		return true;
	}
	else
	{
		return !m_recvqueue.m_bIsOverlaying;
	}
#endif
	return true;
}

int TSDecoder_Instance::init_open_input()
{
#if 0	
	AVCodec *pVideoCodec, *pAudioCodec;
	AVCodecContext *pVideoCodecCtx = NULL;
	AVCodecContext *pAudioCodecCtx = NULL;
	AVIOContext * pb = NULL;
	AVInputFormat *piFmt = NULL;
	AVFormatContext *pFmt = NULL;


	struct timeval tm;

	av_register_all();
	avformat_network_init();

	std::string strtmp=m_cfilename;
	std::size_t found  = strtmp.find(":");
	if (found != std::string::npos)
		m_ifileType = internetfile;
	else
		m_ifileType = localfile;
	printf("-------------file Type=%d\n",m_ifileType);

	if(m_ifileType == localfile || Use_ffmpeg_recv)
	{
		pFmt = avformat_alloc_context();
		if (avformat_open_input(&pFmt, m_cfilename, NULL, NULL) < 0)
		{
			fprintf(stderr, "avformat open failed.\n");
			return -1;
		} 
		else
		{
			fprintf(stdout, "open stream success!\n");
		}
		
	}
	else if(m_ifileType == internetfile)
	{
		//  udp://@:14000
		std::size_t found  = strtmp.find("@:");
		int port = 0;
		if (found != std::string::npos)
		{
			std::string tm = strtmp.substr(found+2,(strtmp.length()-found-2));
			//printf("-------get string sub %s \n",tm.c_str());	
			port = atoi(tm.c_str());
			printf("=============get port =%d \n",port);
			
		}
		m_recvqueue.init_queue(Recv_Queue_size,port,m_strDstIP,m_iPort);

		
		uint8_t *buf = (uint8_t*)av_mallocz(sizeof(uint8_t)*BUF_SIZE);
		
		pb = avio_alloc_context(buf, BUF_SIZE, 0, this, TSDecoder_Instance::read_data, NULL, NULL);
		if (!pb) {
			fprintf(stderr, "avio alloc failed!\n");
			return -1;
		}

		
		//gettimeofday(&tm,NULL);
	//	printf("-----init time 1 =%ld\n",tm.tv_sec*1000+tm.tv_usec/1000);
		int iloop = 10;
		do
		{
			if (av_probe_input_buffer(pb, &piFmt, "", NULL, 0, 0) < 0) {
			fprintf(stderr, "probe failed!\n");
			} else {
				fprintf(stdout, "probe success!\n");
				fprintf(stdout, "format: %s[%s]\n", piFmt->name, piFmt->long_name);
				break;
			}
			usleep(1000);
		}while(iloop-- >=0);


		//gettimeofday(&tm,NULL);
		//printf("-----init time 2 =%ld\n",tm.tv_sec*1000+tm.tv_usec/1000);

		pFmt = avformat_alloc_context();
		pFmt->pb = pb;
		if (avformat_open_input(&pFmt, "", piFmt, NULL) < 0) {
			fprintf(stderr, "avformat open failed.\n");
			return -1;
		} else {
			fprintf(stdout, "open stream success!\n");
		}

		
		//gettimeofday(&tm,NULL);
		//printf("-----init time 3 =%ld\n",tm.tv_sec*1000+tm.tv_usec/1000);
		
	}


	//printf("======max_analyze_duration=%d,probesize=%ld==== \n",pFmt->max_analyze_duration,pFmt->probesize);

	//pFmt->max_analyze_duration  = 1000;
	//pFmt->probesize = 2048;


	if (av_find_stream_info(pFmt) < 0)
	{
		fprintf(stderr, "could not fine stream.\n");
		return -1;
	}

	
	//gettimeofday(&tm,NULL);
	//printf("-----init time 4 =%ld\n",tm.tv_sec*1000+tm.tv_usec/1000);

	
	av_dump_format(pFmt, 0, "", 0);

	int videoindex = -1;
	int audioindex = -1;
	for (int i = 0; i < pFmt->nb_streams; i++) 
	{
		if ( (pFmt->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) &&
				(videoindex < 0) ) {
			videoindex = i;
		}
		if ( (pFmt->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) &&
				(audioindex < 0) ) {
			audioindex = i;
		}
	}


	if (videoindex < 0 || audioindex < 0) {
		fprintf(stderr, "videoindex=%d, audioindex=%d\n", videoindex, audioindex);
		return -1;
	}

	AVStream *pVst,*pAst;
	pVst = pFmt->streams[videoindex];
	pAst = pFmt->streams[audioindex];

	
	pVideoCodecCtx = pVst->codec;
	pAudioCodecCtx = pAst->codec;

	pVideoCodec = avcodec_find_decoder(pVideoCodecCtx->codec_id);
	if (!pVideoCodec) {
		fprintf(stderr, "could not find video decoder!\n");
		return -1;
	}
	if (avcodec_open(pVideoCodecCtx, pVideoCodec) < 0) {
		fprintf(stderr, "could not open video codec!\n");
		return -1;
	}


	pAudioCodec = avcodec_find_decoder(pAudioCodecCtx->codec_id);
	if (!pAudioCodec) {
		fprintf(stderr, "could not find audio decoder!\n");
		return -1;
	}
	if (avcodec_open(pAudioCodecCtx, pAudioCodec) < 0) {
		fprintf(stderr, "could not open audio codec!\n");
		return -1;
	}
	
	m_pVideoCodec = pVideoCodec;
	m_pAudioCodec = pAudioCodec;
	m_pFmt = pFmt;
	m_pVideoCodecCtx = pVideoCodecCtx;
	m_pAudioCodecCtx = pAudioCodecCtx;
	m_pframe = NULL;
	//m_iHeight = iHeight;
	//m_iWidht = iWidht;
	m_videoindex = videoindex;
	m_audioindex = audioindex;

	
	pthread_mutex_init(&m_locker,NULL);
	pthread_mutex_init(&m_audioLocker,NULL);
	
	initQueue(Queue_size);
#endif
	return 0;
}

bool TSDecoder_Instance::Get_tsDecoder_sem(void **pSem)
{
	*pSem = &(m_recvqueue.m_sem_send);
	printf("=====printf add \n");
	printf("----sem add %0x",*pSem);
	return true;
}




int TSDecoder_Instance::uninit_TS_Decoder()
{
	
	//pthread_cancel(p_instanse->read_thread_id);
	//pthread_cancel(p_instanse->write_thread_id);
	//pthread_mutex_destroy(&p_instanse->m_mutex);

	freeQueue();
	
	if (m_pVideoCodecCtx) 
	{
        avcodec_close(m_pVideoCodecCtx);
    }
	if(m_pAudioCodecCtx)
	{
		avcodec_close(m_pAudioCodecCtx);
	}
	//if(p_instanse->m_pFmt)
	//{
	//	avcodec_close(p_instanse->m_pFmt);
	//}
	if (m_img_convert_ctx)  
    {  
        sws_freeContext(m_img_convert_ctx);  
        m_img_convert_ctx = NULL;  
    }  
	av_free(m_pframe);
	if(m_yuv_data)
		free(m_yuv_data);
    /* free the stream */
    av_free(m_pFmt);

	return 0;
}

bool TSDecoder_Instance::Clean_Video_audioQue()
{
	pthread_mutex_lock(&m_locker);
	//m_recvqueue.m_bDelayFrame = false;
	//printf("-----clean video que begin\n");
	while(m_usedQueue.size() >0)
	{
		VideoData* tempbuff = m_usedQueue.front();
		tempbuff->iWidth = 0;
		tempbuff->iHeight = 0;
		m_usedQueue.pop_front();
		m_freeQueue.push_back(tempbuff);

	}
	pthread_mutex_unlock(&m_locker);
	//printf("----clean audio que \n");

	pthread_mutex_lock(&m_audioLocker);
	while(m_audioUsedQueue.size() > 0)
	{
		AudioData* audiobuff = m_audioUsedQueue.front();
		m_audioUsedQueue.pop_front();
		m_audioFreeQueue.push_back(audiobuff);
	}
	pthread_mutex_unlock(&m_audioLocker);

	return true;
}




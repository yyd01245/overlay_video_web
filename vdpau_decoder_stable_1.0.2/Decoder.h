#ifndef __DECODER_H_
#define __DECODER_H_

#include <deque>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <pthread.h>

#ifndef INT64_C
#define INT64_C(c) c##LL
#endif
#ifndef UINT64_C
#define UINT64_C(c) c##LL
#endif


extern "C" {
#include "libavutil/pixfmt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/samplefmt.h"

#include "libavutil/avstring.h"
#include "libavutil/opt.h"

#include "libswresample/swresample.h"



}
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <string>
#include "RecvQueue.h"
#include "av_shm.h"
#include "vdpauContext.h"
#include "avshm.h"


typedef enum enVideoType
{
	CODE_HD_VIDEO = 1,
	CODE_SD_VIDEO
}VideoCodeType;

typedef enum enAuidoCodeType
{
	CODE_AUIDO_MP2=1,
	CODE_AUIDO_MP3,
	CODE_AUDIO_AAC
}AuidoCodeType;


typedef struct _VideoData {
	unsigned char *data;
	long size;
	unsigned long pts;
	int iWidth;
	int iHeight;
} VideoData;

typedef enum
{
	internetfile =1,
	localfile = 2,
	other =3
}TSFILETYPE;


typedef struct _AudioData {
	unsigned char *data;
	long size;
	unsigned long pts;
} AudioData;

typedef struct _TSDecoderParam
{
	char strURL[1024];
	char strAudiofifo[1024];
	int  shm_key; //输出共享内存文件key
	int  width;
	int  hight;
	int  idecoder_id; //decoder core
	int audio_key;

}TSDecoderParam;


class TSDecoder_Instance
{
public:
	TSDecoder_Instance();
	~TSDecoder_Instance();

	int init_shmaddr();
	int init_audioshm();
	int init_audiofifo();
	int init_SwrConverAudio(AVFrame * pAudioDecodeFrame,uint8_t * out_buf);

	int init_Decoder(TSDecoderParam param);
	int initdecoder();
	void Push_Video_Data_shm(int iWidth, int iHight, AVPicture *pAVfram,unsigned long ulPTS);
	void Push_Audio_Data_shm(unsigned char *sample,int isize,unsigned long ulPTS);
	void Push_Audio_Data_new(unsigned char *sample,int isize,unsigned long ulPTS);
	void Push_Video_Data_new(int iWidth, int iHight, AVPicture *pAVfram,unsigned long ulPTS);
	void Push_Video_Data_new(int iwidth, int ihight, char *pInfram);
	int get_Video_data(unsigned char *output_video_yuv420,int *output_video_size,
	int* iwidth,int* iHeight,unsigned long* video_pts=NULL); 

	void Push_Video_Data(int iWidth, int iHight, AVPicture *pAVfram,unsigned long ulPTS);
	
	int get_Audio_data(unsigned char *output_audio_data,int* input_audio_size,
	unsigned long* audio_pts);

	void Push_Audio_Data(unsigned char *sample,int isize,unsigned long ulPTS);

	int uninit_TS_Decoder();
	static void* Audio_output_data_threadFun(void* param);

	static void* output_data_threadFun(void* param);
	static void* decoder_threadFun(void *param);
	static void* get_data_threadFun(void *param);

	void stopDecoder(bool bstop);

	bool initQueue(int isize);

	bool freeQueue();

	int get_video_param(int *iwidth,int *iheight);


	bool set_tsDecoder_stat(bool bstat);
	
	static int read_data(void *opaque, uint8_t *buf, int buf_size);

	int get_queue(uint8_t* buf, int size);

	bool open_inputdata();

	int init_open_input();

	bool Clean_Video_audioQue();
	bool Get_tsDecoder_sem(void **pSem);

	TSDecoderParam m_tsParam;

	
	pthread_t m_iThreadid;
	pthread_t m_iGetdataThreadid;
private:
	AVCodec *m_pVideoCodec;
	AVCodec *m_pAudioCodec;
	AVCodecContext *m_pVideoCodecCtx;
	AVCodecContext *m_pAudioCodecCtx;
	AVIOContext * m_pb;
	//AVInputFormat *m_piFmt;
	AVFormatContext *m_pFmt;
	//AVFrame *m_pframe;
	AVPicture *m_pframe;
	int m_iWidht;
	int m_iHeight;
	int m_videoindex;
	int m_audioindex;
	std::deque<VideoData*> m_usedQueue;
	std::deque<VideoData*> m_freeQueue;	
	pthread_mutex_t m_locker;

	std::deque<AudioData*> m_audioUsedQueue;
	std::deque<AudioData*> m_audioFreeQueue;
	pthread_mutex_t m_audioLocker;

	unsigned long m_ulvideo_pts;

	VideoData* m_video_buff_data;

	bool m_bstop;
	TSFILETYPE m_ifileType;//url 1 or file 2
	NewQueue m_recvqueue;
	char m_cfilename[1024];

	char m_strDstIP[256];
	int m_iPort;

	VideoCodeType m_vCodetype;
	AuidoCodeType m_aCodeType;
	bool m_bDecoderFlag;//true decoder,false send
	bool m_bFirstDecodeSuccess; //I frame decode
	bool m_bGetData ; //标识获取到yuv数据

	int m_audiofd; //audiofifo
	int m_shm_id;
	int m_audioshm_id;
	char* m_yuv_data;
	void *m_shm_addr;
	void *m_audioshm_addr;
	unsigned int m_shm_size;
	unsigned int m_audioshm_size;
	int m_iDelayFrame;

	int m_fdlog;
	unsigned long m_ulTimebase_video;
	unsigned long m_ulTimebase;
	bool m_bBeginPush;
	
	struct SwsContext *m_img_convert_ctx;  
	//AVAudioResampleContext *m_audio_resample;
	struct SwrContext* m_swr_ctx;
	//uint8_t **m_dst_data;
};


#endif

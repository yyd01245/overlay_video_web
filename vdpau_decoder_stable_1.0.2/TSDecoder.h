#ifndef __TSDECODER_H__
#define __TSDECODER_H__


typedef void TSDecoder_t;

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


//<summary>
//<param cfilename> �ļ���
//<param iWidht>  ָ�����YUV��widht
//<param iHight> ָ�����YUV��height
TSDecoder_t* init_TS_Decoder(const char* cfilename,VideoCodeType vCodetype=CODE_HD_VIDEO,AuidoCodeType aCodeType=CODE_AUIDO_MP2,
									const char* strDstIP="192.168.60.246",int iPort=10000);

//<summary>
//<param ts_decoder>  instance
//<param pWidth pHeight> OUT ���YUV�Ŀ�͸�
int get_Video_Param(TSDecoder_t *ts_decoder,int *pWidth,int *pHeight); 


//<summary>
//<param ts_decoder>  instance
//<param output_video_yuv420>  OUT ���YUV420����
//<param output_video_size> OUT IN ����output_video_yuv420��������С�� ���output_video_yuv420ʵ�ʳ���
//<param pWidth pHeight> OUT ���YUV�Ŀ�͸�
//<param video_pts> OUT video PTS

int get_Video_data(TSDecoder_t *ts_decoder,unsigned char *output_video_yuv420,int *output_video_size,
	int *pWidth,int *pHeight,unsigned long *video_pts=0); 

//<summary>
//<param ts_decoder>  instance
//<param output_audio_data>  OUT �����Ƶ����
//<param output_audio_size> OUT IN ����output_video_data��������С�� ���output_audio_dataʵ�ʳ���
//<param audio_pts> OUT �����Ƶ���ݵ�PTS

int get_Audio_data(TSDecoder_t *ts_decoder,unsigned char *output_audio_data,int* input_audio_size,
	unsigned long* audio_pts);

//<summary> ���ý��뿪�أ�trueΪ�������룬falseΪ�رս��룬ֱ��͸����
bool Set_tsDecoder_stat(TSDecoder_t *ts_decoder,bool bStart);

bool Get_tsDecoder_sem(TSDecoder_t *ts_decoder,void **pSem);

int uninit_TS_Decoder(TSDecoder_t *audioencoder);

#endif


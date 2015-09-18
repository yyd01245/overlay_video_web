#include "TSDecoder.h"
#include "Decoder.h"

TSDecoder_t* init_TS_Decoder(const char* cfilename,VideoCodeType vCodetype,AuidoCodeType aCodeType,const char* strDstIP,int iPort)
{
	TSDecoder_Instance *p_instanse = new TSDecoder_Instance();
	//printf("init decode type video=%d audio=%d \n",vCodetype,aCodeType);
	if(p_instanse->init_TS_Decoder(cfilename,vCodetype,aCodeType,strDstIP,iPort) == -1)
	{
		//init failed
		fprintf(stderr,"init_TS_DecoderFailed \n");
		return NULL;
	}

	return p_instanse;
}

//<summary>
//<param ts_decoder>  instance
//<param pWidth pHeight> OUT 输出YUV的宽和高
int get_Video_Param(TSDecoder_t *ts_decoder,int *pWidth,int *pHeight)
{
	TSDecoder_Instance* p_instance = (TSDecoder_Instance*)ts_decoder;
	int ret = p_instance->get_video_param(pWidth,pHeight);
	return ret;
	
}


//<summary>
//<param ts_decoder>  instance
//<param output_video_yuv420>  OUT 输出YUV420数据
//<param output_video_size> OUT IN 输入output_video_yuv420的容量大小， 输出output_video_yuv420实际长度
//

int get_Video_data(TSDecoder_t *ts_decoder,unsigned char *output_video_yuv420,int *output_video_size,
	int *pWidth,int *pHeight,unsigned long *video_pts)
{
	TSDecoder_Instance* p_instance = (TSDecoder_Instance*)ts_decoder;
	int ret = p_instance->get_Video_data(output_video_yuv420,output_video_size, pWidth,pHeight,video_pts);
		
	return ret;
}

//<summary>
//<param ts_decoder>  instance
//<param output_audio_data>  OUT 输出音频数据
//<param output_audio_size> OUT IN 输入output_video_data的容量大小， 输出output_audio_data实际长度
//<param audio_pts> OUT 输出音频数据的PTS

int get_Audio_data(TSDecoder_t *ts_decoder,unsigned char *output_audio_data,int* input_audio_size,
	unsigned long* audio_pts)
{
	TSDecoder_Instance* p_instance = (TSDecoder_Instance*)ts_decoder;
	int ret = p_instance->get_Audio_data(output_audio_data,input_audio_size,audio_pts);
		
	return ret;

}

bool Set_tsDecoder_stat(TSDecoder_t *ts_decoder,bool bStart)
{
	TSDecoder_Instance* p_instance = (TSDecoder_Instance*)ts_decoder;
	return p_instance->set_tsDecoder_stat(bStart);
		
}

bool Get_tsDecoder_sem(TSDecoder_t *ts_decoder,void **pSem)
{
	TSDecoder_Instance* p_instance = (TSDecoder_Instance*)ts_decoder;
	if(NULL == p_instance) return false;
	return p_instance->Get_tsDecoder_sem(pSem);
}


int uninit_TS_Decoder(TSDecoder_t *tsdecoder)
{
	if(tsdecoder == NULL)
	{		 
		fprintf(stderr ,"libtssmooth: Error paraments..\n");		
		return -1;	  
	}

	TSDecoder_Instance* p_instance = (TSDecoder_Instance* )tsdecoder;
	delete p_instance;
	p_instance = NULL;
	return 0;
}






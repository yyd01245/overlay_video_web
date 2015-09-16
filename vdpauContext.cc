#include"vdpauContext.h"

extern"C"{
#include"win_x11.h"
}

#include <unistd.h>
#include <sys/time.h>

static uint8_t _y[2048*1024];
static uint8_t _u[2048*1024/4];
static uint8_t _v[2048*1024/4];

#define YUV_FMT 1


#define USE_FAKE_YUV_DEGUB 0

//yv12格式
#define VDPAU_FMT_YV12 VDP_YCBCR_FORMAT_YV12
#define VDPAU_FMT_420 VDP_CHROMA_TYPE_420

//bgra 格式
//#define VDPAU_FMT_YV12 VDP_RGBA_FORMAT_R8G8B8A8
//#define VDPAU_FMT_420 VDP_CHROMA_TYPE_420




VDPAUContext::VDPAUContext(int idisplay):
    _device(VDP_INVALID_HANDLE),
    _decoder(VDP_INVALID_HANDLE),
    _mixer(VDP_INVALID_HANDLE){



	char strdisplay[12]={0};
	sprintf(strdisplay,":%d",idisplay);
	printf("init display %s \n",strdisplay);
//    win_x11_init_x11();
    win_x11_init_x11_at(strdisplay);
    win_x11_init_vdpau_procs();

    _display=x_display;
    _device=vdp_device;
    _size.set(0,0);

	m_bInitque = true;
	m_bSecInitQue = true;
    _renderStates.clear();
	m_bNeedRecorve = true;
	pthread_mutex_init(&m_locker,NULL);

}

bool VDPAUContext::initQueue(int isize)
{
		
	printf("init render queue size=%d \n",isize);


	for(int i=0;i<isize;++i)
	{
		  vdpau_render_state*s;
		  s=new vdpau_render_state;
		  memset(s,0,sizeof *s);
		  s->surface=VDP_INVALID_HANDLE;  //VDP_CHROMA_TYPE_420
		  fprintf(stderr,"create surface ++++,i=%d,w=%d,h=%d\n ",i,_size.get_width(),_size.get_height());
		  VdpStatus status;
		  status=vdp_video_surface_create(_device,VDPAU_FMT_420,
				  _size.get_width(),_size.get_height(),&s->surface);
		  _CHK_VDPAU(status,"vdp_video_surface_create() failed");
		  _renderStates.push_back(s);

	}
	
	
	return true;
}



VDPAUContext::~VDPAUContext(){


    if(_mixer!=VDP_INVALID_HANDLE)
        vdp_video_mixer_destroy(_mixer);

    if(_decoder!=VDP_INVALID_HANDLE)
        vdp_decoder_destroy(_decoder);

    for(int i=0;i<_renderStates.size();++i){
        vdp_video_surface_destroy(_renderStates[i]->surface);
        delete _renderStates[i];
    }
    _renderStates.clear();

	for(int i=0;i<m_usedQueue.size();++i){
        vdp_video_surface_destroy(m_usedQueue[i]->surface);
        delete m_usedQueue[i];
    }
    
	m_usedQueue.clear();

}




vdpau_render_state* VDPAUContext::obtain_free_render_state(){

    vdpau_render_state*s;

  //  printf("obtain render\n");
	pthread_mutex_lock(&m_locker);
    if(_renderStates.size()>0){

        s=_renderStates.front();
		_renderStates.pop_front();
		//m_usedQueue.push_back(s);
       // printf("obtain free render from freeque size=%d \n",_renderStates.size());
	    pthread_mutex_unlock(&m_locker);
        return s;
    }
	else
	{
		if(m_bSecInitQue)
		{
			initQueue(SURFACE_QUE_SIZE/2);
			m_bSecInitQue = false;
			s=_renderStates.front();
			_renderStates.pop_front();
			//m_usedQueue.push_back(s);
	       // printf("obtain free render from freeque size=%d \n",_renderStates.size());
		    pthread_mutex_unlock(&m_locker);
	        return s;

		}
	
		s= m_usedQueue.front();
		m_usedQueue.pop_front();
		//printf("obtain free render from useque size=%d \n",_renderStates.size());
		pthread_mutex_unlock(&m_locker);
		return s;
	}
	pthread_mutex_unlock(&m_locker);
    //need more vdpau_render_state;;
#if 0    
    s=new vdpau_render_state;
    memset(s,0,sizeof *s);
    s->surface=VDP_INVALID_HANDLE;  //VDP_CHROMA_TYPE_420
  //  VdpVideoSurface *surface;
  // 	surface = (VdpVideoSurface*)malloc(sizeof(*surface));
 	printf("create surface ++++\n ");
	VdpStatus status;
    status=vdp_video_surface_create(_device,VDPAU_FMT_420,
            _size.get_width(),_size.get_height(),&s->surface);
    _CHK_VDPAU(status,"vdp_video_surface_create() failed");
//	s->surface = *surface;
    //_renderStates.push_back(s);

	//_renderStates
    return s;
#endif

}



int VDPAUContext:: get_buffer(AVCodecContext*pctx,AVFrame*frame)
{
   // fprintf(stderr,"get_buffer()\n");
    VDPAUContext*pvc=(VDPAUContext*)pctx->opaque;

	//printf("code w=%d h=%d \n",pctx->width,pctx->height);
	if(pvc->_size.get_width()==0 || pvc->_size.get_height()==0)
		pvc->_size.set(pctx->width,pctx->height);
	if(pvc->m_bInitque)
	{
	    pvc->initQueue(SURFACE_QUE_SIZE);
		pvc->m_bInitque = false;
	}
    vdpau_render_state*s=pvc->obtain_free_render_state();

    frame->data[0]=(uint8_t*)s;
    frame->type=FF_BUFFER_TYPE_USER; 

    s->state|=FF_VDPAU_STATE_USED_FOR_REFERENCE;

    return 0;
}



//does not really release vdpau_render_state until transfer data to application
void VDPAUContext:: release_buffer(AVCodecContext*pctx,AVFrame*frame)
{
   // fprintf(stderr,"release_buffer()\n");

    VDPAUContext*pvc=(VDPAUContext*)pctx->opaque;
	if(pvc->m_bNeedRecorve)
		pvc->Push_render2freeque(frame);
    frame->data[0]=(uint8_t*)0;
//    frame->type=FF_BUFFER_TYPE_USER; 
//    s->state&=~FF_VDPAU_STATE_USED_FOR_REFERENCE;

    return ;
}


void VDPAUContext:: draw_horiz_band(AVCodecContext*pctx,const AVFrame*frame,int offset[4],int y,int type,int height)
{
    //fprintf(stderr,"draw_horiz_band(type:%d)\n",type);

    VDPAUContext*pvc=(VDPAUContext*)pctx->opaque;
    vdpau_render_state*s=(vdpau_render_state*)frame->data[0];

    if(pvc->get_decoder() == VDP_INVALID_HANDLE){
        pvc->make_decoder(pctx);
    }

    VdpStatus status=vdp_decoder_render(pvc->get_decoder(),s->surface,(VdpPictureInfo const*)&s->info,
              s->bitstream_buffers_used,s->bitstream_buffers);

    _CHK_VDPAU(status,"vdp_decoder_render() failed");
/*
	s->state = frame->pict_type;
	//push in use que
	pthread_mutex_lock(&pvc->m_locker);
	pvc->m_usedQueue.push_back(s);
	printf("---usedQue size=%d \n",pvc->m_usedQueue.size());
	pthread_mutex_unlock(&pvc->m_locker);
	*/
}

int VDPAUContext::Push_render2que(AVFrame* frame)
{

		vdpau_render_state*rs=(vdpau_render_state*)frame->data[0];
		//fprintf(stderr,"Frame <%d> Got..\n",i);
		//push in use que
		pthread_mutex_lock(&m_locker);
		m_usedQueue.push_back(rs);
		//printf("---pictype %d ,usedQue size=%d \n",frame->pict_type,m_usedQueue.size());
		pthread_mutex_unlock(&m_locker);


		return 0;
}
int VDPAUContext::Push_render2freeque(AVFrame* frame)
{

		vdpau_render_state*rs=(vdpau_render_state*)frame->data[0];
		//fprintf(stderr,"Frame <%d> Got..\n",i);
		//push in use que
		pthread_mutex_lock(&m_locker);
		_renderStates.push_back(rs);
		//printf("---push to freeQue size=%d \n",_renderStates.size());
		pthread_mutex_unlock(&m_locker);


		return 0;
}


/*
void* VDPAUContext::get_surface_thread(void *param)
{
	VDPAUContext* this0 = (VDPAUContext*)param;

	while(1)
	{
		usleep(40*1000);

	}

	return NULL;
}
*/
int VDPAUContext::get_surface_data(AVPicture* pic)
{
	int iret = -1;
	pthread_mutex_lock(&m_locker);
	if(m_usedQueue.size() > 0)
	{
		vdpau_render_state * pRenderState = m_usedQueue.front();

	  //  printf(" get surface data from que size =%d \n",m_usedQueue.size());
		
		fetch_yuv(pRenderState,pic);
		m_usedQueue.pop_front();
		_renderStates.push_back(pRenderState);
		iret = 0;
	}
		
	pthread_mutex_unlock(&m_locker);
	
	return iret;
}

AVPixelFormat VDPAUContext::get_format(AVCodecContext* pContext, const AVPixelFormat* pFmt)
{                            
                             
    //fprintf(stderr,"get_format()\n");
                             
                             
    switch (pContext->codec_id) {
        case AV_CODEC_ID_H264:
            return PIX_FMT_VDPAU_H264;
        case AV_CODEC_ID_MPEG1VIDEO:
            return PIX_FMT_VDPAU_MPEG1;
        case AV_CODEC_ID_MPEG2VIDEO:
            return PIX_FMT_VDPAU_MPEG2;
        case AV_CODEC_ID_WMV3:
            return PIX_FMT_VDPAU_WMV3;
        case AV_CODEC_ID_VC1:
            return PIX_FMT_VDPAU_VC1;
        default:             
            return pFmt[0];  
    }                        
}         




void VDPAUContext::make_decoder(AVCodecContext*pctx){
    
    VdpStatus status;

    VdpDecoderProfile profile = 0;

    switch(pctx->pix_fmt){

        case PIX_FMT_VDPAU_MPEG1:
            profile=VDP_DECODER_PROFILE_MPEG1;
            break;

        case PIX_FMT_VDPAU_MPEG2:
            profile=VDP_DECODER_PROFILE_MPEG2_MAIN;
            break;

        case PIX_FMT_VDPAU_H264:
            profile=VDP_DECODER_PROFILE_H264_HIGH;
            break;

        default:
            exit(-1);
    }

    status=vdp_decoder_create(_device,profile,_size.get_width(),_size.get_height(),16,&_decoder);
    _CHK_VDPAU(status,"vdp_decoder_create() Failed");

    _pixfmt=pctx->pix_fmt;

    VdpVideoMixerFeature features[] = { 
        VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL,
        VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL,
    };  
    VdpVideoMixerParameter params[] = { 
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE,
        VDP_VIDEO_MIXER_PARAMETER_LAYERS
    };  
    VdpChromaType chroma = VDPAU_FMT_420;//VDP_CHROMA_TYPE_420;
    int  numLayers = 0;
    int  w=_size.get_width();
    int  h=_size.get_height();
    void const* paramValues [] = { &w, &h, &chroma, &numLayers  };
                        
    status = vdp_video_mixer_create(_device, 2, features, 4, params, 
    paramValues, &_mixer);
    _CHK_VDPAU(status,"vdp_video_mixer_create() Failed");





}

static unsigned char* fake_buff[1280]={0};
int fill_fake_yuv(AVPicture* pAVfram)
{
	//printf("---fill fake yuv \n");
	memset(&fake_buff,55,1280);
	int iWidth=1280;
	int iHight = 720;
	for (int i = 0; i < iHight; i++)
	{
		memcpy(pAVfram->data[0]+i*pAVfram->linesize[0],fake_buff,  iWidth);
	}
	for (int i=0; i < iHight/2; i++)
	{
		memcpy(pAVfram->data[1]+i*pAVfram->linesize[1],fake_buff,iWidth/2);
	}
	for (int i=0; i < iHight/2; i++)
	{
		memcpy(pAVfram->data[2]+i*pAVfram->linesize[2],fake_buff, iWidth/2);
	}


}


int VDPAUContext::Fill_yuv_pic(AVFrame* frame,AVPicture* pic)
{

	vdpau_render_state*rs=(vdpau_render_state*)frame->data[0];
	//fprintf(stderr,"Frame <%d> Got..\n",i);
	
	//fetch_yuv(rs,pic);
#if USE_FAKE_YUV_DEGUB
	fill_fake_yuv(pic);
#else
//	struct timeval tv1;
//	long start_time,now_time;

//	gettimeofday(&tv1,NULL);
//	start_time = tv1.tv_sec*1000*1000+tv1.tv_usec;
	fetch_yuv(rs,pic);
//	gettimeofday(&tv1,NULL);
//	now_time = tv1.tv_sec*1000*1000+tv1.tv_usec;
//	printf("fetch yuv time =%d us \n",now_time-start_time);
	
#endif
//	if(i<200)
//	write_avpicture(&p,fd);
	
	release_vdpau_render_state(rs);

}


int VDPAUContext::Fill_yuv_pic(AVFrame* frame,AVFrame* pic)
{
		vdpau_render_state*rs=(vdpau_render_state*)frame->data[0];
		//fprintf(stderr,"Frame <%d> Got..\n",i);
		
		fetch_yuv(rs,pic);
		
	//	if(i<200)
	//	write_avpicture(&p,fd);
		
		release_vdpau_render_state(rs);


}


AVCodec* VDPAUContext::open_vdpau_codec(AVCodecContext*pctx){
    
    AVCodec*pcodec=NULL;

    if(!ok()){
        fprintf(stderr,"not ok()\n");
        return NULL;
    }
    

    switch(pctx->codec_id){

        case AV_CODEC_ID_H264:
            pcodec=avcodec_find_decoder_by_name("h264_vdpau");
            break;

        case AV_CODEC_ID_MPEG2VIDEO:
            pcodec=avcodec_find_decoder_by_name("mpegvideo_vdpau");
            break;

        default:
            pcodec=NULL;

    }


    if(pcodec){
        

        pctx->get_buffer=VDPAUContext::get_buffer;
        pctx->release_buffer=VDPAUContext::release_buffer;
        pctx->draw_horiz_band=VDPAUContext::draw_horiz_band;
        pctx->get_format=VDPAUContext::get_format;

        pctx->slice_flags=SLICE_FLAG_CODED_ORDER|SLICE_FLAG_ALLOW_FIELD;
		printf("video codec w=%d h=%d \n",pctx->width,pctx->height);
        _size.set(pctx->width,pctx->height);

    }
	printf("set  vdpau over\n");


    return pcodec;
}


int VDPAUContext::fetch_yuv(vdpau_render_state*pRenderState,AVFrame*pic)
{
    int w=_size.get_width();
    int h=_size.get_height();




    VdpStatus status;
#if 0
    void*dest[3]={_y,_u,_v};
    uint32_t pitches[3]={w,w/2,w/2};

	status = vdp_video_surface_get_bits_y_cb_cr(pRenderState->surface,
            VDP_YCBCR_FORMAT_YV12, dest, pitches);
    _CHK_VDPAU(status ,"vdp_video_surface_get_bits_y_cb_cr() failed");

    pic->data[0]=_y;
    pic->data[1]=_v;
    pic->data[2]=_u; //
    pic->linesize[0]=w;
    pic->linesize[1]=w/2;
    pic->linesize[2]=w/2;

    return h*w*3/2;
#endif

	
		  int ret, chroma_type;
		
		  status = vdp_video_surface_get_parameters(pRenderState->surface, (VdpChromaType*)&chroma_type,
												(uint32_t*)&pic->width,
												(uint32_t*)&pic->height);
		_CHK_VDPAU(status ,"vdp_decoder_get_parameters() failed");
		//printf("pic w =%d,h=%d \n",pic->width,pic->height);

		status = vdp_output_surface_get_bits_native(pRenderState->surface,NULL,
		reinterpret_cast<void * const *>(pic->data),
		reinterpret_cast<const uint32_t *>(pic->linesize));
		_CHK_VDPAU(status ,"vdp_video_surface_get_bits_y_cb_cr() failed");

}



int VDPAUContext::fetch_yuv(vdpau_render_state*pRenderState,AVPicture*pic)
{
    int w=_size.get_width();
    int h=_size.get_height();



    VdpStatus status;
#if YUV_FMT
    void*dest[3]={_y,_u,_v};
    uint32_t pitches[3]={w,w/2,w/2};

	status = vdp_video_surface_get_bits_y_cb_cr(pRenderState->surface,
            VDPAU_FMT_YV12, dest, pitches);
    _CHK_VDPAU(status ,"vdp_video_surface_get_bits_y_cb_cr() failed");

    pic->data[0]=_y;
    pic->data[1]=_v;
    pic->data[2]=_u; //yv12 to i420
    pic->linesize[0]=w;
    pic->linesize[1]=w/2;
    pic->linesize[2]=w/2;

    return h*w*3/2;
#else
    VdpOutputSurface surface;
    vdp_output_surface_create(_device, VDP_RGBA_FORMAT_B8G8R8A8, w, h, &surface);
    VdpVideoSurface videoSurface = pRenderState->surface;
	_CHK_VDPAU(status ,"vdp_output_surface_create() failed");
    status = vdp_video_mixer_render(_mixer,
        VDP_INVALID_HANDLE,
       	NULL,
        VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME,
        0, NULL,
        videoSurface,
        0, NULL,
        NULL,
        surface,
        NULL, NULL, 0, NULL);
	_CHK_VDPAU(status ,"vdp_video_mixer_render() failed");	
		status = vdp_output_surface_get_bits_native(surface,NULL,
		reinterpret_cast<void * const *>(pic->data),
		reinterpret_cast<const uint32_t *>(pic->linesize));
		
		vdp_output_surface_destroy(surface);
		_CHK_VDPAU(status ,"vdp_output_surface_get_bits_native() failed");
		
#endif		

}








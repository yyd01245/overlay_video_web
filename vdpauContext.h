#ifndef _HH_VDPAU_CONTEXT_
#define _HH_VDPAU_CONTEXT_



#include<vdpau/vdpau.h>
#include"win_x11.h"

extern "C"{


#include<libavcodec/vdpau.h>

}

#include<vector>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <pthread.h>

#include<stdio.h>
#include<deque>

#define _CHK_VDPAU(s,msg,arg...) \
    if(s!=VDP_STATUS_OK){\
        fprintf(stderr,"VDPAU Failed:: ");\
        fprintf(stderr,msg,##arg);\
        fprintf(stderr,"\n");\
        exit(-1);\
    }


#define SURFACE_QUE_SIZE 30

class Rect{

    public:
        Rect(int w=0,int h=0){
            this->width=w;
            this->height=h;
        }
        void set(int w,int h){
            this->width=w;
            this->height=h;
        }

        int get_width(){
            return width;
        }
        int get_height(){
            return height;
        }

    private:
        int width;
        int height;

};



class VDPAUContext{
  


    public:
        VDPAUContext(int idisplay=0);
        ~VDPAUContext();

        AVCodec*open_vdpau_codec(AVCodecContext*pcodec);

        VdpDecoder get_decoder(){
            return  _decoder;
        }

        void make_decoder(AVCodecContext*pctx);

        int fetch_yuv(vdpau_render_state*pRenderState,AVPicture*pic);
		int Fill_yuv_pic(AVFrame* frame,AVPicture* pic);
		
		int fetch_yuv(vdpau_render_state*pRenderState,AVFrame*pic);
		int Fill_yuv_pic(AVFrame* frame,AVFrame* pic);
		int Push_render2que(AVFrame* frame);
		int Push_render2freeque(AVFrame* frame);

	  	bool initQueue(int isize);

  //  protected:
        bool ok(){
            return _device!=VDP_INVALID_HANDLE;
        }
        vdpau_render_state* obtain_free_render_state();

        static int get_buffer(AVCodecContext*pctx,AVFrame*frame);
        static void release_buffer(AVCodecContext*pctx,AVFrame*frame);
        static void draw_horiz_band(AVCodecContext*pctx,const AVFrame*frame,int offset[4],int y,int type,int height);
        static AVPixelFormat get_format(AVCodecContext* pContext, const AVPixelFormat* pFmt);

	//	static void* get_surface_thread(void *param);
		int get_surface_data(AVPicture* pic);
			bool m_bInitque; //need init queue

			bool m_bSecInitQue;//second queue;
			
		std::deque<vdpau_render_state*> m_usedQueue;
		std::deque<vdpau_render_state*> _renderStates;	 //free
		pthread_mutex_t m_locker;
		bool m_bNeedRecorve; //������Դ
    private:
        Display* _display;
        VdpDevice _device;
        VdpDecoder _decoder;
        VdpVideoMixer _mixer;

        AVPixelFormat _pixfmt;
        Rect _size;

       // std::vector<vdpau_render_state*> _renderStates;



};






static void release_vdpau_render_state(vdpau_render_state* pRenderState)
{
    pRenderState->state &= ~FF_VDPAU_STATE_USED_FOR_REFERENCE;
}














#endif


#include<vdpau/vdpau.h>
extern "C"{

#include<libavformat/avformat.h>
#include<libavcodec/avcodec.h>
#include<libavcodec/vdpau.h>

}

//#include"VDPAUDecoder.h"
//#include"VDPAUHelper.h"

#include"vdpauContext.h"

#if 0
#define CA \
    fprintf(stderr,"%s called\n",__FUNCTION__);

vdpau_render_state* renderstates[16];



void init_vdpau()
{


    for(int i=0;i<16;i++){

        vdpau_render_state*prs=new vdpau_render_state;
        memset(prs,0,sizeof *prs);
        prs->surface=VDP_INVALID_HANDLE;

        VdpStatus status=vdp_video_surface_create(vdp_device,VDP_CHROMA_TYPE_420,1920,1080,&prs->surface);
        ASSERT(status==VDP_STATUS_OK)

    }


}







int get_buffer(AVCodecContext*ctx,AVFrame*f){

    CA
    

    int i;
    for(i=0;i<16;i++){
        if(renderstates[i]->state & FF_VDPAU_STATE_USED_FOR_REFERENCE){
            break;            
        }
    }

    vdpau_render_state*prs=renderstates[i];
    f->data[0]=(uint8_t*)prs;
    f->type=FF_BUFFER_TYPE_USER;
    prs->state|=FF_VDPAU_STATE_USED_FOR_REFERENCE;

    return 0;
}

#endif



#include<unistd.h>
#include<fcntl.h>


int write_avpicture(AVPicture*pic,int fd){

    //packed
    
    for(int i=0;i<720;i++){
        write(fd,pic->data[0]+i*1280,1280);
    }

    for(int i=0;i<720/2;i++){
        write(fd,pic->data[1]+i*1280/2,1280/2);
    }
    for(int i=0;i<720/2;i++){
        write(fd,pic->data[2]+i*1280/2,1280/2);
    }

}


#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>


//avg::VDPAUDecoder vd;
//

VDPAUContext vc;

AVFormatContext *fmtctx;

AVCodecContext* decctx;
AVCodec*dec;
AVStream*st;
int vst;
int main(int argc,char**argv)
{

//    win_x11_init_vdpau_procs();


    int fd=creat("out.yuv",0666);
    if(fd<0){
        fprintf(stderr,"Create yuv file Failed..\n");
        return 0;
    }

    int ret;
    av_register_all();
    avformat_network_init();

//    fmtctx=NULL;


    ret=avformat_open_input(&fmtctx,argv[1],NULL,NULL);
    if(ret<0){
        fprintf(stderr,"avformat_open_input() failed..\n");
        return -1;
    }



    ret=avformat_find_stream_info(fmtctx,NULL);
    if(ret<0){
        fprintf(stderr,"avformat_find_stream_info() failed..\n");
        return -1;
    }



    av_dump_format(fmtctx,0,argv[1],0);



    ret=av_find_best_stream(fmtctx,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);
    if(ret<0){
        fprintf(stderr,"av_find_best_stream() failed..\n");
        return -3;
    }
    vst=ret;
    
    st=fmtctx->streams[ret];
    decctx=st->codec;
#if 0
/*    dec=avcodec_find_decoder_by_name("h264_vdpau");
    if(!dec){
        fprintf(stderr,"avcodec_find_decoder_by_name() failed..\n");
        return -4;
    }

*/
    decctx->opaque=&vd;
    dec=vd.openCodec(decctx);
#else

    decctx->opaque=&vc;
    dec=vc.open_vdpau_codec(decctx);

#endif

    ret=avcodec_open2(decctx,dec,NULL);
    if(ret<0){
        fprintf(stderr,"avcodec_open2() failed..\n");
        return -5;
    }

//    vc.make_decoder(decctx);

    AVPacket pkt;

    av_init_packet(&pkt);


    AVPicture p;

    AVFrame*frame=NULL;
    int got=0;
    int i=0;

    frame=av_frame_alloc();

    while(av_read_frame(fmtctx,&pkt)>=0){



        if(pkt.stream_index!=vst){
            continue; 
        }
    

        ret=avcodec_decode_video2(decctx,frame,&got,&pkt);
        if(ret<0){
            fprintf(stderr,"avcodec_decode() failed..\n");
            return -5; 
        }
        if(got){
            vdpau_render_state*rs=(vdpau_render_state*)frame->data[0];
            fprintf(stderr,"Frame <%d> Got..\n",i);
            i++;

            vc.fetch_yuv(rs,&p);

            if(i<200)
            write_avpicture(&p,fd);

            release_vdpau_render_state(rs);

        }else{
            fprintf(stderr,"Wait Frame..\n");
        }



    }




    close(fd);




//    sleep(10000);


    return 0;




}

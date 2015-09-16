#ifndef __AV_SHM_H__

#define __AV_SHM_H__



typedef struct _shm_head{
    int flag;//should never access directly 
#if 0
    int x;
    int y;//top left
    int x1;
    int y1;//bottom right
#endif
    int w;
    int h;
    
}av_shm_head;


static inline int shmhdr_input_data(void *shm_addr,int w,int h,char* pyuv,int Len)
{
	//printf("--input data shm len= %d \n",Len);
	if(Len <= 0)
		return -1;
	av_shm_head* hdr = (av_shm_head*)shm_addr;
	char *shm_yuv = (char*)shm_addr + sizeof(av_shm_head);
    //LOCK
    int itime = 10;

	while(hdr->flag == 1 && itime > 0)// busy
	{
		usleep(500);
		--itime;
	}
	//plen must more len shm data
	if(hdr->flag == 1)
	{
		//still busy
		return -1;
	}
	hdr->flag = 1; //lock
#if 0
	hdr->x = x;
    hdr->y = y;
    hdr->x1 = x1;
    hdr->y1 = y1;
#endif
    hdr->w = w;
    hdr->h = h;
	//copy data len = w*h*3/2
	memcpy(shm_yuv,pyuv,Len);
	
    //UNLOCK
   	hdr->flag = 0;
	//printf("--input data shm len=%d over\n ",hdr->len);

    return 1;
}


static inline int shmhdr_get_data(void *shm_addr,int*w,int*h,char* pyuv,int *pLen){

	av_shm_head* hdr = (av_shm_head*)shm_addr;
	char *shm_yuv = (char*)shm_addr + sizeof(av_shm_head);
    //LOCK
    int itime = 10;
	while(hdr->flag == 1 && itime > 0)// busy
	{
		usleep(500);
		--itime;
	}
	
	//plen must more len shm data
	int len = hdr->w*hdr->h*3/2;
	if(len > *pLen)
	{
		fprintf(stderr,"shm len not enough \n");
		return -1;
	}
	if(hdr->flag == 1)
	{
		//still busy
		return -1;
	}
	hdr->flag = 1; //lock
#if 0
    if(x)
        *x=hdr->x;
    if(y)
        *y=hdr->y;
    if(x1)
        *x1=hdr->x1;
    if(y1)
        *y1=hdr->y1;
#endif
    if(w)
        *w=hdr->w;
    if(h)
        *h=hdr->h;
	//copy data
	memcpy(pyuv,shm_yuv,len);
	*pLen = len;
	
    //UNLOCK
   	hdr->flag = 0;
	//printf("get data over\n");
    return 1;
}








#endif //__AV_SHM_H__


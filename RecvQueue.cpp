#include "RecvQueue.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

const int TS_PACKET_SIZE = 188;

FILE *fpdumxer = NULL;

NewQueue::NewQueue(int iport)
{
	bufsize = 0;
	write_ptr = 0;
	read_ptr = 0;
	buf = NULL;
	m_iport = iport;
	m_boverlay = false;
	m_hsIDRFrame = false;
	m_bIsOverlaying = false;
	m_bInitDecoder = false;
	m_bDelayFrame = false;

	sem_init(&m_sem_send,0,0);

	//if(NULL == fpdumxer)
	//	fpdumxer = fopen("dumxer.pes","w+b");
}

NewQueue::~NewQueue()
{
	free_queue();
}

void* NewQueue::udp_ts_recv(void* param)
{
	NewQueue* this0 = (NewQueue*)param;
	struct sockaddr_in s_addr;
	struct sockaddr_in c_addr;
	int sock;
	socklen_t addr_len;
	int len;
	uint8_t UDP_buf[4096];
	FILE *fp;


	//fp = fopen("out.ts", "wb+");

	if ( (sock = socket(AF_INET, SOCK_DGRAM, 0))  == -1) {
		perror("socket");
		exit(errno);
	} else
		printf("create socket.\n\r");

	memset(&s_addr, 0, sizeof(struct sockaddr_in));
	s_addr.sin_family = AF_INET;

	s_addr.sin_port = htons(this0->m_iport);


	s_addr.sin_addr.s_addr = INADDR_ANY;

	struct sockaddr_in send_addr;
	send_addr.sin_family=AF_INET;
	send_addr.sin_port = htons(this0->m_iSendPort);
	send_addr.sin_addr.s_addr = inet_addr(this0->m_cdstIP);

	if ( (bind(sock, (struct sockaddr*)&s_addr, sizeof(s_addr))) == -1 ) {
		perror("bind");
		exit(errno);
	}else
		printf("bind address to socket.\n\r");

	int nRecvBuf = 0;
	socklen_t iLen = 4;
	getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &nRecvBuf, &iLen);
	nRecvBuf = 1024*1024;//设置为
	setsockopt(sock,SOL_SOCKET,SO_RCVBUF,&nRecvBuf,sizeof(socklen_t));
	int nSize = 0;
	getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &nSize, &iLen);

	printf("begin recv data=========port =%d,nsize=%d \n",this0->m_iport,nSize);

	
	addr_len = sizeof(struct sockaddr);
	bool bneedwait = false;

	while(1) {
		len = recvfrom(sock, UDP_buf, sizeof(UDP_buf)-1, 0, (struct sockaddr*)&c_addr, &addr_len);
		if (len < 0) {
			perror("recvfrom");
			exit(errno);
		}
		bool bhandle = false;
		//printf("======recv size = %d",len);
//		fwrite(buf, sizeof(char), len, fp);
//		printf("recv from %s:%d,msg len:%d\n\r", inet_ntoa(c_addr.sin_addr), ntohs(c_addr.sin_port), len);

/*		if(this0->m_bDelayFrame)
		{
			int ihandleLen = 0;
			bhandle = this0->dumxer(UDP_buf,len,&ihandleLen,1);
			
			if(bhandle)
			{
				printf("------wait a frame delay \n");
				this0->m_bDelayFrame=false;
				int iret = sendto(sock,UDP_buf,len,0,(struct sockaddr*)&send_addr,sizeof(send_addr));
				if(iret < 0)
				{
					perror("sendto");
					exit(errno);
				}
				//后面部分需要送解码
				this0->put_queue( UDP_buf, len);
			}
		}
*/		
		if((this0->m_boverlay && !this0->m_hsIDRFrame) /*|| bneedwait*/)
		{

			int ihandleLen = 0;
			
			bhandle = this0->dumxer(UDP_buf,len,&ihandleLen);
			if(bhandle)
			{
				struct timeval tm;
				
				gettimeofday(&tm,NULL);
				printf("-----change to overlay =%ld\n",tm.tv_sec*1000+tm.tv_usec/1000);
				this0->m_bIsOverlaying = true;
				this0->m_hsIDRFrame = true;
				bneedwait = !bneedwait;
				this0->m_bDelayFrame = false;
				printf("------dumxer handle len =%d \n",ihandleLen);
				//fwrite(UDP_buf+ihandleLen,1,len-ihandleLen,fpdumxer);
				//fflush(fpdumxer);

				//fwrite(UDP_buf,1,len,fpdumxer);
				//fflush(fpdumxer);
				//if(!bneedwait)
				{
					//前面部分不是I帧，需要发送出去
					int iret = sendto(sock,UDP_buf,ihandleLen,0,(struct sockaddr*)&send_addr,sizeof(send_addr));
					if(iret < 0)
					{
						perror("sendto");
						exit(errno);
					}
					//后面部分需要送解码
					this0->put_queue( UDP_buf, len);
				}
			//else
			
/*			
				{
					if(ihandleLen > 0)
					{
						int iret = sendto(sock,UDP_buf,ihandleLen,0,(struct sockaddr*)&send_addr,sizeof(send_addr));
						if(iret < 0)
						{
							perror("sendto");
							exit(errno);
						}
					}
					//后面部分需要送解码
					this0->put_queue( UDP_buf, len);
				}
*/			
				

			}
			//需要将发送数据与解码数据区分处理
			
		}
		else if((!this0->m_boverlay && this0->m_bIsOverlaying))
		{

			int ihandleLen = 0;
			this0->m_bDelayFrame = true;
			
			bhandle = this0->dumxer(UDP_buf,len,&ihandleLen);
			if(bhandle)
			{
				//需要将发送数据与解码数据区分处理
				this0->m_bIsOverlaying = false;
				this0->m_hsIDRFrame = false;				
				
				bneedwait = !bneedwait;
					//printf("------dumxer handle len =%d \n",ihandleLen);
				//fwrite(UDP_buf+ihandleLen,1,len-ihandleLen,fp);
				//fflush(fp);
				
	/*				//前面部分不是I帧，需要送解码
				this0->put_queue( UDP_buf, ihandleLen);
				//后面部分需要发送出去 
				int iret = sendto(sock,UDP_buf+ihandleLen,len-ihandleLen,0,(struct sockaddr*)&send_addr,sizeof(send_addr));
				if(iret < 0)
				{
					perror("sendto");
					exit(errno);
				}
				
				*/

				


				struct timeval tm;

				gettimeofday(&tm,NULL);
				printf("-----change to send  =%ld\n",tm.tv_sec*1000+tm.tv_usec/1000);
				this0->put_queue( UDP_buf, len);

				//等tssmooth模块发送完整帧结束
				int mswait = 1000*40; //40 ms
				struct timespec ts;                         
	            clock_gettime(CLOCK_REALTIME, &ts );    //获取当前时间
                ts.tv_sec += (mswait / 1000 * 1000 *1000);        //加上等待时间的秒数
                ts.tv_nsec += ( mswait % 1000 ) * 1000; //加上等待时间纳秒数
                int rv = 0;
                rv=sem_timedwait( &this0->m_sem_send, &ts );

			//	sem_wait(&this0->m_sem_send);
				gettimeofday(&tm,NULL);
				printf("-----change to send 222 =%ld\n",tm.tv_sec*1000+tm.tv_usec/1000);
				//后面部分需要发送出去
				this0->m_bDelayFrame = false;
				int iret = sendto(sock,UDP_buf+ihandleLen,len-ihandleLen,0,(struct sockaddr*)&send_addr,sizeof(send_addr));
				if(iret < 0)
				{
					perror("sendto");
					exit(errno);
				}
					//usleep(5*1000);
			
			}	
		}
			
		if(!bhandle)
		{
			if((this0->m_boverlay && this0->m_hsIDRFrame)  ||(!this0->m_boverlay && this0->m_bIsOverlaying)){
				this0->put_queue( UDP_buf, len);
				/*if(!this0->m_boverlay && this0->m_bIsOverlaying)
				{
					int iret = sendto(sock,UDP_buf,len,0,(struct sockaddr*)&send_addr,sizeof(send_addr));
					if(iret < 0)
					{
						perror("sendto");
						exit(errno);
					}
				}*/
			}
			else if(!this0->m_boverlay && !this0->m_bIsOverlaying )
			{
				if(!this0->m_bInitDecoder)
				{
					//printf("--------init data copy\n");
					this0->put_queue( UDP_buf, len);
				}
				int iret = sendto(sock,UDP_buf,len,0,(struct sockaddr*)&send_addr,sizeof(send_addr));
				if(iret < 0)
				{
					perror("sendto");
					exit(errno);
				}
			}
		}
	}
//	fclose(fp);
	return NULL;
	
}

void NewQueue::init_queue( int size,int  iport,const char* dstip,short isendPort)
{
	pthread_mutex_init(&locker, NULL);
	//pthread_cond_init(&cond, NULL);
	buf = (uint8_t*)malloc(sizeof(uint8_t)*size);
	read_ptr = write_ptr = 0;
	bufsize = size;
	m_iport = iport;
	m_iSendPort = isendPort;
	memset(m_cdstIP,0,sizeof(m_cdstIP));
	strcpy(m_cdstIP,dstip);
	m_boverlay = false;
	
	//
	pthread_t udp_recv_thread;
	pthread_create(&udp_recv_thread, NULL, udp_ts_recv, this);
	pthread_detach(udp_recv_thread);
}

void NewQueue::free_queue()
{
	if(buf)
		free(buf);
	pthread_mutex_destroy(&locker);
	//pthread_cond_destroy(&cond);

}



void NewQueue::put_queue(uint8_t* inputbuff, int size)
{
	uint8_t* dst = buf + write_ptr;
	//if(m_boverlay)
	//{
	//	fwrite(inputbuff,1,size,fpdumxer);
	//	fflush(fpdumxer);
	//}

	if((write_ptr-read_ptr >= bufsize-1) || (write_ptr <= read_ptr -2))
	{
		printf("====failed write_ptr < readptr\n");
		usleep(100);
	}
	//printf("recv_data writelen=%d readlen=%d,buffsize=%d\n",write_ptr,read_ptr,bufsize);
	pthread_mutex_lock(&locker);

	if ((write_ptr + size) > bufsize) {
		memcpy(dst, inputbuff,(bufsize - write_ptr));
		memcpy(buf, inputbuff+(bufsize - write_ptr), size-(bufsize - write_ptr));
	} else {
		memcpy(dst, inputbuff, size*sizeof(uint8_t));
	}
	write_ptr = (write_ptr + size) % bufsize;

	//pthread_cond_signal(&cond);
	pthread_mutex_unlock(&locker);

}

int NewQueue::get_queue(uint8_t* outbuf, int size)
{
	uint8_t* src = buf + read_ptr;
	int wrap = 0;
	int irealLen = size;

	pthread_mutex_lock(&locker);

	int pos = write_ptr;

	if (pos < read_ptr) {
		pos += bufsize;
		wrap = 1;
	}

	if ( (read_ptr + size) > pos) {
		//memcpy(outbuf, src, (bufsize - read_ptr));
		//printf("read recv data failed \n");
		pthread_mutex_unlock(&locker);
		return -1;
//		struct timespec timeout;
//		timeout.tv_sec=time(0)+1;
//		timeout.tv_nsec=0;
//		pthread_cond_timedwait(&que->cond, &que->locker, &timeout);
//		if ( (que->read_ptr + size) > pos ) {
//			pthread_mutex_unlock(&que->locker);
//			return 1;
//		}
	}

	if (wrap) {
		fprintf(stdout, "wrap...\n");
		if(size > bufsize - read_ptr)
		{
			memcpy(outbuf, src, (bufsize - read_ptr));
			memcpy(outbuf+(bufsize - read_ptr), buf, size-(bufsize - read_ptr));
		}
		else
		{
			memcpy(outbuf, src, sizeof(uint8_t)*size);

		}
	} else {
		memcpy(outbuf, src, sizeof(uint8_t)*size);
	}
	read_ptr = (read_ptr + size) % bufsize;
	pthread_mutex_unlock(&locker);

	return 0;
}


bool NewQueue::set_tsDecoder_stat(bool bstat)
{
	if(m_boverlay != bstat)
	{

		m_boverlay= bstat;

		if(m_boverlay)
		{
			clean_RecvQue();
		}
		struct timeval tm;
		

		gettimeofday(&tm,NULL);
		printf("-----Set decoder %d stat =%ld\n",bstat,tm.tv_sec*1000+tm.tv_usec/1000);
		//m_hsIDRFrame = false;
		//m_bFirstDecodeSuccess = false;
	}
	return true;
}



bool NewQueue::dumxer(unsigned char * buff,int ilen,int *iHandleLen,int ineedflag)
{

	
	//识别是否ts头
	int itsHead;
	int index = 0;
	unsigned char *packet;
	const uint8_t *pESH264_IDR= NULL;

	int iseekLen = 0; //ts head len
	int iPesHeadLen = 0; //pes head len
	int iEsSeekLen = 0;
	int ifindLen =0;

	bool hasFindIframe = false;
	bool hasFindSPS = false;
	bool hasFindPPS = false;
	while(index < ilen && !hasFindIframe)
	{
		if(buff[index]==0x47 && buff[index+TS_PACKET_SIZE]==0x47)
		{
			hasFindIframe = false;
			hasFindSPS = false;
			hasFindPPS = false;
			//printf("-----find a ts packet \n");
			packet = &buff[index];
			if(packet[1] & 0x40)  //is_start = packet[1] & 0x40;
			{
				int len, pid, cc,  afc, is_start, is_discontinuity,
					has_adaptation, has_payload;
				unsigned short *sPID =  (unsigned short*)(packet + 1);
				pid = ntohs(*sPID)& 0x1fff;
				//printf("-------pid=%d\n",pid);
				

				afc = (packet[3] >> 4) & 3;//adaption
				
				if (afc == 0) /* reserved value */
					return 0;
				has_adaptation = afc & 2;
				has_payload = afc & 1;
				is_discontinuity = has_adaptation
					&& packet[4] != 0 /* with length > 0 */
					&& (packet[5] & 0x80); /* and discontinuity indicated */

				cc = (packet[3] & 0xf);
				const uint8_t *p, *p_end,*pEsData;

				p = packet + 4;
				if (has_adaptation) {
					/* skip adaptation field */
					p += p[0] + 1;
					/* if past the end of packet, ignore */
					p_end = packet + TS_PACKET_SIZE;
					if (p >= p_end)
						return 0;
				}
				//pes
				iseekLen = p - packet;
				if(p[0]==0x00&&p[1]==0x00&&p[2]==0x01&&p[3]==0xE0)
				{
					//pes len
					int ilen = p[4]<<8;
					ilen = ilen | p[5];
					//printf("-----------pes len =%d\n",ilen);

					iPesHeadLen=p[8] + 9;
					//printf("--------------pesheadlen=%d \n",iPesHeadLen);
					pEsData = p+iPesHeadLen;
					//
					//find 00000001 7 8 5
					int itempLen = TS_PACKET_SIZE - iseekLen - iPesHeadLen;
					
					const uint8_t *pData= NULL;
					ifindLen = 0;
					while(ifindLen < itempLen)
					{
						pData = pEsData + ifindLen;
						if(*pData==0x00 && *(pData+1)==0x00&& *(pData+2)==0x00 && *(pData+3)==0x01 )
						{
								printf("------------4 0 flag bits =%0x\n",*(pData+4));
								int iflag = (*(pData+4))&0x1f;
								//printf("---------h264 nal type =%d \n",iflag);

							/*	if(iflag == ineedflag)
								{
									printf("----------find need flag \n");
									pESH264_IDR = pData - 5;
									hasFindIframe = true;
									hasFindSPS = true;
									hasFindPPS = true;
									break;
								}
						*/		
								if(iflag == 7)
								{
									printf("----------find sps \n");
									pESH264_IDR = pData - 5;
									hasFindSPS = true;
									//break;
								}
								else if(iflag == 8)
								{
									printf("-------find pps \n");
									pESH264_IDR = pData - 5;
									hasFindPPS = true;
									//break;
									
								}
								else if(iflag == 5)
								{
									printf("-------find IDR \n");
									pESH264_IDR = pData - 5;
									hasFindIframe = true;
									//break;
								}
								if(hasFindIframe && hasFindPPS && hasFindSPS)
								{
									break;
								}
								ifindLen += 5;
							
						}
						else if(*pData==0x00 && *(pData+1)==0x00&& *(pData+2)==0x01 )
						{
								printf("------------3 0 flag bits =%0x\n",*(pData+3));
								int iflag = (*(pData+3))&0x1f;
								//printf("---------h264 nal type =%d \n",iflag);

							/*	if(iflag == ineedflag)
								{
									printf("----------find need flag \n");
									pESH264_IDR = pData - 4;
									hasFindIframe = true;
									break;
								}
								*/
								if(iflag == 7)
								{
									printf("----------find sps \n");
									pESH264_IDR = pData - 4;
									hasFindSPS = true;
									//break;
								}
								else if(iflag == 8)
								{
									printf("-------find pps \n");
									pESH264_IDR = pData - 4;
									hasFindPPS = true;
									//break;
									
								}
								else if(iflag == 5)
								{
									printf("-------find IDR \n");
									pESH264_IDR = pData - 4;
									hasFindIframe = true;
									//break;
								}
								if(hasFindIframe && hasFindPPS && hasFindSPS)
								{
									printf("----find all info \n");
									break;
								}
								ifindLen += 4;
									
							
						}
						else
						{
							ifindLen ++;
						}
						
					}
					
				}
			}
			if(!hasFindIframe)
				index += TS_PACKET_SIZE;
		}
		else
		{
			index++;
		}

	}
	if(hasFindIframe && hasFindPPS && hasFindSPS)
	{
		//int iseek_len = index /*+ pESH264_IDR - packet*/;

		printf("--------find IDR SPS  PPS index =%d tsheade len=%d peslen=%d eslen=%d \n",index,iseekLen,iPesHeadLen,ifindLen);
		*iHandleLen = index;
		// less len = ilen - iseek_len;
		//send data buff to iseek_len 

		return true;
	}

	return false;

}

void NewQueue::clean_RecvQue()
{
	pthread_mutex_lock(&locker);

	read_ptr = 0;
	write_ptr = 0;
	pthread_mutex_unlock(&locker);
}




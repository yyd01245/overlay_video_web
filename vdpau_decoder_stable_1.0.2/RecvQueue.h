#ifndef __RECVQUEUE_H_
#define __RECVQUEUE_H_

#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>



typedef unsigned char uint8_t;


class  NewQueue
{
public:
	pthread_mutex_t locker;
	//pthread_cond_t cond;
	sem_t m_sem_send;
	uint8_t* buf;
	int bufsize;
	int write_ptr;
	int read_ptr;
	int m_iport;
	int m_iSendPort;
	char m_cdstIP[256];
	bool m_boverlay;
	bool m_hsIDRFrame;
	bool m_bIsOverlaying;
	bool m_bInitDecoder;
	bool m_bDelayFrame;

	NewQueue(int iport=12000);
	~NewQueue();

	static void* udp_ts_recv(void* param);
	void init_queue( int size,int iport,const char* dstip,short isendPort);
	void free_queue();
	void put_queue( uint8_t* buf, int size);
	int get_queue(uint8_t* buf, int size);
	bool set_tsDecoder_stat(bool bstat);
	void clean_RecvQue();
	bool dumxer(unsigned char* buff,int ilen,int *iHandleLen,int iflag=7);
};








#endif

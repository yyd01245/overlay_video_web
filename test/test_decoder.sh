#!/bin/bash

iloopnum=128
decoder_path=../vdpau_decoder_v1.0/decoder
video_path=/home/x00/yyd/video_600x300/aqgy_600x360.ts

video_out=2001
audio_out=3000
width=600
height=360
corenum=4

#$decoder_path --inputurl udp://@:12345 --width 1280 --height 720 --videokey 1001 --audiofile /tmp/audiofifo

for((i=0;i<$iloopnum;++i))
do
    videokey=`expr 1000 + $i`
    echo $videokey
    audiofile="/tmp/audio"${i}
    echo $audiofile
    #coreid=`expr $i%$corenum`
    coreid=$(($i%($corenum)))
    #coreid=3
    echo $coreid
    $decoder_path --inputurl $video_path --width $width --height $height --coreid $coreid  --videokey $videokey --audiofile $audiofile 2>>/dev/null 1>>/dev/null  &
    #$decoder_path --inputurl $video_path --width $width --height $height --coreid $coreid  --videokey $videokey --audiofile $audiofile 
    sleep 1
done    

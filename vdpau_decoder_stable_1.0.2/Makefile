files=Decoder.cpp RecvQueue.cpp vdpauContext.cc win_x11.c

paths=-I ./ffmpeg_include
#ffpath=/usr/local/lib/
ffpath=./ffmpeg_lib/
libs=-pthread $(ffpath)libavformat.a $(ffpath)libavcodec.a $(ffpath)libavutil.a $(ffpath)libswscale.a -lz -lm $(ffpath)libswresample.a  -lX11 -lxcb -lvdpau

DEBUG= -g -D_DEBUG
#-pg -g -g -D_DEBUG

decoder:
	g++ pro_main.cpp -fPIC $(files) $(paths) $(libs)  $(DEBUG) -D__linux__ -o decoder
	#g++ pro_main.cpp -fPIC $(files)   $(libs) $(DEBUG) -D__linux__ -o decoder
	#g++ pro_main.cpp Decoder.cpp RecvQueue.cpp -o decoder -I./ffmpeg_include -L./ffmpeg_lib -lavformat -lavdevice -lavcodec -lavutil -pthread  -lswscale 
libtsdecoder.so: $(files)
	g++ -shared -fPIC $(files) $(libs) $(DEBUG) -D__linux -o libtsdecoder.so

install:libtsdecoder.so
	-strip -x libtsdecoder.so
	cp libtsdecoder.so /usr/local/lib/
	cp TSDecoder.h /usr/local/include/
	
uninstall:
	rm -rf /usr/local/lib/libtsdecoder.so
	rm -rf /usr/local/include/TSDecoder.h
	
test:
	-rm -f test
	g++ test.cpp -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o test
clean:
	-rm -f decoder
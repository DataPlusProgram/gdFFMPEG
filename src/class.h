#pragma once
#ifndef CLASS_HPP
#define CLASS_HPP

#include <Godot.hpp>
#include <Node.hpp>

#include <string>
#include<deque>
#include<Image.hpp>
#include"ImageFrame.h"
#include"simplePool.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <inttypes.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

}

struct rgbFrame
{
	int poolId;
	double timeStamp;
	unsigned char* rgb;
	rgbFrame(double timeStamp, unsigned char* rgb,int poolId);
};



struct audioFrame
{
	double timeStamp;
	int size;
	uint8_t* samples;
	audioFrame(int size, uint8_t* samples,double timeStamp);
};



struct StreamInfo
{
	int index = -1;
	const AVCodec* codec;
	AVCodecParameters* codecParams;
	AVCodecContext* codecCtx;

	StreamInfo();
	StreamInfo(int index,const AVCodec* codec, AVCodecParameters* codecParams);

};
 
namespace godot
{

class videoClass : public Node 
{
	GODOT_CLASS(videoClass, Node)

	public:
		videoClass();
		~videoClass();

		Dictionary loadFile(String path);
		void close();
		static void _register_methods();
		void _init();

		StreamInfo videoStream, audioStream;
		AVFormatContext* formatCtx;
		void printError(int);
		int readFrame();
		int width, height;
		bool initialized = false;
		bool hasAudio = false;
		bool hasVideo = false;
		int sampleRate, channels;
		double latestTime = 0;
		int imageBufferSize = 8;
		PoolByteArray rgbGodot;
		std::deque<AVFrame*> videoFrameBuffer;
		std::deque<AVFrame*> audioFrameBuffer;
		std::deque<audioFrame> audioBuffer;
		SimplePool* imagePool;
		Array getAudioInfo();
		SwsContext* sws_scalar_ctx = nullptr;
		unsigned char* data = nullptr;
		Array popImageBuffer();
		PoolByteArray popRawBuffer();
		int getImageBufferSize();
		void clearPoolEntry(int id);
		int getAudioBufferSize();
		int getSizeOfNextAudioFrame();
		PoolVector2Array popSampleBuffer();
		void processVideoFrame(AVFrame* frame);
		void processAudioFrame(AVFrame* frame);
		PoolEntry rgbArrToImage(unsigned char* data);
		PoolByteArray rgbArrToByte(unsigned char* data);
		
		std::deque<ImageFrame> imageBuffer;
		std::deque<Array> rawBuffer;
		Vector2 getDimensions();
		audioFrame fetchAudioBuffer(); 
		double getDuration();
		
		void processAudioFrame2(AVFrame* frame);

		void seek(float msec);
		void dgbPrintPoolSize();

		SwrContext* audioConv = nullptr;
		double durationSec, ratio, audioRatio;
		int process();
		bool videoOver = false;

		double getCurVideoTime();
		double getCurAudioTime();

		double curVideoTime = 9999;
		double curAudioTime = 9999;
		
		
};


}

#endif
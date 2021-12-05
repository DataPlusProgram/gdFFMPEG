#include "class.h"


using namespace godot;


void godot::videoClass::_register_methods()
{
	register_method("loadFile", &godot::videoClass::loadFile);
	register_method("process", &godot::videoClass::process);
	register_method("getDimensions", &godot::videoClass::getDimensions);
	register_method("popAudioBuffer", &godot::videoClass::popSampleBuffer);
	register_method("popRawBuffer", &godot::videoClass::popRawBuffer);
	register_method("getAudioInfo", &godot::videoClass::getAudioInfo);
	register_method("getSizeOfNextAudioFrame", &godot::videoClass::getSizeOfNextAudioFrame);
	register_method("getCurAudioTime", &godot::videoClass::getCurAudioTime);
	register_method("getCurVideoTime", &godot::videoClass::getCurVideoTime);
	register_method("getDuration", &godot::videoClass::getDuration);
	register_method("seek", &godot::videoClass::seek);
	register_method("dgbPrintPoolSize", &godot::videoClass::dgbPrintPoolSize);
	register_method("getImageBufferSize", &godot::videoClass::getImageBufferSize);
	register_method("getAudioBufferSize", &godot::videoClass::getAudioBufferSize);
	register_method("popImageBuffer", &godot::videoClass::popImageBuffer);
	register_method("clearPoolEntry", &godot::videoClass::clearPoolEntry);
	register_method("close", &godot::videoClass::close);
}



void godot::videoClass::_init()
{
;
}

godot::videoClass::videoClass()
{
}
		
godot::videoClass::~videoClass()
{
    ;
}





Dictionary godot::videoClass::loadFile(String path)
{
	Dictionary dict;

	formatCtx = avformat_alloc_context();
	int ret = avformat_open_input(&formatCtx, path.alloc_c_string(), NULL, NULL);

	if (ret != 0)
	{
		printError(ret);
		dict["error"] = ret;
		return dict;
	}

	avformat_find_stream_info(formatCtx, NULL);

	AVCodec* codec = nullptr;
	AVCodecParameters* codecParams = nullptr;
	const AVCodec* curCodec;
	
	durationSec = formatCtx->duration / 1000000.0;
	
	for (int i = 0; i < formatCtx->nb_streams; ++i)
	{
		codecParams = formatCtx->streams[i]->codecpar;
		curCodec = avcodec_find_decoder(codecParams->codec_id);

		if (!curCodec)
		{
			continue;
		}

		if (curCodec->type == AVMEDIA_TYPE_VIDEO)
		{
			videoStream = StreamInfo(i, curCodec, codecParams);

			AVRational timeBase = formatCtx->streams[i]->time_base;
			
			ratio = (double)timeBase.num / (double)timeBase.den;
			
			width = codecParams->width;
			height = codecParams->height;
			imagePool = new SimplePool(Vector2(width,height));
			hasVideo = true;
		}

		if (curCodec->type == AVMEDIA_TYPE_AUDIO)
		{
			AVRational timeBase = formatCtx->streams[i]->time_base;
			audioRatio = (double)timeBase.num / (double)timeBase.den;
			audioStream = StreamInfo(i, curCodec, codecParams);
			sampleRate = audioStream.codecCtx->sample_rate;
			channels = audioStream.codecCtx->channels;

			hasAudio = true;
		}

	}

	dict["hasAudio"] = hasAudio;
	dict["hasVideo"] = hasVideo;
	dict["error"] = ret;
	initialized = true;
	return dict;
}

void godot::videoClass::close()
{

	curVideoTime = 9999;
	curAudioTime = 9999;

	for (int i = 0; i < videoFrameBuffer.size(); i++)
	{
		av_frame_free(&videoFrameBuffer[i]);
	}

	for (int i = 0; i < audioFrameBuffer.size(); i++)
	{
		av_frame_free(&audioFrameBuffer[i]);
	}

	videoFrameBuffer.clear();
	audioFrameBuffer.clear();

	rawBuffer.clear();
	imageBuffer.clear();

	for (int i = 0; i < audioBuffer.size(); i++)
	{
		delete[] audioBuffer[i].samples;
	}

	audioBuffer.clear();
	imagePool->pool.clear();
	sws_scalar_ctx = nullptr;
	initialized = false;
	if (data != nullptr)
	{
		delete[] data;
		data = nullptr;
	}
	rgbGodot.resize(0);


	if (hasVideo) avcodec_close(videoStream.codecCtx);
	if (hasAudio) avcodec_close(audioStream.codecCtx);

	if (audioConv != nullptr)
	{

		swr_close(audioConv);
		swr_free(&audioConv);
		audioConv = nullptr;
	}

	videoStream.index = -1;
	audioStream.index = -1;

	hasVideo = false;
	hasAudio = false;

	avformat_close_input(&formatCtx);
}

void godot::videoClass::printError(int err)
{
	char* t = new char[44];
	av_strerror(err, t, 44);

}

int godot::videoClass::readFrame()
{
	
	AVPacket* av_packet = av_packet_alloc();
	int response = 0;
	int gotFrame = -1;
	while (av_read_frame(formatCtx, av_packet) >= 0)
	{

		gotFrame = 1;
		if (av_packet->stream_index != videoStream.index && av_packet->stream_index != audioStream.index)
			continue;

		int result = 0;

		StreamInfo targetStream;
		if (av_packet->stream_index == videoStream.index) targetStream = videoStream;
		if (av_packet->stream_index == audioStream.index) targetStream = audioStream;


		result = avcodec_send_packet(targetStream.codecCtx, av_packet);

		if (result == AVERROR_EOF)
		{
			av_packet_unref(av_packet);
			av_packet_free(&av_packet);
			return -1;
		}


		if (av_packet->size > 0 )
		{
			AVFrame* av_frame = av_frame_alloc();
			response = avcodec_receive_frame(targetStream.codecCtx, av_frame);
			if (response >= 0)
			{
				if (av_packet->stream_index == videoStream.index)
				{
					videoFrameBuffer.push_back(av_frame);
					break;
				}

				if (av_packet->stream_index == audioStream.index)
				{
					audioFrameBuffer.push_back(av_frame);
					if (hasAudio && !hasVideo)
					{
						break;
					}
				}
			}

			continue;
		}


	}

	av_packet_unref(av_packet);
	av_packet_free(&av_packet);
	if (gotFrame == -1)
	{
		return -1;
	}
	return 1;
}

int godot::videoClass::process()
{
	int ret = 1;
	if (initialized == false)
		return 1;

	if (hasVideo)
		if (imagePool->pool.size() > 200)//memory leak killswitch
			return -1;


	bool condition1 = hasVideo && !videoOver && videoFrameBuffer.size() < 4;
	bool condition2 = hasAudio && !videoOver && audioFrameBuffer.size() < 4;

	


	if (condition1 || condition2)
	{
		ret = readFrame();
		//if (ret == -1) return -1;
	}
	
	if (videoFrameBuffer.size() > 0 && rawBuffer.size() < imageBufferSize)//change if to while to process whole buffer
	{
		processVideoFrame(videoFrameBuffer.front());
		videoFrameBuffer.pop_front();
	}

	if (audioFrameBuffer.size() > 0 && audioBuffer.size() < 300)
	{
		processAudioFrame2(audioFrameBuffer.front());
		audioFrameBuffer.pop_front();
	}



	if (rawBuffer.size() > 0) curVideoTime = rawBuffer[0][1];
	if (audioBuffer.size() > 0) curAudioTime = audioBuffer[0].timeStamp;

	return ret;
}

double godot::videoClass::getCurVideoTime()
{
	return curVideoTime;
}

double godot::videoClass::getCurAudioTime()
{
	return curAudioTime;
}



audioFrame godot::videoClass::fetchAudioBuffer()
{
	if (audioBuffer.size() > 0)
	{
		audioFrame ret = audioBuffer[0];
		audioBuffer.pop_front();
		curAudioTime = ret.timeStamp;
		return ret;
	}

	return audioFrame(0, nullptr,-1);
}

double godot::videoClass::getDuration()
{
	return durationSec;
}



Array godot::videoClass::getAudioInfo()
{
	Array ret;
	ret.append(hasAudio);
	ret.append(channels);
	ret.append(sampleRate);
	return ret;
}


Array godot::videoClass::popImageBuffer()
{
	Ref<Image> img;
	//godot_dictionary dict;
	Array arr;
	if (imageBuffer.size() > 0)
	{
		img = imageBuffer[0].img;
		arr.append(imageBuffer[0].img);
		arr.append(imageBuffer[0].poolId);
		curVideoTime = imageBuffer[0].timeStamp;
		imageBuffer.pop_front();



		if (imageBuffer.size() > 0)
		{
			curVideoTime = imageBuffer[0].timeStamp;
		}
	}
	
	return arr;
}

PoolByteArray godot::videoClass::popRawBuffer()
{
	//Dictionary dict = rawBuffer[0];
	Array arr = rawBuffer[0];
	PoolByteArray r = arr[0];//dict["data"];
	
	curVideoTime = rawBuffer[0][1];
	rawBuffer.pop_front();
	

	if (rawBuffer.size() > 0)
		curVideoTime = rawBuffer[0][1];

	return r;
}


PoolByteArray godot::videoClass::rgbArrToByte(unsigned char* data)
{
	PoolByteArray ret;
	
	int size = width * height * 4;

	ret.resize(size);

	PoolByteArray::Write w = ret.write();
	uint8_t* p = w.ptr();
	memcpy(p, data, size);


	return ret;
}


PoolEntry godot::videoClass::rgbArrToImage(unsigned char* data)
{
	PoolEntry poolE = imagePool->fetch();
	Ref<Image> image = poolE.data.img;

	int size = width * height * 4;



	if (rgbGodot.size() != size)
	{
		rgbGodot.resize(size);
	}

	static PoolByteArray::Write w = rgbGodot.write();
	static uint8_t* p = w.ptr();


	memcpy(p, data, size);

	Dictionary d;
	d["width"] = width;
	d["height"] = height;
	d["mipmaps"] = false;
	d["format"] = "RGBA8";
	d["data"] = rgbGodot;

	image->_set_data(d);

	return poolE;
}


int godot::videoClass::getImageBufferSize()
{
	return rawBuffer.size();
	//return imageBuffer.size();
}

void godot::videoClass::clearPoolEntry(int id)
{
	imagePool->free(id);
}

int godot::videoClass::getAudioBufferSize()
{
	return audioBuffer.size();
}

int godot::videoClass::getSizeOfNextAudioFrame()
{
	if (audioBuffer.size() == 0)
		return INFINITY;
	return audioBuffer[0].size;
}

PoolVector2Array godot::videoClass::popSampleBuffer()
{
	PoolVector2Array sampleGodot;
	

	if (audioBuffer.size() > 0)
	{

		audioFrame audioFrame = audioBuffer[0];
		audioBuffer.pop_front();

		int size = (audioFrame.size/2)/2;
		sampleGodot.resize(size);
		

		for (int i = 0; i < size; i++)
		{
			unsigned short u1 = audioFrame.samples[(i*4)+0] | (audioFrame.samples[(i*4)+1] << 8);
			u1 = (u1 + 32768) & 0xffff;
			float s1 = float(u1 - 32768) / 32768.0;
			
			unsigned short u2 = audioFrame.samples[(i * 4) + 2] | (audioFrame.samples[(i * 4) + 3] << 8);
			u2 = (u2 + 32768) & 0xffff;
			float s2 = float(u2 - 32768) / 32768.0;

			Vector2 vec = Vector2(s1, s2);
			sampleGodot.set(i,vec);


		}


		if (audioBuffer.size() > 0)
		{
			curAudioTime = audioBuffer[0].timeStamp;
		}

		delete[] audioFrame.samples;
		 
	}
	return sampleGodot;
}

void godot::videoClass::processVideoFrame(AVFrame* frame)
{
	AVCodecParameters* codecParam = videoStream.codecParams;
	AVPixelFormat pixFormat = videoStream.codecCtx->pix_fmt;
	
	if (sws_scalar_ctx == nullptr)
		sws_scalar_ctx = sws_getContext(width, height, pixFormat, width, height, AVPixelFormat::AV_PIX_FMT_RGBA, SWS_FAST_BILINEAR, NULL, NULL, NULL);
	
	if (sws_scalar_ctx == NULL)
		return;

	int size = width * height * 5;//this should be 4 but there was a certain video that crashed and when changed to 5 it didn't. 

	if (data == nullptr)
		data = new unsigned char[size];

	unsigned char* dest[4] = { data,NULL,NULL,NULL };

	int dest_linesize[4] = { width * 4,0,0,0 };

	sws_scale(sws_scalar_ctx, frame->data, frame->linesize, 0, height, dest, dest_linesize);//converting YUV to RGBA
	double timestamp = frame->pts * ratio;
	
	if (frame->pts < 0 && frame->pkt_dts < 0)
	{

		std::cout << "negative dts found\n";
	}

	if (frame->pts < 0 && frame->pkt_dts > 0)
	{
		timestamp = frame->pkt_dts * ratio;
	}


	if (rawBuffer.size() == 0)
	{
		curVideoTime = timestamp;
	}

	//PoolEntry imgPe = rgbArrToImage(data);
	
	//Dictionary dict;
	//dict["data"] =rgbArrToByte(data);
	//dict["timestamp"] = timestamp;
	Array arr;
	arr.push_back(rgbArrToByte(data));
	arr.push_back(timestamp);


	//ImageFrame imageFrame  = imgPe.data;
	//imageFrame.timeStamp = timestamp;
	//imageBuffer.push_back(imageFrame);
	rawBuffer.push_back(arr);
	curVideoTime = rawBuffer[0][1];
	

	av_frame_free(&frame);

}




void godot::videoClass::processAudioFrame(AVFrame* frame)
{
	AVSampleFormat sampleForamt = audioStream.codecCtx->sample_fmt;
	int out_linesize;


	int dataSize = av_samples_get_buffer_size(&out_linesize, channels, frame->nb_samples, sampleForamt, 1);

	if (dataSize == 0)
	{
		int t = 3;
	}

	if (audioConv == nullptr)
	{

		
		audioConv = swr_alloc();
		audioConv = swr_alloc_set_opts(audioConv, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, frame->sample_rate, av_get_default_channel_layout(channels), sampleForamt, sampleRate, 1, NULL);
		if (audioConv < 0)
			int t = 3;

		int err = swr_init(audioConv);
		if (err < 0)
			printError(err);

	}

	uint8_t* out_buffer = new uint8_t[dataSize];
	int test = swr_convert(audioConv, &out_buffer, out_linesize, (const uint8_t**)frame->data, frame->nb_samples);

	if (test < 0)
	{
		printError(test);
	}

	double timestamp = frame->pts * audioRatio;
	if (frame->pts < 0) timestamp = curAudioTime + audioRatio;



	audioBuffer.push_back(audioFrame(out_linesize, out_buffer,timestamp));
	av_frame_free(&frame);
}

void videoClass::processAudioFrame2(AVFrame* frame)
{
	AVSampleFormat sampleForamt = audioStream.codecCtx->sample_fmt;

	int out_linesize;
	//int dataSize = av_samples_get_buffer_size(&out_linesize, channels, frame->nb_samples, sampleForamt, 1);
	int dataSize = av_samples_get_buffer_size(&out_linesize, 2, frame->nb_samples, AV_SAMPLE_FMT_S16, 1);

	if (frame->pkt_size == 0)
	{
		return;
	}

	if (dataSize < 0)
	{
		printError(dataSize);
		av_frame_free(&frame);

		return;
	}

	uint8_t* out_buffer = new uint8_t[dataSize];

	if (audioConv == nullptr)
	{
		audioConv = swr_alloc();

		audioConv = swr_alloc_set_opts(audioConv, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, frame->sample_rate, av_get_default_channel_layout(channels), sampleForamt, sampleRate, 1, NULL);
		int err = swr_init(audioConv);
		if (err != 0)
		{
			printError(err);
			audioConv == nullptr;
			return;
		}

	}

	int dst_nb_samples = (int)av_rescale_rnd(swr_get_delay(audioConv, frame->sample_rate) + frame->nb_samples, sampleRate, AV_CH_LAYOUT_STEREO, AV_ROUND_INF);
	int size2 = swr_convert(audioConv, &out_buffer, dst_nb_samples, (const uint8_t**)frame->data, frame->nb_samples);

	double timestamp = frame->pts * audioRatio;

	int t = channels * size2 * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

	int remaining = dataSize - size2;


	if (timestamp < 0)
	{
		timestamp = getCurVideoTime();
	}


	//audioBuffer.push_back(audioFrame(channels * size2 * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16), out_buffer, timestamp));
	audioBuffer.push_back(audioFrame(dataSize, out_buffer, timestamp));

}


Vector2 godot::videoClass::getDimensions()
{

	return Vector2(width,height);
}


StreamInfo::StreamInfo()
{
	;
}

StreamInfo::StreamInfo(int index,const AVCodec* codec, AVCodecParameters* codecParams)
{
	this->index = index;
	this->codec = codec;
	this->codecParams = codecParams;
	this->codecCtx = avcodec_alloc_context3(codec);


	if (this->codec->id != AV_CODEC_ID_PNG && this->codec->id != AV_CODEC_ID_WEBP && this->codec->id != AV_CODEC_ID_PSD)
	{

		this->codecCtx->thread_count = 0;

		if (codec->capabilities | AV_CODEC_CAP_FRAME_THREADS)
			this->codecCtx->thread_type = FF_THREAD_FRAME;
		else if (codec->capabilities | AV_CODEC_CAP_SLICE_THREADS)
			this->codecCtx->thread_type = FF_THREAD_SLICE;
		else
			this->codecCtx->thread_count = 1;
	}

	avcodec_parameters_to_context(codecCtx, codecParams);
	avcodec_open2(codecCtx, codec, NULL);
}

rgbFrame::rgbFrame(double timeStamp, unsigned char* rgb,int poolId)
{
	this->timeStamp = timeStamp;
	this->rgb = rgb;
	this->poolId = poolId;
}

audioFrame::audioFrame(int size, uint8_t* samples,double timeStamp)
{
	this->size = size;
	this->samples = samples;
	this->timeStamp = timeStamp;
}

void videoClass::seek(float sec)
{

	
	int ret = av_seek_frame(formatCtx, videoStream.index, sec / ratio, 0);
	printError(ret);

	if (hasAudio) avcodec_flush_buffers(audioStream.codecCtx);
	if (hasVideo) avcodec_flush_buffers(videoStream.codecCtx);



	audioBuffer.clear();
	videoFrameBuffer.clear();

	/*for (int i = 0; i < rgbBuffer.size(); i++)
	{
		delete[] rgbBuffer[i].rgb;
	}*/


	for (int i = 0; i < audioBuffer.size(); i++)
	{
		delete[] audioBuffer[i].samples;
	}


	rawBuffer.clear();
	audioBuffer.clear();

}

void godot::videoClass::dgbPrintPoolSize()
{
	std::cout << imagePool->pool.size();
}

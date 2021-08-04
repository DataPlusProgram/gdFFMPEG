#include "class.h"


using namespace godot;


void godot::videoClass::_register_methods()
{
	register_method("loadFile", &godot::videoClass::loadFile);
	register_method("process", &godot::videoClass::process);
	register_method("getDimensions", &godot::videoClass::getDimensions);
	register_method("popAudioBuffer", &godot::videoClass::popSampleBuffer);
	register_method("getAudioInfo", &godot::videoClass::getAudioInfo);
	register_method("getCurAudioTime", &godot::videoClass::getCurAudioTime);
	register_method("getCurVideoTime", &godot::videoClass::getCurVideoTime);
	register_method("getDuration", &godot::videoClass::getDuration);
	register_method("seek", &godot::videoClass::seek);
	register_method("dgbPrintPoolSize", &godot::videoClass::dgbPrintPoolSize);
	register_method("getImageBufferSize", &godot::videoClass::getImageBufferSize);
	register_method("popImageBuffer", &godot::videoClass::popImageBuffer);
	register_method("clearPoolEntry", &godot::videoClass::clearPoolEntry);
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
	avformat_find_stream_info(formatCtx, NULL);

	if (ret != 0)
	{
		printError(ret);
		dict["error"] = ret;
		return dict;
	}


	AVCodec* codec = nullptr;
	AVCodecParameters* codecParams = nullptr;
	const AVCodec* curCodec;
	


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
			
			durationSec = formatCtx->duration / 1000000.0;
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
	return dict;
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

		if (av_packet->size > 0)
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
	
	if (hasVideo)
		if (imagePool->pool.size() > 200)//memory leak killswitch
			return -1;

	if (!videoOver && imageBuffer.size() < imageBufferSize)
	{
		int ret = readFrame();
		if (ret == -1) return -1;
	}
	
	if (videoFrameBuffer.size() > 0 && imageBuffer.size() < imageBufferSize)//change if to while to process whole buffer
	{
		processVideoFrame(videoFrameBuffer.front());
		videoFrameBuffer.pop_front();
	}

	if (audioFrameBuffer.size() > 0)
	{
		processAudioFrame(audioFrameBuffer.front());
		audioFrameBuffer.pop_front();
	}



	if (imageBuffer.size() > 0) curVideoTime = imageBuffer[0].timeStamp;
	if (audioBuffer.size() > 0) curAudioTime = audioBuffer[0].timeStamp;
	return 1;
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
		imageBuffer.pop_front();

		if (imageBuffer.size() > 0)
		{
			curVideoTime = imageBuffer[0].timeStamp;
		}
	}
	
	return arr;
}

PoolEntry godot::videoClass::rgbArrToImage(unsigned char* data)
{
	PoolByteArray rgbGodot;
	PoolEntry poolE = imagePool->fetch();

	Ref<Image> image = poolE.data.img;

	int size = width * height * 4;

	if (rgbGodot.size() != size)
	{
		rgbGodot.resize(size);
	}


	auto w = rgbGodot.write();
	uint8_t* p = w.ptr();
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
	return imageBuffer.size();
}

void godot::videoClass::clearPoolEntry(int id)
{
	imagePool->free(id);
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

	static SwsContext* sws_scalar_ctx = sws_getContext(width, height, pixFormat, width, height, AVPixelFormat::AV_PIX_FMT_RGB0, SWS_FAST_BILINEAR, NULL, NULL, NULL);
	if (sws_scalar_ctx == NULL)
		return;

	int size = width * height * 5;//this should be 4 but there was a certain video that crashed and when changed to 5 it didn't. 

	static unsigned char* data = new unsigned char[size];
	unsigned char* dest[4] = { data,NULL,NULL,NULL };

	int dest_linesize[4] = { width * 4,0,0,0 };

	sws_scale(sws_scalar_ctx, frame->data, frame->linesize, 0, height, dest, dest_linesize);//converting YUV to RGBA
	double timestamp = frame->pts * ratio;
	
	PoolEntry imgPe = rgbArrToImage(data);
	
	ImageFrame imageFrame  = imgPe.data;
	imageFrame.timeStamp = timestamp;
	imageBuffer.push_back(imageFrame);

	curVideoTime = imageBuffer[0].timeStamp;
	av_frame_free(&frame);

}




void godot::videoClass::processAudioFrame(AVFrame* frame)
{
	AVSampleFormat sampleForamt = audioStream.codecCtx->sample_fmt;
	int out_linesize;

	int dataSize = av_samples_get_buffer_size(&out_linesize, channels, frame->nb_samples, sampleForamt, 1);

	if (audioConv == nullptr)
	{
		audioConv = swr_alloc();
		audioConv = swr_alloc_set_opts(audioConv, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, frame->sample_rate, audioStream.codecCtx->channel_layout, sampleForamt, sampleRate, 0, NULL);
		swr_init(audioConv);

	}

	uint8_t* out_buffer = new uint8_t[dataSize];
	int test = swr_convert(audioConv, &out_buffer, out_linesize, (const uint8_t**)frame->data, frame->nb_samples);
	double timestamp = frame->pts * audioRatio;

	audioBuffer.push_back(audioFrame(out_linesize, out_buffer,timestamp));
	av_frame_free(&frame);
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
	avformat_seek_file(formatCtx, videoStream.index, 0, sec, sec, 0);
	avcodec_flush_buffers(audioStream.codecCtx);
	avcodec_flush_buffers(videoStream.codecCtx);


	videoFrameBuffer.clear();
	imageBuffer.clear();


	for (int i = 0; i < audioBuffer.size(); i++)
	{
		delete[] audioBuffer[i].samples;
	}

	audioBuffer.clear();
	curVideoTime = 0;
	curAudioTime = 0;

}

void godot::videoClass::dgbPrintPoolSize()
{
	std::cout << imagePool->pool.size();
}

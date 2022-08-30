#include "pch.h"
#include "MyDecode.h"
#include <vector>

unsigned int srcWidth;
unsigned int srcHight;
long AVPixFmtYUV420PToBuff(unsigned char** pixBuff, char* outputBuff);
long BuffToAVPixFmtYUV422P(char* inputBuff, unsigned char** pixBuff);
long BuffToAVPixFmtYUV420P(char* inputBuff, unsigned char** pixBuff);
long AVPixFmtYUV422PToBuff(unsigned char** pixBuff, char* outputBuff);
long AVPixFmtNV12ToBuff(unsigned char** pixBuff, char* outputBuff);
long AVPixFmtRGB24ToBuff(unsigned char** pixBuff, char* outputBuff);
long BuffToAVPixFmtRGB24(char* inputBuff, unsigned char** pixBuff);
long BuffToAVPixFmtNV12(char* inputBuff, unsigned char** pixBuff);
static int hw_decoder_init(AVCodecContext* ctx, const enum AVHWDeviceType type);
static enum AVPixelFormat get_hw_format(AVCodecContext* ctx,const enum AVPixelFormat* pix_fmts);
static int decode_write(AVCodecContext* avctx, AVPacket* packet);

static AVBufferRef* hw_device_ctx = NULL;
static enum AVPixelFormat hw_pix_fmt;
static FILE* output_file = NULL;


int MyDecode::Decode(const char* filename,const char* outyuv, const char* outpcm)
{
	//1.读取视频
	AVFormatContext* formatCtx = avformat_alloc_context();
	if (formatCtx == nullptr) {
		printf("AVFormatContext* alloc fail.\n");
		return -1;
	}
	int ret = avformat_open_input(&formatCtx, filename, nullptr, nullptr);
	if (!ret) {
		avformat_find_stream_info(formatCtx, nullptr);
	}
	else {
		printf("file open failed.\n");
		return -1;
	}
	int videoStreamIndex = av_find_best_stream(formatCtx, AVMediaType::AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, (int)nullptr);
	int audioStreamIndex = av_find_best_stream(formatCtx, AVMediaType::AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, (int)nullptr);
	printf("VideoStreamIndex:%d\n", videoStreamIndex);
	printf("AudioStreamIndex:%d\n", audioStreamIndex);
	
	std::vector< AVCodecContext*>decoderList;
	//AVCodecParameters* codecpar = avcodec_parameters_alloc();
	int streamCount = formatCtx->nb_streams;
	int streamIndex = 0;
	AVCodecContext* codecContext;
	AVFrame* frame=nullptr;
	AVCodec* avCodec;
	for (int i = 0; i < streamCount; i++) {
		AVStream* ffmpegStream = formatCtx->streams[i];
		streamIndex = ffmpegStream->index;
		printf("StreamIndex:%d\n", streamIndex);
		//初始化解码器
		codecContext = avcodec_alloc_context3(nullptr);
		avcodec_parameters_to_context(codecContext, ffmpegStream->codecpar);
		avCodec = avcodec_find_decoder(codecContext->codec_id);
		codecContext->time_base = ffmpegStream->time_base;
		int ret = avcodec_open2(codecContext, avCodec, nullptr);
		if (ret) {
			printf("avcodec_open2 failed:%d\n", ret);
			return -1;
		}
		decoderList.push_back(codecContext);
	}	
	FILE* f_yuv, *f_pcm;
	fopen_s(&f_yuv, outyuv, "wb");
	fopen_s(&f_pcm, outpcm, "wb");

	AVPacket* pkt = av_packet_alloc();
	while (1) {
		ret = av_read_frame(formatCtx, pkt);
		if (ret) {
			break;
		}

		int index = pkt->stream_index;
		AVCodecContext* decoder = decoderList[index];
		ret = avcodec_send_packet(decoder, pkt);
		if (ret) {
			continue;
		}
		av_packet_unref(pkt);
		while (1) {
			frame = av_frame_alloc();
			ret = avcodec_receive_frame(decoder, frame);
			if (ret) {
				break;
			}
			if (index == videoStreamIndex) {
				int width = frame->width;
				int height = frame->height;
				printf("width:%d\n", frame->width);
				printf("height:%d\n", frame->height);
				for (int i = 0; i < AV_NUM_DATA_POINTERS; i++) {
					printf("Linesize[%d]:%d\n", i, frame->linesize[i]);
				}
				unsigned char* y = (unsigned char*)malloc(width * height);
				unsigned char* u = (unsigned char*)malloc(width / 2 * height / 2);
				unsigned char* v = (unsigned char*)malloc(width / 2 * height / 2);
				for (int i = 0; i < height; i++) {
					memcpy(y + i * width, frame->data[0] + i * frame->linesize[0], width);
				}
				for (int i = 0; i < height/2; i++) {
					memcpy(u + i * width/2, frame->data[1] + i * frame->linesize[1], width/2);
				}
				for (int i = 0; i < height/2; i++) {
					memcpy(v + i * width/2, frame->data[2] + i * frame->linesize[2], width/2);
				}
				fwrite(y, width * height, 1, f_yuv);
				fwrite(u, width / 2 * height / 2, 1, f_yuv);
				fwrite(v, width / 2 * height / 2, 1, f_yuv);
				free(y);
				free(u);
				free(v);
			}
			if (index == audioStreamIndex) {
				printf("声道channels:%d\n", frame->channels);
				printf("每个频道音频采样数nb_samples:%d\n", frame->nb_samples);
				printf("sample_rate:%d\n", frame->sample_rate);
				for (int i = 0; i < AV_NUM_DATA_POINTERS; i++) {
					printf("Linesize[%d]:%d\n", i, frame->linesize[i]);
				}
				int data_size = av_get_bytes_per_sample(decoder->sample_fmt);
				int channels = decoder->channels;
				int nb_samples = frame->nb_samples;
				if (av_sample_fmt_is_planar(decoder->sample_fmt) == 1) {
					printf("planar模式\n");
					for (int i = 0; i < nb_samples; i++) {
						for (int ch = 0; ch < channels; ch++) {
							fwrite(frame->data[ch] + data_size * i, 1, data_size, f_pcm);
						}
					}
				}
				else {
					printf("packet模式\n");
					fwrite(frame->data[0], nb_samples * data_size, 1, f_pcm);
					fwrite(frame->data[1], nb_samples * data_size, 1, f_pcm);
				}
			}
		}
	}
	av_packet_free(&pkt);
	//End:释放资源
	for (int i = 0; i < decoderList.size(); i++) {
		AVCodecContext* decoder = decoderList[i];
		ret= avcodec_send_packet(decoder, nullptr);
		while (1) {
			ret= avcodec_receive_frame(decoder,frame);
			if (ret) {
				break;
			}
		}
	}
	avformat_close_input(&formatCtx);
	for (int i = 0; i < decoderList.size(); i++) {
		AVCodecContext* decoder = decoderList[i];
		avcodec_close(decoder);
	}
	decoderList.clear();

	if (formatCtx != nullptr) {
		avformat_free_context(formatCtx);
		formatCtx = nullptr;
	}
	fclose(f_yuv);
	fclose(f_pcm);
	return 0;
}

int MyDecode::sws_convert_420pToRGB24(const char* filesrc, const char* filedst)
{
	FILE* srcVideo = nullptr;//源视频
	FILE* dstVideo = nullptr;//要转换的目标视频
	AVPixelFormat srcFormat;
	unsigned int dstWidth;
	unsigned int dstHight;
	AVPixelFormat dstFormat;
	uint8_t* m_srcPointers[4]{ nullptr,nullptr,nullptr,nullptr };
	int m_srcLinesizes[4]{ 0,0,0,0 };
	uint8_t* m_dstPointers[4]{ nullptr,nullptr,nullptr,nullptr };
	int m_dstLinesizes[4]{ 0,0,0,0 };
	float flotScale = 0;
	float out_flotScale = 0;
	int nSrcBuffLen = 0, nDstBuffLen = 0;
	srcWidth = 1920;
	srcHight = 1080;
	dstWidth = 1920;
	dstHight = 1080;
	srcFormat = AV_PIX_FMT_YUV420P;
	dstFormat = AV_PIX_FMT_RGB24;
	flotScale = 1.5;
	out_flotScale = 3;
	int in_buff_len = srcWidth * srcHight * flotScale;
	int out_buff_len = dstWidth * dstHight * out_flotScale;
	char* inbuff = new char[in_buff_len];//缓冲区，指向存储对象的内存块指针
	char* outbuff = new char[out_buff_len];
	fopen_s(&srcVideo, filesrc, "rb+");
	fopen_s(&dstVideo, filedst, "wb+");
	nSrcBuffLen = av_image_alloc(m_srcPointers, m_srcLinesizes, srcWidth, srcHight, srcFormat, 1);
	if (nSrcBuffLen <= 0) {
		printf("分配失败\n");
	}
	nDstBuffLen = av_image_alloc(m_dstPointers, m_dstLinesizes, dstWidth, dstHight, dstFormat, 1);
	if (nDstBuffLen <= 0) {
		printf("分配失败\n");
	}
	SwsContext* m_SwsCtx = sws_getContext(srcWidth, srcHight, srcFormat, dstWidth, dstHight, dstFormat, SWS_BICUBIC, NULL, NULL, NULL);
	if (m_SwsCtx == NULL) {
		printf("初始化失败\n");
		av_freep(&m_srcPointers);
		av_freep(&m_dstPointers);
	}
	while (true) {
		if (fread(inbuff, 1, in_buff_len, srcVideo) != in_buff_len) {
			break;
		}
		BuffToAVPixFmtYUV420P(inbuff, m_srcPointers);
		int ret = sws_scale(m_SwsCtx, m_srcPointers, m_srcLinesizes, 0, srcHight, m_dstPointers, m_dstLinesizes);
		AVPixFmtRGB24ToBuff(m_dstPointers, outbuff);
		if (ret != 0) {
			printf("conversion:ret:%d\n", ret);
		}
		fwrite(outbuff, 1, out_buff_len, dstVideo);
	}

	//关闭
	if (m_srcPointers)
		av_freep(&m_srcPointers);
	if (m_dstPointers)
		av_freep(&m_dstPointers);
	m_srcPointers[0] = nullptr;
	m_dstPointers[0] = nullptr;
	if (m_SwsCtx != nullptr) {
		sws_freeContext(m_SwsCtx);
	}
	m_SwsCtx = nullptr;
	return 0;
}
int MyDecode::sws_convert_420pToNV12(const char* filesrc, const char* filedst)
{
	FILE* srcVideo = nullptr;//源视频
	FILE* dstVideo = nullptr;//要转换的目标视频
	AVPixelFormat srcFormat;
	unsigned int dstWidth;
	unsigned int dstHight;
	AVPixelFormat dstFormat;
	uint8_t* m_srcPointers[4]{ nullptr,nullptr,nullptr,nullptr };
	int m_srcLinesizes[4]{ 0,0,0,0 };
	uint8_t* m_dstPointers[4]{ nullptr,nullptr,nullptr,nullptr };
	int m_dstLinesizes[4]{ 0,0,0,0 };
	float flotScale = 0;
	float out_flotScale = 0;
	int nSrcBuffLen = 0, nDstBuffLen = 0;
	srcWidth = 1920;
	srcHight = 1080;
	dstWidth = 1920;
	dstHight = 1080;
	srcFormat = AV_PIX_FMT_YUV420P;
	dstFormat = AV_PIX_FMT_NV12;
	flotScale = 1.5;
	out_flotScale = 1.5;
	int in_buff_len = srcWidth * srcHight * flotScale;
	int out_buff_len = dstWidth * dstHight * out_flotScale;
	char* inbuff = new char[in_buff_len];//缓冲区，指向存储对象的内存块指针
	char* outbuff = new char[out_buff_len];
	fopen_s(&srcVideo, filesrc, "rb+");
	fopen_s(&dstVideo, filedst, "wb+");
	nSrcBuffLen = av_image_alloc(m_srcPointers, m_srcLinesizes, srcWidth, srcHight, srcFormat, 1);
	if (nSrcBuffLen <= 0) {
		printf("分配失败\n");
	}
	nDstBuffLen = av_image_alloc(m_dstPointers, m_dstLinesizes, dstWidth, dstHight, dstFormat, 1);
	if (nDstBuffLen <= 0) {
		printf("分配失败\n");
	}
	SwsContext* m_SwsCtx = sws_getContext(srcWidth, srcHight, srcFormat, dstWidth, dstHight, dstFormat, SWS_BICUBIC, NULL, NULL, NULL);
	if (m_SwsCtx == NULL) {
		printf("初始化失败\n");
		av_freep(&m_srcPointers);
		av_freep(&m_dstPointers);
	}
	while (true) {
		if (fread(inbuff, 1, in_buff_len, srcVideo) != in_buff_len) {
			break;
		}
		BuffToAVPixFmtYUV420P(inbuff, m_srcPointers);
		int ret = sws_scale(m_SwsCtx, m_srcPointers, m_srcLinesizes, 0, srcHight, m_dstPointers, m_dstLinesizes);
		AVPixFmtNV12ToBuff(m_dstPointers, outbuff);
		if (ret != 0) {
			printf("conversion:ret:%d\n", ret);
		}
		fwrite(outbuff, 1, out_buff_len, dstVideo);
	}

	//关闭
	if (m_srcPointers)
		av_freep(&m_srcPointers);
	if (m_dstPointers)
		av_freep(&m_dstPointers);
	m_srcPointers[0] = nullptr;
	m_dstPointers[0] = nullptr;
	if (m_SwsCtx != nullptr) {
		sws_freeContext(m_SwsCtx);
	}
	m_SwsCtx = nullptr;
	return 0;
}
int MyDecode::filter_drawtext(const char* filesrc, const char* filedst)
{
	int ret;
	AVFrame* frame_in;
	AVFrame* frame_out;
	unsigned char* frame_buffer_in;
	unsigned char* frame_buffer_out;
	AVFilterGraph* filter_graph;
	AVFilterContext* bufferSink_ctx;
	AVFilterContext* bufferSrc_ctx;
	static int video_stream_index = -1;
	//输入yuv文件
	FILE* in;
	fopen_s(&in, filesrc, "rb+");
	if (in == nullptr) {
		printf("文件打开失败\n");
		return -1;
	}
	int width = 1920;
	int height = 1080;
	//输出yuv
	FILE* out;
	fopen_s(&out, filedst, "wb+");
	if (out == NULL) {
		printf("文件输出失败\n");
		return -1;
	}
	const char* filter_descr = "drawtext=fontfile=arial.ttf:fontcolor=green:fontsize=500:text='XYF'";

	char args[512] = { 0 };
	AVFilter* bufferSrc = (AVFilter*)avfilter_get_by_name("buffer");
	AVFilter* bufferSink = (AVFilter*)avfilter_get_by_name("buffersink");
	AVFilterInOut* outputs = avfilter_inout_alloc();
	AVFilterInOut* inputs = avfilter_inout_alloc();
	enum  AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P ,AV_PIX_FMT_NONE };
	AVBufferSinkParams* bufferSink_params;

	filter_graph = avfilter_graph_alloc();
	//_snprintf将可变参数 “…” 按照format的格式格式化为字符串，然后再将其拷贝至str中。
	_snprintf_s(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
		width, height, AV_PIX_FMT_YUV420P, 1, 50, 1, 1);
	//创建一个过滤器实例并将其添加到现有的图中。
	ret = avfilter_graph_create_filter(&bufferSrc_ctx, bufferSrc, "in", args, NULL, filter_graph);

	if (ret < 0) {
		printf("创建buffer source失败\n");
		return ret;
	}
	//缓冲视频接收器:用来终止过滤器链
	bufferSink_params = av_buffersink_params_alloc();
	bufferSink_params->pixel_fmts = pix_fmts;
	ret = avfilter_graph_create_filter(&bufferSink_ctx, bufferSink, "out", NULL, bufferSink_params, filter_graph);
	av_free(bufferSink_params);
	if (ret < 0) {
		printf("创建buffer sink失败\n");
		return ret;
	}
	outputs->name = av_strdup("in");
	outputs->filter_ctx = bufferSrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = bufferSink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;
	if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_descr,
		&inputs, &outputs, NULL)) < 0) {
		return ret;
	}
	if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0) {
		return ret;
	}
	frame_in = av_frame_alloc();
	frame_buffer_in = (unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width, height, 1));
	av_image_fill_arrays(frame_in->data, frame_in->linesize, frame_buffer_in,
		AV_PIX_FMT_YUV420P, width, height, 1);
	frame_out = av_frame_alloc();
	frame_buffer_out = (unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width, height, 1));
	av_image_fill_arrays(frame_out->data, frame_out->linesize, frame_buffer_out,
		AV_PIX_FMT_YUV420P, width, height, 1);
	frame_in->width = width;
	frame_in->height = height;
	frame_in->format = AV_PIX_FMT_YUV420P;
	while (1)
	{
		if (fread(frame_buffer_in, 1, width * height * 3 / 2, in) != width * height * 3 / 2) {
			break;
		}
		frame_in->data[0] = frame_buffer_in;
		frame_in->data[1] = frame_buffer_in + width * height;
		frame_in->data[2] = frame_buffer_in + width * height * 5 / 4;

		if (av_buffersrc_add_frame(bufferSrc_ctx, frame_in) < 0) {
			printf("add frame error.\n");
			break;
		}
		ret = av_buffersink_get_frame(bufferSink_ctx, frame_out);
		if (ret < 0) {
			break;
		}

		if (frame_out->format == AV_PIX_FMT_YUV420P) {
			for (int i = 0; i < frame_out->height; i++) {
				fwrite(frame_out->data[0] + frame_out->linesize[0] * i, 1, frame_out->width, out);
			}
			for (int i = 0; i < frame_out->height / 2; i++) {
				fwrite(frame_out->data[1] + frame_out->linesize[1] * i, 1, frame_out->width / 2, out);
			}
			for (int i = 0; i < frame_out->height / 2; i++) {
				fwrite(frame_out->data[2] + frame_out->linesize[2] * i, 1, frame_out->width / 2, out);
			}
		}
		printf("process 1 frame.\n");
		av_frame_unref(frame_out);
	}
	fclose(in);
	fclose(out);
	av_frame_free(&frame_in);
	av_frame_free(&frame_out);
	avfilter_graph_free(&filter_graph);
	return 0;
}
int MyDecode::hwDecode(const char* filesrc, const char* filedst)
{
	const char* hwdevice_fmt = "d3d11va";
	AVFormatContext* input_ctx = NULL;
	int video_stream, ret;
	AVStream* video = NULL;
	AVCodecContext* decoder_ctx = NULL;
	const AVCodec* decoder = NULL;
	AVPacket* packet = NULL;
	enum AVHWDeviceType type;
	int i;

	type = av_hwdevice_find_type_by_name(hwdevice_fmt);
	packet = av_packet_alloc();
	if (!packet) {
		fprintf(stderr, "Failed to allocate AVPacket\n");
		return -1;
	}
	if (avformat_open_input(&input_ctx, filesrc, NULL, NULL) != 0) {
		fprintf(stderr, "Cannot open input file '%s'\n", filesrc);
		return -1;
	}
	if (avformat_find_stream_info(input_ctx, NULL) < 0) {
		fprintf(stderr, "Cannot find input stream information.\n");
		return -1;
	}

	ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, (AVCodec**)&decoder, 0);
	if (ret < 0) {
		fprintf(stderr, "Cannot find a video stream in the input file\n");
		return -1;
	}
	video_stream = ret;
	for (i = 0;; i++) {
		const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, i);
		if (!config) {
			fprintf(stderr, "Decoder %s does not support device type %s.\n",
				decoder->name, av_hwdevice_get_type_name(type));
			return -1;
		}
		if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
			config->device_type == type) {
			hw_pix_fmt = config->pix_fmt;
			break;
		}
	}
	if (!(decoder_ctx = avcodec_alloc_context3(decoder)))
		return AVERROR(ENOMEM);

	video = input_ctx->streams[video_stream];
	if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0)
		return -1;
	decoder_ctx->get_format = get_hw_format;

	if (hw_decoder_init(decoder_ctx, type) < 0)
		return -1;

	if ((ret = avcodec_open2(decoder_ctx, decoder, NULL)) < 0) {
		fprintf(stderr, "Failed to open codec for stream #%u\n", video_stream);
		return -1;
	}
	/* open the file to dump raw data */
	fopen_s(&output_file, filedst, "w+b");
	while (ret >= 0) {
		if ((ret = av_read_frame(input_ctx, packet)) < 0)
			break;

		if (video_stream == packet->stream_index)
			ret = decode_write(decoder_ctx, packet);

		av_packet_unref(packet);
	}

	/* flush the decoder */
	ret = decode_write(decoder_ctx, NULL);
	if (output_file)
		fclose(output_file);
	av_packet_free(&packet);
	avcodec_free_context(&decoder_ctx);
	avformat_close_input(&input_ctx);
	av_buffer_unref(&hw_device_ctx);

	return 0;
}

int MyDecode::sws_convert_RGB24To420p(const char* filesrc, const char* filedst)
{
	FILE* srcVideo = nullptr;//源视频
	FILE* dstVideo = nullptr;//要转换的目标视频
	AVPixelFormat srcFormat;
	unsigned int dstWidth;
	unsigned int dstHight;
	AVPixelFormat dstFormat;
	uint8_t* m_srcPointers[4]{ nullptr,nullptr,nullptr,nullptr };
	int m_srcLinesizes[4]{ 0,0,0,0 };
	uint8_t* m_dstPointers[4]{ nullptr,nullptr,nullptr,nullptr };
	int m_dstLinesizes[4]{ 0,0,0,0 };
	float flotScale = 0;
	float out_flotScale = 0;
	int nSrcBuffLen = 0, nDstBuffLen = 0;
	srcWidth = 1920;
	srcHight = 1080;
	dstWidth = 1920;
	dstHight = 1080;
	srcFormat = AV_PIX_FMT_RGB24;
	dstFormat = AV_PIX_FMT_YUV420P;
	flotScale = 3;
	out_flotScale = 1.5;
	int in_buff_len = srcWidth * srcHight * flotScale;
	int out_buff_len = dstWidth * dstHight * out_flotScale;
	char* inbuff = new char[in_buff_len];//缓冲区，指向存储对象的内存块指针
	char* outbuff = new char[out_buff_len];
	fopen_s(&srcVideo, filesrc, "rb+");
	fopen_s(&dstVideo, filedst, "wb+");
	nSrcBuffLen = av_image_alloc(m_srcPointers, m_srcLinesizes, srcWidth, srcHight, srcFormat, 1);
	if (nSrcBuffLen <= 0) {
		printf("分配失败\n");
	}
	nDstBuffLen = av_image_alloc(m_dstPointers, m_dstLinesizes, dstWidth, dstHight, dstFormat, 1);
	if (nDstBuffLen <= 0) {
		printf("分配失败\n");
	}
	SwsContext* m_SwsCtx = sws_getContext(srcWidth, srcHight, srcFormat, dstWidth, dstHight, dstFormat, SWS_BICUBIC, NULL, NULL, NULL);
	if (m_SwsCtx == NULL) {
		printf("初始化失败\n");
		av_freep(&m_srcPointers);
		av_freep(&m_dstPointers);
	}
	while (true) {
		if (fread(inbuff, 1, in_buff_len, srcVideo) != in_buff_len) {
			break;
		}
		BuffToAVPixFmtRGB24(inbuff, m_srcPointers);
		int ret = sws_scale(m_SwsCtx, m_srcPointers, m_srcLinesizes, 0, srcHight, m_dstPointers, m_dstLinesizes);
		AVPixFmtYUV420PToBuff(m_dstPointers, outbuff);
		if (ret != 0) {
			printf("conversion:ret:%d\n", ret);
		}
		fwrite(outbuff, 1, out_buff_len, dstVideo);
	}

	//关闭
	if (m_srcPointers)
		av_freep(&m_srcPointers);
	if (m_dstPointers)
		av_freep(&m_dstPointers);
	m_srcPointers[0] = nullptr;
	m_dstPointers[0] = nullptr;
	if (m_SwsCtx != nullptr) {
		sws_freeContext(m_SwsCtx);
	}
	m_SwsCtx = nullptr;
	return 0;
}
int MyDecode::sws_convert_NV12To420p(const char* filesrc, const char* filedst)
{
	FILE* srcVideo = nullptr;//源视频
	FILE* dstVideo = nullptr;//要转换的目标视频
	AVPixelFormat srcFormat;
	unsigned int dstWidth;
	unsigned int dstHight;
	AVPixelFormat dstFormat;
	uint8_t* m_srcPointers[4]{ nullptr,nullptr,nullptr,nullptr };
	int m_srcLinesizes[4]{ 0,0,0,0 };
	uint8_t* m_dstPointers[4]{ nullptr,nullptr,nullptr,nullptr };
	int m_dstLinesizes[4]{ 0,0,0,0 };
	float flotScale = 0;
	float out_flotScale = 0;
	int nSrcBuffLen = 0, nDstBuffLen = 0;
	srcWidth = 1920;
	srcHight = 1080;
	dstWidth = 1920;
	dstHight = 1080;
	srcFormat = AV_PIX_FMT_NV12;
	dstFormat =AV_PIX_FMT_YUV420P ;
	flotScale = 1.5;
	out_flotScale = 1.5;
	int in_buff_len = srcWidth * srcHight * flotScale;
	int out_buff_len = dstWidth * dstHight * out_flotScale;
	char* inbuff = new char[in_buff_len];//缓冲区，指向存储对象的内存块指针
	char* outbuff = new char[out_buff_len];
	fopen_s(&srcVideo, filesrc, "rb+");
	fopen_s(&dstVideo, filedst, "wb+");
	nSrcBuffLen = av_image_alloc(m_srcPointers, m_srcLinesizes, srcWidth, srcHight, srcFormat, 1);
	if (nSrcBuffLen <= 0) {
		printf("分配失败\n");
	}
	nDstBuffLen = av_image_alloc(m_dstPointers, m_dstLinesizes, dstWidth, dstHight, dstFormat, 1);
	if (nDstBuffLen <= 0) {
		printf("分配失败\n");
	}
	SwsContext* m_SwsCtx = sws_getContext(srcWidth, srcHight, srcFormat, dstWidth, dstHight, dstFormat, SWS_BICUBIC, NULL, NULL, NULL);
	if (m_SwsCtx == NULL) {
		printf("初始化失败\n");
		av_freep(&m_srcPointers);
		av_freep(&m_dstPointers);
	}
	while (true) {
		if (fread(inbuff, 1, in_buff_len, srcVideo) != in_buff_len) {
			break;
		}
		BuffToAVPixFmtYUV420P(inbuff, m_srcPointers);
		int ret = sws_scale(m_SwsCtx, m_srcPointers, m_srcLinesizes, 0, srcHight, m_dstPointers, m_dstLinesizes);
		AVPixFmtYUV420PToBuff(m_dstPointers, outbuff);
		if (ret != 0) {
			printf("conversion:ret:%d\n", ret);
		}
		fwrite(outbuff, 1, out_buff_len, dstVideo);
	}

	//关闭
	if (m_srcPointers)
		av_freep(&m_srcPointers);
	if (m_dstPointers)
		av_freep(&m_dstPointers);
	m_srcPointers[0] = nullptr;
	m_dstPointers[0] = nullptr;
	if (m_SwsCtx != nullptr) {
		sws_freeContext(m_SwsCtx);
	}
	m_SwsCtx = nullptr;
	return 0;
}
int MyDecode::filter_hflip(const char* filesrc, const char* filedst)
{
	int ret;
	AVFrame* frame_in;
	AVFrame* frame_out;
	unsigned char* frame_buffer_in;
	unsigned char* frame_buffer_out;
	AVFilterGraph* filter_graph;
	AVFilterContext* bufferSink_ctx;
	AVFilterContext* bufferSrc_ctx;
	static int video_stream_index = -1;
	//输入yuv文件
	FILE* in;
	fopen_s(&in, filesrc, "rb+");
	if (in == nullptr) {
		printf("文件打开失败\n");
		return -1;
	}
	int width = 1920;
	int height = 1080;
	//输出yuv
	FILE* out;
	fopen_s(&out, filedst, "wb+");
	if (out == NULL) {
		printf("文件输出失败\n");
		return -1;
	}
	const char* filter_descr = "hflip";

	char args[512] = { 0 };
	AVFilter* bufferSrc = (AVFilter*)avfilter_get_by_name("buffer");
	AVFilter* bufferSink = (AVFilter*)avfilter_get_by_name("buffersink");
	AVFilterInOut* outputs = avfilter_inout_alloc();
	AVFilterInOut* inputs = avfilter_inout_alloc();
	enum  AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P ,AV_PIX_FMT_NONE };
	AVBufferSinkParams* bufferSink_params;

	filter_graph = avfilter_graph_alloc();
	//_snprintf将可变参数 “…” 按照format的格式格式化为字符串，然后再将其拷贝至str中。
	_snprintf_s(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
		width, height, AV_PIX_FMT_YUV420P, 1, 50, 1, 1);
	//创建一个过滤器实例并将其添加到现有的图中。
	ret = avfilter_graph_create_filter(&bufferSrc_ctx, bufferSrc, "in", args, NULL, filter_graph);

	if (ret < 0) {
		printf("创建buffer source失败\n");
		return ret;
	}
	//缓冲视频接收器:用来终止过滤器链
	bufferSink_params = av_buffersink_params_alloc();
	bufferSink_params->pixel_fmts = pix_fmts;
	ret = avfilter_graph_create_filter(&bufferSink_ctx, bufferSink, "out", NULL, bufferSink_params, filter_graph);
	av_free(bufferSink_params);
	if (ret < 0) {
		printf("创建buffer sink失败\n");
		return ret;
	}
	outputs->name = av_strdup("in");
	outputs->filter_ctx = bufferSrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = bufferSink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;
	if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_descr,
		&inputs, &outputs, NULL)) < 0) {
		return ret;
	}
	if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0) {
		return ret;
	}
	frame_in = av_frame_alloc();
	frame_buffer_in = (unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width, height, 1));
	av_image_fill_arrays(frame_in->data, frame_in->linesize, frame_buffer_in,
		AV_PIX_FMT_YUV420P, width, height, 1);
	frame_out = av_frame_alloc();
	frame_buffer_out = (unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width, height, 1));
	av_image_fill_arrays(frame_out->data, frame_out->linesize, frame_buffer_out,
		AV_PIX_FMT_YUV420P, width, height, 1);
	frame_in->width = width;
	frame_in->height = height;
	frame_in->format = AV_PIX_FMT_YUV420P;
	while (1)
	{
		if (fread(frame_buffer_in, 1, width * height * 3 / 2, in) != width * height * 3 / 2) {
			break;
		}
		frame_in->data[0] = frame_buffer_in;
		frame_in->data[1] = frame_buffer_in + width * height;
		frame_in->data[2] = frame_buffer_in + width * height * 5 / 4;

		if (av_buffersrc_add_frame(bufferSrc_ctx, frame_in) < 0) {
			printf("add frame error.\n");
			break;
		}
		ret = av_buffersink_get_frame(bufferSink_ctx, frame_out);
		if (ret < 0) {
			break;
		}

		if (frame_out->format == AV_PIX_FMT_YUV420P) {
			for (int i = 0; i < frame_out->height; i++) {
				fwrite(frame_out->data[0] + frame_out->linesize[0] * i, 1, frame_out->width, out);
			}
			for (int i = 0; i < frame_out->height / 2; i++) {
				fwrite(frame_out->data[1] + frame_out->linesize[1] * i, 1, frame_out->width / 2, out);
			}
			for (int i = 0; i < frame_out->height / 2; i++) {
				fwrite(frame_out->data[2] + frame_out->linesize[2] * i, 1, frame_out->width / 2, out);
			}
		}
		printf("process 1 frame.\n");
		av_frame_unref(frame_out);
	}
	fclose(in);
	fclose(out);
	av_frame_free(&frame_in);
	av_frame_free(&frame_out);
	avfilter_graph_free(&filter_graph);
	return 0;
}


int DecodeFile(const char* filename, const char* outyuv, const char* outpcm)
{
	MyDecode decode;
	return decode.Decode(filename, outyuv, outpcm);
}
int Convert420pToRGB24(const char* filesrc, const char* filedst)
{
	MyDecode d;
	return d.sws_convert_420pToRGB24(filesrc, filedst);
}
int Convert420pToNV12(const char* filesrc, const char* filedst)
{
	MyDecode d;
	return d.sws_convert_420pToNV12(filesrc, filedst);
}
int Filter_drawtext(const char* filesrc, const char* filedst)
{
	MyDecode d;
	return d.filter_drawtext(filesrc, filedst);
}
int HwDecode(const char* filesrc, const char* filedst)
{
	MyDecode d;
	return d.hwDecode(filesrc, filedst);
}
int ConvertRGB24To420p(const char* filesrc, const char* filedst)
{
	MyDecode d;
	return d.sws_convert_RGB24To420p(filesrc, filedst);
}
int ConvertNV12To420p(const char* filesrc, const char* filedst)
{
	MyDecode d;
	return d.sws_convert_NV12To420p(filesrc, filedst);
}
int Filter_hflip(const char* filesrc, const char* filedst)
{
	MyDecode d;
	return d.filter_hflip(filesrc, filedst);
}


long AVPixFmtYUV420PToBuff(unsigned char** pixBuff, char* outputBuff)
{
	memcpy(outputBuff, pixBuff[0], srcWidth * srcHight);											//Y
	memcpy(outputBuff + srcWidth * srcHight, pixBuff[1], srcWidth * srcHight / 4);				//U
	memcpy(outputBuff + srcWidth * srcHight * 5 / 4, pixBuff[2], srcWidth * srcHight / 4);		//V
	return 0;
}
long BuffToAVPixFmtYUV422P(char* inputBuff, unsigned char** pixBuff)
{
	memcpy(pixBuff[0], inputBuff, srcWidth * srcHight);											//Y
	memcpy(pixBuff[1], inputBuff + srcWidth * srcHight, srcWidth * srcHight / 2);				//U
	memcpy(pixBuff[2], inputBuff + srcWidth * srcHight * 3 / 2, srcWidth * srcHight / 2);		//V
	return 0;
}
long BuffToAVPixFmtYUV420P(char* inputBuff, unsigned char** pixBuff)
{
	memcpy(pixBuff[0], inputBuff, static_cast<size_t>(srcWidth * srcHight));											//Y
	memcpy(pixBuff[1], inputBuff + srcWidth * srcHight, srcWidth * srcHight / 4);				//U
	memcpy(pixBuff[2], inputBuff + srcWidth * srcHight * 5 / 4, srcWidth * srcHight / 4);		//V
	return 0;
}
long AVPixFmtYUV422PToBuff(unsigned char** pixBuff, char* outputBuff)
{
	memcpy(outputBuff, pixBuff[0], srcWidth * srcHight);											//Y
	memcpy(outputBuff + srcWidth * srcHight, pixBuff[1], srcWidth * srcHight / 2);				//U
	memcpy(outputBuff + srcWidth * srcHight * 3 / 2, pixBuff[2], srcWidth * srcHight / 2);		//V
	return 0;
}
long AVPixFmtNV12ToBuff(unsigned char** pixBuff, char* outputBuff)
{
	memcpy(outputBuff, pixBuff[0], srcHight * srcWidth);                    //Y
	memcpy(outputBuff + srcHight * srcWidth, pixBuff[1], srcHight * srcWidth / 2);      //Uv
	return 0;
}
long AVPixFmtRGB24ToBuff(unsigned char** pixBuff, char* outputBuff)
{
	memcpy(outputBuff, pixBuff[0], srcWidth * srcHight * 3);
	return 0;
}
long BuffToAVPixFmtRGB24(char* inputBuff, unsigned char** pixBuff)
{
	memcpy(pixBuff[0], inputBuff, srcWidth * srcHight * 3);
	return 0;
}
long BuffToAVPixFmtNV12(char* inputBuff, unsigned char** pixBuff)
{
	memcpy(pixBuff[0], inputBuff, srcWidth * srcHight);                    //Y
	memcpy(pixBuff[1], inputBuff + srcWidth * srcHight, srcWidth * srcHight / 2);      //Uv
	return 0;
}


static int hw_decoder_init(AVCodecContext* ctx, const enum AVHWDeviceType type)
{
	int err = 0;

	if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
		NULL, NULL, 0)) < 0) {
		fprintf(stderr, "Failed to create specified HW device.\n");
		return err;
	}
	ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

	return err;
}

static enum AVPixelFormat get_hw_format(AVCodecContext* ctx,
	const enum AVPixelFormat* pix_fmts)
{
	const enum AVPixelFormat* p;

	for (p = pix_fmts; *p != -1; p++) {
		if (*p == hw_pix_fmt)
			return *p;
	}

	fprintf(stderr, "Failed to get HW surface format.\n");
	return AV_PIX_FMT_NONE;
}

static int decode_write(AVCodecContext* avctx, AVPacket* packet)
{
	AVFrame* frame = NULL, * sw_frame = NULL;
	AVFrame* tmp_frame = NULL;
	uint8_t* buffer = NULL;
	int size;
	int ret = 0;

	ret = avcodec_send_packet(avctx, packet);
	if (ret < 0) {
		fprintf(stderr, "Error during decoding\n");
		return ret;
	}

	while (1) {
		if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
			fprintf(stderr, "Can not alloc frame\n");
			ret = AVERROR(ENOMEM);
			goto fail;
		}

		ret = avcodec_receive_frame(avctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			av_frame_free(&frame);
			av_frame_free(&sw_frame);
			return 0;
		}
		else if (ret < 0) {
			fprintf(stderr, "Error while decoding\n");
			goto fail;
		}

		if (frame->format == hw_pix_fmt) {
			/* retrieve data from GPU to CPU */
			if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
				fprintf(stderr, "Error transferring the data to system memory\n");
				goto fail;
			}
			tmp_frame = sw_frame;
		}
		else
			tmp_frame = frame;

		size = av_image_get_buffer_size((AVPixelFormat)tmp_frame->format, tmp_frame->width,
			tmp_frame->height, 1);
		buffer = (uint8_t*)av_malloc(size);
		if (!buffer) {
			fprintf(stderr, "Can not alloc buffer\n");
			ret = AVERROR(ENOMEM);
			goto fail;
		}
		ret = av_image_copy_to_buffer(buffer, size,
			(const uint8_t* const*)tmp_frame->data,
			(const int*)tmp_frame->linesize, (AVPixelFormat)tmp_frame->format,
			tmp_frame->width, tmp_frame->height, 1);
		if (ret < 0) {
			fprintf(stderr, "Can not copy image to buffer\n");
			goto fail;
		}

		if ((ret = fwrite(buffer, 1, size, output_file)) < 0) {
			fprintf(stderr, "Failed to dump raw data.\n");
			goto fail;
		}

	fail:
		av_frame_free(&frame);
		av_frame_free(&sw_frame);
		av_freep(&buffer);
		if (ret < 0)
			return ret;
	}
}
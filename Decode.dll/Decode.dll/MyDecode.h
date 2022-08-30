#pragma once
extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/frame.h"
#include "libavcodec/avcodec.h"
#include "libavutil/samplefmt.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/avutil.h"
#include "libavutil/pixdesc.h"
#include "libavutil/hwcontext.h"
#include "libavutil/avassert.h"
};
class MyDecode
{
public:
	int Decode(const char* filename,const char* outyuv, const char* outpcm);
	int sws_convert_420pToRGB24(const char* filesrc, const char* filedst);
	int sws_convert_420pToNV12(const char* filesrc, const char* filedst);
	int filter_drawtext(const char* filesrc, const char* filedst);
	int hwDecode(const char* filesrc, const char* filedst);
	int sws_convert_RGB24To420p(const char* filesrc, const char* filedst);
	int sws_convert_NV12To420p(const char* filesrc, const char* filedst);
	int filter_hflip(const char* filesrc, const char* filedst);
};

extern "C" __declspec (dllexport) int DecodeFile(const char* filename, const char* outyuv, const char* outpcm);
extern "C" __declspec (dllexport) int Convert420pToRGB24(const char* filesrc, const char* filedst);
extern "C" __declspec (dllexport) int Convert420pToNV12(const char* filesrc, const char* filedst);
extern "C" __declspec (dllexport) int Filter_drawtext(const char* filesrc, const char* filedst);
extern "C" __declspec (dllexport) int HwDecode(const char* filesrc, const char* filedst);
extern "C" __declspec (dllexport) int ConvertRGB24To420p(const char* filesrc, const char* filedst);
extern "C" __declspec (dllexport) int ConvertNV12To420p(const char* filesrc, const char* filedst);
extern "C" __declspec (dllexport) int Filter_hflip(const char* filesrc, const char* filedst);
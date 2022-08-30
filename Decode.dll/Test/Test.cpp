// Test.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <Windows.h>
typedef int (*funDecodeFile)(const char* filename, const char* outyuv, const char* outpcm);
typedef int (*funConvert420pToRGB24)(const char* filesrc, const char* filedst);
typedef int (*funConvert420pToNV12)(const char* filesrc, const char* filedst);
typedef int (*funFilter_drawtext)(const char* filesrc, const char* filedst);
typedef int (*funHwDecode)(const char* filesrc, const char* filedst);
typedef int (*funConvertRGB24To420p)(const char* filesrc, const char* filedst);
typedef int (*funConvertNV12To420p)(const char* filesrc, const char* filedst);
typedef int (*funFilter_hflip)(const char* filesrc, const char* filedst);
int main()
{
    HMODULE handle = LoadLibraryA("Decode.dll.dll");
    if (handle && handle != INVALID_HANDLE_VALUE)
    {
        /*funDecodeFile decode = (funDecodeFile)GetProcAddress(handle, "DecodeFile");
        if (decode)
        {
            decode("d://test_media/in/test_1920x1080.mp4", "d://test_media/out/outyuv.yuv", "d://test_media/out/outpcm.pcm");
        }*/
        funHwDecode f = (funHwDecode)GetProcAddress(handle, "HwDecode");
        if (f) {
            f("d://test_media/test_1920x1080.mp4", "d://test_media/out/hwdecode_NV12.yuv");
        }
        FreeLibrary(handle);
    }
    std::cout << "Hello World!\n";
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件

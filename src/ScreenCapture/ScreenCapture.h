#pragma once

#include <Windows.h>

extern "C" {
    #define INVALID_HANDLE -1
    unsigned int __declspec(dllexport) WINAPI StartCapture(int x, int y, int width, int height, bool captureCursor);
    void __declspec(dllexport) WINAPI StopCapture(unsigned int handle);
    double __declspec(dllexport) WINAPI ReadNextFrame(unsigned int handle, char* buffer, unsigned int size);
    bool __declspec(dllexport)  WINAPI WaitForNextFrame(unsigned int handle, int timeout);
    
    // EncodeVideo error return codes.
    const int ReasonUnknown = 1;
    const int ReasonInvalidProfile = 2;
    const int ReasonCodecNotFound = 3;

    const int VideoEncodingQualityAuto = 0;
    const int VideoEncodingQualityHD1080p = 1;
    const int VideoEncodingQualityHD720p = 2;
    const int VideoEncodingQualityWvga = 3;
    const int VideoEncodingQualityNtsc = 4;
    const int VideoEncodingQualityPal = 5;
    const int VideoEncodingQualityVga = 6;
    const int VideoEncodingQualityQvga = 7;
    const int VideoEncodingQualityUhd2160p = 8;
    const int VideoEncodingQualityUhd4320p = 9;

    struct VideoEncoderProperties
    {
        unsigned int bitrateInBps; // e.g. 9000000 for 9 mbps.
        unsigned int frameRate; // e.g 30 or 60
        unsigned int quality; // see above
        unsigned int seconds; // maximum length before encoding finishes or 0 for infinite.
        unsigned int memory_cache; // 1=use in-memory caching so no disk activity until encoding is done.
    };

    int __declspec(dllexport) WINAPI EncodeVideo(unsigned int captureHandle, const WCHAR* filename, VideoEncoderProperties* properties);
    int __declspec(dllexport) WINAPI StopEncoding();
    unsigned int __declspec(dllexport) WINAPI GetTicks(double* buffer, unsigned int size);
    unsigned int __declspec(dllexport) WINAPI GetArrivalTimes(unsigned int captureHandle, double* buffer, unsigned int size);
}
#pragma once

#include <Windows.h>

extern "C" {
    #define INVALID_HANDLE -1
    unsigned int WINAPI StartCapture(int x, int y, int width, int height, bool captureCursor);
    void WINAPI StopCapture(unsigned int handle);
    unsigned long long WINAPI ReadNextFrame(unsigned int handle, char* buffer, unsigned int size);
    bool WINAPI WaitForNextFrame(unsigned int handle, int timeout);
    
    const int Unknown = 1;
    const int InvalidProfile = 2;
    const int CodecNotFound = 3;

    int WINAPI EncodeVideo(unsigned int captureHandle, const WCHAR* filename, unsigned int bitrateInBps, unsigned int frameRate);
    int WINAPI StopEncoding();
    unsigned int WINAPI GetTicks(double* buffer, unsigned int size);
}
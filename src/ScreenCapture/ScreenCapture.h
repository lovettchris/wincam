#pragma once

#include <Windows.h>

typedef int CaptureHandle;
#define INVALID_HANDLE -1

CaptureHandle WINAPI StartCapture(int x, int y, int width, int height, bool captureCursor);
void WINAPI StopCapture(CaptureHandle handle);
unsigned long long WINAPI ReadNextFrame(CaptureHandle handle, char* buffer, unsigned int size);
bool WINAPI WaitForFirstFrame(CaptureHandle handle, int timeout);
RECT WINAPI GetCaptureBounds(CaptureHandle handle);

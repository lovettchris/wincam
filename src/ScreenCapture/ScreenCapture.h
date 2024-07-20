#pragma once

#include <Windows.h>

int WINAPI StartCapture(int x, int y, int width, int height, bool captureCursor);
void WINAPI StopCapture();
unsigned long long WINAPI ReadNextFrame(char* buffer, unsigned int size);
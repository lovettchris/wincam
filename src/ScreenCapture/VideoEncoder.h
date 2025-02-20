#pragma once
#include "SimpleCapture.h"
#include "ScreenCapture.h"
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Storage.Streams.h>

class VideoEncoderImpl;

class VideoEncoder
{
public:
    __declspec(dllexport) VideoEncoder();
    __declspec(dllexport) ~VideoEncoder();

    __declspec(dllexport) winrt::Windows::Foundation::IAsyncOperation<int> EncodeAsync(
        std::shared_ptr<SimpleCapture> capture,
        VideoEncoderProperties* properties,
        winrt::Windows::Storage::Streams::IRandomAccessStream stream);

    __declspec(dllexport) void Stop();

    __declspec(dllexport) unsigned int GetTicks(double* buffer, unsigned int size);

    __declspec(dllexport) bool IsRunning();

    __declspec(dllexport) double GetStartDelay();

private:
    std::unique_ptr<VideoEncoderImpl> m_pimpl;
};

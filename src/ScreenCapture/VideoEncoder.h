#pragma once
#include "SimpleCapture.h"
#include "ScreenCapture.h"

class VideoEncoderImpl
{
public:
    virtual winrt::Windows::Foundation::IAsyncOperation<int> EncodeAsync(
        std::shared_ptr<SimpleCapture> capture,
        VideoEncoderProperties* properties,
        std::wstring filePath) = 0;

    virtual void Stop() = 0;

    virtual unsigned int GetSampleTimes(double* buffer, unsigned int size) = 0;

    virtual bool IsRunning() = 0;

    unsigned int GetBestBitRate(int frameRate, int  quality);

    virtual const char* GetErrorMessage(int hr) = 0;
};

class VideoEncoder
{
public:
    __declspec(dllexport) VideoEncoder();
    __declspec(dllexport) ~VideoEncoder();

    __declspec(dllexport) winrt::Windows::Foundation::IAsyncOperation<int> EncodeAsync(
        std::shared_ptr<SimpleCapture> capture,
        VideoEncoderProperties* properties,
        std::wstring filePath);

    __declspec(dllexport) void Stop();

    __declspec(dllexport) unsigned int GetSampleTimes(double* buffer, unsigned int size);

    __declspec(dllexport) bool IsRunning();

    __declspec(dllexport) const char* GetErrorMessage(int hr);

private:
    void CreateImpl();
    std::unique_ptr<VideoEncoderImpl> m_pimpl;
    bool _ffmpeg;
};

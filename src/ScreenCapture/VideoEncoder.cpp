#include "pch.h"
#include "FFmpegEncoder.h"
#include "WindowsEncoder.h"

unsigned int VideoEncoderImpl::GetBestBitRate(int frameRate, int quality)
{
    if (frameRate != 30 && frameRate != 60) frameRate = 30;
    int mbps = 0;
    switch (quality)
    {
    case VideoEncodingQualityHD1080p:
        mbps = frameRate == 30 ? 16 : 24;
        break;
    case VideoEncodingQualityWvga:
        mbps = frameRate == 30 ? 3 : 4;
        break;
    case VideoEncodingQualityNtsc:
    case VideoEncodingQualityPal:
    case VideoEncodingQualityVga:
    case VideoEncodingQualityQvga:
        mbps = frameRate == 30 ? 1 : 2;
        break;
    case VideoEncodingQualityUhd2160p:
        mbps = frameRate == 30 ? 24 : 36;
        break;
    case VideoEncodingQualityUhd4320p:
        mbps = frameRate == 30 ? 36 : 72;
        break;
    case VideoEncodingQualityAuto:
    case VideoEncodingQualityHD720p:
        mbps = frameRate == 30 ? 8 : 12;
        break;
    default:
        mbps = frameRate == 30 ? 5 : 9;
        break;
    }
    return mbps * 1000000;
}


VideoEncoder::VideoEncoder()
{
    m_pimpl = CreateWindowsEncoder();
    // m_pimpl = CreateFFmpegEncoder();
}

VideoEncoder::~VideoEncoder()
{
}

void VideoEncoder::Stop()
{
    m_pimpl->Stop();
}

bool VideoEncoder::IsRunning() { return m_pimpl->IsRunning(); }

unsigned int VideoEncoder::GetSampleTimes(double* buffer, unsigned int size)
{
	return m_pimpl->GetSampleTimes(buffer, size);
}

winrt::Windows::Foundation::IAsyncOperation<int> VideoEncoder::EncodeAsync(
    std::shared_ptr<SimpleCapture> capture,
    VideoEncoderProperties* properties,
    winrt::Windows::Storage::Streams::IRandomAccessStream stream)
{
	return m_pimpl->EncodeAsync(capture, properties, stream);
}

const char* VideoEncoder::GetErrorMessage(int hr)
{
    return m_pimpl->GetErrorMessage(hr);
}
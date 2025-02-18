#pragma once
#include "SimpleCapture.h"
#include "ScreenCapture.h"
#include "Timer.h"
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Storage.Streams.h>

class VideoEncoder
{
public:
    VideoEncoder();
    ~VideoEncoder();

    winrt::Windows::Foundation::IAsyncOperation<int> EncodeAsync(
        std::shared_ptr<SimpleCapture> capture,
        VideoEncoderProperties* properties,
        winrt::Windows::Storage::Streams::IRandomAccessStream stream);

    void Stop();

    unsigned int GetTicks(double* buffer, unsigned int size);

    bool IsRunning() { return _running; }

private:
    void OnVideoStarting(winrt::Windows::Media::Core::MediaStreamSource const& src, winrt::Windows::Media::Core::MediaStreamSourceStartingEventArgs const& args);
    void OnSampleRequested(winrt::Windows::Media::Core::MediaStreamSource const& src, winrt::Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs const& args);
    std::shared_ptr<SimpleCapture> _capture; 
    bool _stopped = false;
    std::vector<double> _ticks;
    double _maxDuration = 0; // seconds
    bool _running = false;
    util::Timer _sampleTimer;
    uint32_t _msPerFrame = 30;
};


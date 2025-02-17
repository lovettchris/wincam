#pragma once
#include "SimpleCapture.h"
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Storage.Streams.h>

class VideoEncoder
{
public:
    VideoEncoder();
    ~VideoEncoder();

    winrt::Windows::Foundation::IAsyncOperation<int> EncodeAsync(
        std::shared_ptr<SimpleCapture> capture,
        unsigned int bitrateInBps, unsigned int framerate,
        winrt::Windows::Storage::Streams::IRandomAccessStream stream);

    void Stop();

    unsigned int GetTicks(double* buffer, unsigned int size);

    bool IsRunning() { return _running; }

private:
    void OnVideoStarting(winrt::Windows::Media::Core::MediaStreamSource const& src, winrt::Windows::Media::Core::MediaStreamSourceStartingEventArgs const& args);
    void OnSampleRequested(winrt::Windows::Media::Core::MediaStreamSource const& src, winrt::Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs const& args);
    std::shared_ptr<SimpleCapture> _capture; 
    bool _stopped;
    std::vector<double> _ticks;
    double _startTick;
    bool _running;
};


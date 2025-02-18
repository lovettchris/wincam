#include "pch.h"
#include <Windows.Foundation.h>
#include <Windows.System.Threading.h>
#include <wrl/event.h>
#include <stdio.h>
#include <Objbase.h>

#include "VideoEncoder.h"
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.MediaProperties.h>
#include <winrt/Windows.Media.Transcoding.h>

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::System::Threading;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace winrt::Windows::Media::Core;

#include <d3d11.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <Windows.graphics.directx.direct3d11.interop.h>
#include <winrt/base.h>
#undef min 

winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface CreateDirect3DSurfaceFromTexture(ID3D11Texture2D* texture)
{
    winrt::com_ptr<IDXGISurface> dxgiSurface;
    texture->QueryInterface(__uuidof(IDXGISurface), dxgiSurface.put_void());

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface d3dSurface{ nullptr };
    winrt::check_hresult(CreateDirect3D11SurfaceFromDXGISurface(dxgiSurface.get(), reinterpret_cast<::IInspectable**>(winrt::put_abi(d3dSurface))));

    return d3dSurface;
}

VideoEncoder::VideoEncoder()
{
    _stopped = false;
}

VideoEncoder::~VideoEncoder()
{
}

void VideoEncoder::Stop()
{
    _stopped = true;
}

unsigned int VideoEncoder::GetTicks(double* buffer, unsigned int size)
{    
    auto available = (unsigned int)_ticks.size();
    if (buffer != nullptr) {
        auto count = std::min(available, size);
        ::memcpy(buffer, _ticks.data(), count * sizeof(double));
    }
    return available;
}

winrt::Windows::Foundation::IAsyncOperation<int> VideoEncoder::EncodeAsync(
    std::shared_ptr<SimpleCapture> capture,
    VideoEncoderProperties* properties,
    winrt::Windows::Storage::Streams::IRandomAccessStream stream)
{
    _stopped = false;
    _running = true;
    if (!capture->WaitForNextFrame(10000)) {
        _running = false;
        throw std::exception("frames are not arriving");
    }
    _ticks.clear();
    _capture = capture;
    auto bounds = capture->GetTextureBounds();
    auto width = bounds.right - bounds.left;
    auto height = bounds.bottom - bounds.top;
    auto bitrateInBps = properties->bitrateInBps;
    auto frameRate = properties->frameRate;
    _maxDuration = properties->seconds;
    _msPerFrame = (1000 / frameRate);

    // Describe mp4 video properties
    auto qality = (winrt::Windows::Media::MediaProperties::VideoEncodingQuality)(properties->quality);
    auto encodingProfile = winrt::Windows::Media::MediaProperties::MediaEncodingProfile::CreateMp4(qality);
    encodingProfile.Container().Subtype(L"MPEG4");
    auto videoProperties = encodingProfile.Video();
    videoProperties.Subtype(L"H264");
    videoProperties.Width((uint32_t)width);
    videoProperties.Height((uint32_t)height);
    videoProperties.Bitrate(bitrateInBps);
    videoProperties.FrameRate().Numerator(frameRate);
    videoProperties.FrameRate().Denominator(1);
    videoProperties.PixelAspectRatio().Numerator(1);
    videoProperties.PixelAspectRatio().Denominator(1);

    // Describe our input: uncompressed BGRA8 buffers
    auto streamProperties = winrt::Windows::Media::MediaProperties::VideoEncodingProperties::CreateUncompressed(
        winrt::Windows::Media::MediaProperties::MediaEncodingSubtypes::Bgra8(), 
        (uint32_t)width, (uint32_t)height);
    streamProperties.Bitrate(bitrateInBps);
    VideoStreamDescriptor videoDescriptor(streamProperties);

    // Create our MediaStreamSource
    MediaStreamSource mediaStreamSource(videoDescriptor);
    mediaStreamSource.BufferTime(std::chrono::seconds::zero());
    winrt::event_token token1 = mediaStreamSource.Starting({ this, &VideoEncoder::OnVideoStarting });
    winrt::event_token token2 = mediaStreamSource.SampleRequested({ this, &VideoEncoder::OnSampleRequested });
    int result = 0;
    try {
        winrt::Windows::Media::Transcoding::MediaTranscoder transcoder;
        transcoder.HardwareAccelerationEnabled(true);
        auto prepResult = co_await transcoder.PrepareMediaStreamSourceTranscodeAsync(mediaStreamSource, stream, encodingProfile);
        if (!prepResult.CanTranscode()) {
            result = (int)prepResult.FailureReason();
        }
        else 
        {
            auto progress = prepResult.TranscodeAsync();
            co_await progress;
        }
        mediaStreamSource.Starting(token1);
        mediaStreamSource.SampleRequested(token2);
        _capture = nullptr;
        _running = false;
    }
    catch (const std::exception&)
    {
        mediaStreamSource.Starting(token1);
        mediaStreamSource.SampleRequested(token2);
        _capture = nullptr;
        _running = false;
        throw;
    }
    co_return result;
}

void VideoEncoder::OnVideoStarting(MediaStreamSource const& src, MediaStreamSourceStartingEventArgs const& args)
{
    winrt::com_ptr<ID3D11Texture2D> result;
    _capture->ReadNextTexture(10000, result);
    _sampleTimer.start();
    args.Request().SetActualStartPosition(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::milliseconds(0)));
}

void VideoEncoder::OnSampleRequested(MediaStreamSource const& src, MediaStreamSourceSampleRequestedEventArgs const& args)
{
    if (_stopped) {
        args.Request().Sample(nullptr);
    }
    else 
    {
        winrt::com_ptr<ID3D11Texture2D> result;
        // don't wait more than _msPerFrame for each sample so we get a nice smooth video.
        auto timestamp = _capture->ReadNextTexture(_msPerFrame, result);
        auto seconds = _sampleTimer.seconds();
        // we could compare timestamp and seconds to see how well the capture rate is doing.
        _ticks.push_back(seconds);
        if (_maxDuration > 0 && seconds >= _maxDuration) {
            _stopped = true;
        };
        if (result != nullptr) {
            std::chrono::microseconds ms(static_cast<long long>(seconds * 1e6));
            auto sample = MediaStreamSample::CreateFromDirect3D11Surface(
                CreateDirect3DSurfaceFromTexture(result.get()), ms);
            args.Request().Sample(sample);
        }
        else
        {
            args.Request().Sample(nullptr);
        }
    }
}

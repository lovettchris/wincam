#include "pch.h"
#include <Windows.Foundation.h>
#include <Windows.System.Threading.h>
#include <wrl/event.h>
#include <stdio.h>
#include <Objbase.h>

#include "Timer.h"
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

class VideoEncoderImpl {
public:
    std::shared_ptr<SimpleCapture> _capture;
    bool _stopped = false;
    std::vector<double> _ticks;
    double _maxDuration = 0; // seconds
    bool _running = false;
    util::Timer _sampleTimer;

public:

    VideoEncoderImpl() {

    }
    void Stop() {
        _stopped = true;
    }
    unsigned int GetTicks(double* buffer, unsigned int size)
    {
        auto available = (unsigned int)_ticks.size();
        if (buffer != nullptr) {
            auto count = std::min(available, size);
            ::memcpy(buffer, _ticks.data(), count * sizeof(double));
        }
        return available;
    }

    bool IsRunning() { return _running; }

    static unsigned int GetBestBitRate(int frameRate, winrt::Windows::Media::MediaProperties::VideoEncodingQuality quality)
    {
        if (frameRate != 30 && frameRate != 60) frameRate = 30;
        int mbps = 0;
        switch (quality)
        {
        case winrt::Windows::Media::MediaProperties::VideoEncodingQuality::HD1080p:
            mbps = frameRate == 30 ? 16 : 24;
            break;
        case winrt::Windows::Media::MediaProperties::VideoEncodingQuality::Wvga:
            mbps = frameRate == 30 ? 3 : 4;
            break;
        case winrt::Windows::Media::MediaProperties::VideoEncodingQuality::Ntsc:
        case winrt::Windows::Media::MediaProperties::VideoEncodingQuality::Pal:
        case winrt::Windows::Media::MediaProperties::VideoEncodingQuality::Vga:
        case winrt::Windows::Media::MediaProperties::VideoEncodingQuality::Qvga:
            mbps = frameRate == 30 ? 1 : 2;
            break;
        case winrt::Windows::Media::MediaProperties::VideoEncodingQuality::Uhd2160p:
            mbps = frameRate == 30 ? 24 : 36;
            break;
        case winrt::Windows::Media::MediaProperties::VideoEncodingQuality::Uhd4320p:
            mbps =  frameRate == 30 ? 36 : 72;
            break;
        case winrt::Windows::Media::MediaProperties::VideoEncodingQuality::Auto:
        case winrt::Windows::Media::MediaProperties::VideoEncodingQuality::HD720p:
            mbps = frameRate == 30 ? 8 : 12;
            break;
        default:
            mbps = frameRate == 30 ? 5 : 9;
            break;
        }
        return mbps * 1000000;
    }

    winrt::Windows::Foundation::IAsyncOperation<int> EncodeAsync(
        std::shared_ptr<SimpleCapture> capture,
        VideoEncoderProperties* properties,
        winrt::Windows::Storage::Streams::IRandomAccessStream stream)
    {
        _stopped = false;
        _running = true;
        _ticks.clear();
        _sampleTimer.Start();
        _capture = capture;

        if (!capture->WaitForNextFrame(10000)) {
            _running = false;
            throw std::exception("frames are not arriving");
        }
        auto bounds = capture->GetTextureBounds();
        auto width = bounds.right - bounds.left;
        auto height = bounds.bottom - bounds.top;
        auto frameRate = properties->frameRate;
        auto quality = (winrt::Windows::Media::MediaProperties::VideoEncodingQuality)(properties->quality);
        auto bitrateInBps = properties->bitrateInBps;
        if (bitrateInBps == 0) {
            bitrateInBps = GetBestBitRate(frameRate, quality);
            properties->bitrateInBps = bitrateInBps;
        }
        _maxDuration = properties->seconds;

        // Describe mp4 video properties
        auto encodingProfile = winrt::Windows::Media::MediaProperties::MediaEncodingProfile::CreateMp4(quality);
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
        winrt::event_token token1 = mediaStreamSource.Starting({ this, &VideoEncoderImpl::OnVideoStarting });
        winrt::event_token token2 = mediaStreamSource.SampleRequested({ this, &VideoEncoderImpl::OnSampleRequested });
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

    void OnVideoStarting(MediaStreamSource const& src, MediaStreamSourceStartingEventArgs const& args)
    {
        winrt::com_ptr<ID3D11Texture2D> result;
        _capture->ReadNextTexture(10000, result);
        args.Request().SetActualStartPosition(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::milliseconds(0)));
    }

    void OnSampleRequested(MediaStreamSource const& src, MediaStreamSourceSampleRequestedEventArgs const& args)
    {
        if (_stopped) {
            args.Request().Sample(nullptr);
        }
        else
        {
            winrt::com_ptr<ID3D11Texture2D> result;
            auto timestamp = _capture->ReadNextTexture(10000, result);
            auto seconds = timestamp; // use the time this frame was rendered to sync the video.
            
            // but store ticks according to our start time so the user knows how much delay there was getting
            // the video pipeline up and running.
            _ticks.push_back(_sampleTimer.Seconds()); 
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
};

VideoEncoder::VideoEncoder()
{
	m_pimpl = std::make_unique<VideoEncoderImpl>();
}

VideoEncoder::~VideoEncoder()
{
}

void VideoEncoder::Stop()
{
    m_pimpl->Stop();
}

bool VideoEncoder::IsRunning() { return m_pimpl->_running; }

unsigned int VideoEncoder::GetTicks(double* buffer, unsigned int size)
{
	return m_pimpl->GetTicks(buffer, size);
}

winrt::Windows::Foundation::IAsyncOperation<int> VideoEncoder::EncodeAsync(
    std::shared_ptr<SimpleCapture> capture,
    VideoEncoderProperties* properties,
    winrt::Windows::Storage::Streams::IRandomAccessStream stream)
{
	return m_pimpl->EncodeAsync(capture, properties, stream);
}
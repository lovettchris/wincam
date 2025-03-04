#include "pch.h"
#include <wrl/event.h>
#include <stdio.h>
#include <filesystem>
#include <Objbase.h>
#include "WindowsEncoder.h"
#include "Timer.h"
#include "VideoEncoder.h"
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.MediaProperties.h>
#include <winrt/Windows.Media.Transcoding.h>
#include <Windows.Foundation.h>
#include <Windows.System.Threading.h>
#define D3D11_NO_HELPERS
#include <d3d11.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <Windows.graphics.directx.direct3d11.interop.h>
#include <winrt/base.h>
#undef min

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::System::Threading;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace winrt::Windows::Media::Core;

#define ERROR_NO_FRAMES -10
#define ERROR_UNKNOWN -11


winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface CreateDirect3DSurfaceFromTexture(ID3D11Texture2D* texture)
{
    winrt::com_ptr<IDXGISurface> dxgiSurface;
    texture->QueryInterface(__uuidof(IDXGISurface), dxgiSurface.put_void());

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface d3dSurface{ nullptr };
    winrt::check_hresult(CreateDirect3D11SurfaceFromDXGISurface(dxgiSurface.get(), reinterpret_cast<::IInspectable**>(winrt::put_abi(d3dSurface))));

    return d3dSurface;
}


class MediaTranscoderImpl : public VideoEncoderImpl {
public:
    std::shared_ptr<SimpleCapture> _capture;
    bool _stopped = false;
    std::vector<double> _ticks;
    double _maxDuration = 0; // seconds
    bool _running = false;
    util::Timer _sampleTimer;
    std::string _errorString;

public:

    MediaTranscoderImpl() {

    }
    void Stop() override {
        _stopped = true;
    }
    unsigned int GetSampleTimes(double* buffer, unsigned int size) override
    {
        auto available = (unsigned int)_ticks.size();
        if (buffer != nullptr) {
            auto count = std::min(available, size);
            ::memcpy(buffer, _ticks.data(), count * sizeof(double));
        }
        return available;
    }

    bool IsRunning() { return _running; }

    winrt::Windows::Foundation::IAsyncOperation<int> EncodeAsync(
        std::shared_ptr<SimpleCapture> capture,
        VideoEncoderProperties* properties,
        std::wstring filePath) override
    {
        _stopped = false;
        _running = true;
        _ticks.clear();
        _sampleTimer.Start();
        _capture = capture;
        _errorString = "";

        if (!capture->WaitForNextFrame(10000)) {
            _running = false;
            co_return ERROR_NO_FRAMES;
        }
        auto bounds = capture->GetTextureBounds();
        auto width = bounds.right - bounds.left;
        auto height = bounds.bottom - bounds.top;
        auto frameRate = properties->frameRate;
        auto quality = (winrt::Windows::Media::MediaProperties::VideoEncodingQuality)(properties->quality);
        auto bitrateInBps = properties->bitrateInBps;
        if (bitrateInBps == 0) {
            bitrateInBps = GetBestBitRate(frameRate, properties->quality);
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

        // Open our output file
        std::filesystem::path fsPath(filePath);
        auto path = fsPath.parent_path().wstring();
        auto filename = fsPath.filename().wstring();
        int rc = 0;
        auto folder = co_await winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(path);
        auto file = co_await folder.CreateFileAsync(filename);
        auto stream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::ReadWrite);

        // Create our MediaStreamSource
        MediaStreamSource mediaStreamSource(videoDescriptor);
        winrt::event_token token1 = mediaStreamSource.Starting({ this, &MediaTranscoderImpl::OnVideoStarting });
        winrt::event_token token2 = mediaStreamSource.SampleRequested({ this, &MediaTranscoderImpl::OnSampleRequested });
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
        catch (const std::exception& ex)
        {
            _errorString = ex.what();
            mediaStreamSource.Starting(token1);
            mediaStreamSource.SampleRequested(token2);
            _capture = nullptr;
            _running = false;
            result = ERROR_UNKNOWN;
        }
        co_await stream.FlushAsync();
        stream.Close();
        co_return result;
    }

    const char* GetErrorMessage(int hr) override
    {
        // EncodeVideo error return codes.
        switch (hr) {
        case ERROR_NO_FRAMES:
            return "No frames are arriving";
        case ERROR_UNKNOWN:
            return _errorString.c_str();
        case 2:
            return "Invalid profile";
        case 3:
            return "Codec not found";
        default:
            return "Unknown error";
        }
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


std::unique_ptr<VideoEncoderImpl> CreateWindowsEncoder()
{
    return std::make_unique<MediaTranscoderImpl>();
}
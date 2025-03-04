#include "pch.h"
#include "SimpleCapture.h"
#include "Errors.h"

#include <winrt/Windows.Graphics.Capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.capture.h>
#include <winrt/windows.graphics.directx.direct3d11.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <windows.ui.composition.interop.h>
#include <d2d1_1.h>
#include <dxgi1_6.h>
#include <d3d11.h>

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Numerics;
    using namespace Windows::Graphics;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::Graphics::DirectX::Direct3D11;
    using namespace Windows::System;
    using namespace Windows::UI;
    using namespace Windows::UI::Composition;
}


namespace util {

    struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1"))
        IDirect3DDxgiInterfaceAccess : ::IUnknown
    {
        virtual HRESULT __stdcall GetInterface(GUID const& id, void** object) = 0;
    };

    template <typename T>
    auto GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object)
    {
        auto access = object.as<IDirect3DDxgiInterfaceAccess>();
        winrt::com_ptr<T> result;
        winrt::check_hresult(access->GetInterface(winrt::guid_of<T>(), result.put_void()));
        return result;
    }

}

// since we are requesting B8G8R8A8UIntNormalized
const int CHANNELS = 4;

CRITICAL_SECTION m_mutex;

class CriticalSectionGuard
{
public:
    CriticalSectionGuard() { EnterCriticalSection(&m_mutex); }
    ~CriticalSectionGuard() { LeaveCriticalSection(&m_mutex); }
};


class SimpleCaptureImpl
{
public:

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device{ nullptr };
    winrt::com_ptr<ID3D11Device> m_d3dDevice{ nullptr };
    winrt::com_ptr<ID3D11DeviceContext> m_d3dContext{ nullptr };
    std::mutex frame_mutex; // to protect updating and reading of m_d3dCurrentFrame
    winrt::com_ptr<ID3D11Texture2D> m_d3dCurrentFrame{ nullptr };
    winrt::Windows::Graphics::DirectX::DirectXPixelFormat m_pixelFormat = winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized;
    bool m_closed = false;
    winrt::event_token m_frameArrivedToken;
    RECT m_bounds = { 0 };
    RECT m_croppedBounds = { 0 };;
    RECT m_captureBounds = { 0 };
    unsigned long long m_frameId = 0;
    double m_frameTime = 0;
    HANDLE m_event = NULL;
    bool m_saveBitmap = false;
    std::vector<double> m_arrivalTimes;
    double m_firstFrameTime = 0;

public:
    void StartCapture(
        winrt::IDirect3DDevice const& device,
        winrt::GraphicsCaptureItem const& item,
        winrt::DirectXPixelFormat pixelFormat,
        RECT bounds,
        bool captureCursor)
    {
        m_frameId = 0;
        m_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        m_item = item;
        m_device = device;
        m_pixelFormat = pixelFormat;
        m_bounds = bounds;
        m_croppedBounds = bounds;
        m_captureBounds = bounds;

        auto width = m_bounds.right - m_bounds.left;
        auto height = m_bounds.bottom - m_bounds.top;
        uint32_t size = width * height * CHANNELS;

        m_d3dDevice = util::GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
        m_d3dDevice->GetImmediateContext(m_d3dContext.put());

        // Creating our frame pool with 'Create' instead of 'CreateFreeThreaded'
        // means that the frame pool's FrameArrived event is called on the thread
        // the frame pool was created on. This also means that the creating thread
        // must have a DispatcherQueue. If you use this method, it's best not to do
        // it on the UI thread.
        m_framePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(m_device, m_pixelFormat, 2, m_item.Size());
        m_session = m_framePool.CreateCaptureSession(m_item);
        if (!m_session.IsSupported())
        {
            throw std::exception("CreateCaptureSession is not supported on this version of Windows.\n");
        }
        m_session.IsCursorCaptureEnabled(captureCursor);

        auto session3 = m_session.try_as<winrt::Windows::Graphics::Capture::IGraphicsCaptureSession3>();
        if (session3) {
            session3.IsBorderRequired(false);
        }
        else
        {
            printf("Cannot disable the capture border on this version of windows\n");
        }

        m_frameArrivedToken = m_framePool.FrameArrived({ this, &SimpleCaptureImpl::OnFrameArrived });

        m_session.StartCapture();
    }

    bool WaitForNextFrame(uint32_t timeout)
    {
        auto hr = WaitForMultipleObjects(1, &m_event, TRUE, timeout);
        return hr == 0;
    }


    void Close()
    {
        if (!m_closed)
        {
            m_framePool.FrameArrived(m_frameArrivedToken); // Remove the handler
            CriticalSectionGuard guard;
            m_closed = true;
            m_d3dCurrentFrame = nullptr;
            m_session.Close();
            m_framePool.Close();
            m_framePool = nullptr;
            m_session = nullptr;
            m_item = nullptr;
            CloseHandle(m_event);
        }
    }
    double ReadNextFrame(uint32_t timeout, char* buffer, unsigned int size)
    {
        if (m_closed) {
            debug_hresult(L"ReadNextFrame: Capture is closed", E_FAIL, true);
        }
        // make sure a frame has been written.
        int hr = WaitForMultipleObjects(1, &m_event, TRUE, timeout);
        if (hr == WAIT_TIMEOUT) {
            printf("timeout waiting for FrameArrived event\n");
            return 0;
        }
        double frameTime = 0;
        winrt::com_ptr<ID3D11Texture2D> frame;
        {
            std::scoped_lock lock(frame_mutex);
            frame = m_d3dCurrentFrame;
            frameTime = m_frameTime;
        }

        if (frame != nullptr) {
            ReadPixels(frame.get(), buffer, size);
        }
        else {
            return 0;
        }

        return frameTime;
    }

    double ReadNextTexture(uint32_t timeout, winrt::com_ptr<ID3D11Texture2D>& result)
    {
        if (m_closed) {
            debug_hresult(L"ReadNextFrame: Capture is closed", E_FAIL, true);
            return -1;
        }

        // wait for next frame
        int hr = WaitForMultipleObjects(1, &m_event, TRUE, timeout);
        if (hr == WAIT_TIMEOUT) {
            printf("timeout waiting for FrameArrived event\n");
            return -1;
        }
        double frameTime = 0;
        {
            std::scoped_lock lock(frame_mutex);
            result = m_d3dCurrentFrame;;
            frameTime = m_frameTime;
        }

        return frameTime;
    }


    RECT GetCaptureBounds()
    {
        // to get the proper CPU mapped memory bounds we need to call ReadPixels at least once.
        WaitForNextFrame(10000);
        winrt::com_ptr<ID3D11Texture2D> frame;
        {
            std::scoped_lock lock(frame_mutex);
            frame = m_d3dCurrentFrame;
        }
        if (frame != nullptr) {
            ReadPixels(frame.get(), nullptr, 0);
        }
        else {
            throw std::exception("No frames are arriving");
        }
        return m_captureBounds;
    }

    inline int32_t Clamp(int32_t x, int32_t min, int32_t max) {
        if (x < min) x = min;
        if (x > max) x = max;
        return x;
    }

    void OnFrameArrived(winrt::Direct3D11CaptureFramePool const& sender, winrt::IInspectable const&)
    {
        // mutex ensures we don't try and shut down this class while it is in the middle of handling a frame.
        CriticalSectionGuard guard;
        {
            if (m_closed) {
                return;
            }
        }

        {
            auto frame = sender.TryGetNextFrame();
            auto _systemFrameTime = frame.SystemRelativeTime();
            auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(_systemFrameTime);
            auto frameTime = static_cast<double>(nanoseconds.count() / 1e9);
            if (m_firstFrameTime == 0) {
                m_firstFrameTime = frameTime;
            }
            frameTime -= m_firstFrameTime;
            m_arrivalTimes.push_back(frameTime);

            auto frameSize = frame.ContentSize();

            // get the d3d surface.
            auto sourceTexture = util::GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

            D3D11_TEXTURE2D_DESC desc;
            sourceTexture->GetDesc(&desc);
            auto width = Clamp(frameSize.Width, 0, desc.Width);
            auto height = Clamp(frameSize.Height, 0, desc.Height);
            D3D11_BOX srcBox = {
                (UINT)Clamp((int32_t)m_bounds.left,0,  (UINT)width),
                (UINT)Clamp((int32_t)m_bounds.top, 0, (UINT)height),
                0,
                (UINT)Clamp((int32_t)m_bounds.right, 0, (UINT)width),
                (UINT)Clamp((int32_t)m_bounds.bottom, 0, (UINT)height),
                1
            };

            // Then we need to crop by using CopySubresourceRegion
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;
            desc.Width = srcBox.right - srcBox.left;
            desc.Height = srcBox.bottom - srcBox.top;

            m_croppedBounds.left = 0;
            m_croppedBounds.top = 0;
            m_croppedBounds.right = srcBox.right - srcBox.left;
            m_croppedBounds.bottom = srcBox.bottom - srcBox.top;
            m_captureBounds = m_croppedBounds;

            winrt::com_ptr<ID3D11Texture2D> croppedTexture;
            int hr = m_d3dDevice->CreateTexture2D(&desc, NULL, croppedTexture.put());
            debug_hresult(L"CreateTexture2D", hr, true);

            winrt::com_ptr<ID3D11DeviceContext> immediate;
            m_d3dDevice->GetImmediateContext(immediate.put());
            immediate->CopySubresourceRegion(croppedTexture.get(), 0, 0, 0, 0, sourceTexture.get(), 0, &srcBox);

            {
                std::scoped_lock lock(frame_mutex);
                m_d3dCurrentFrame = croppedTexture;
                m_frameTime = frameTime;
            }
        }

        SetEvent(m_event);
    }

    void SaveBitmap(UCHAR* pixels, D3D11_TEXTURE2D_DESC& desc, int stride) {

        winrt::com_ptr<IWICImagingFactory> wicFactory;
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(wicFactory),
            wicFactory.put_void());
        if (FAILED(hr)) {
            printf("Failed to create instance of WICImagingFactory\n");
            return;
        }

        winrt::com_ptr<IWICBitmapEncoder> wicEncoder;
        hr = wicFactory->CreateEncoder(
            GUID_ContainerFormatBmp,
            nullptr,
            wicEncoder.put());
        if (FAILED(hr)) {
            printf("Failed to create BMP encoder\n");
            return;
        }

        winrt::com_ptr<IWICStream> wicStream;
        hr = wicFactory->CreateStream(wicStream.put());
        if (FAILED(hr)) {
            printf("Failed to create IWICStream");
            return;
        }

        hr = wicStream->InitializeFromFilename(L"d:\\temp\\test.bmp", GENERIC_WRITE);
        if (FAILED(hr)) {
            printf("Failed to initialize stream from file name\n");
            return;
        }

        hr = wicEncoder->Initialize(wicStream.get(), WICBitmapEncoderNoCache);
        if (FAILED(hr)) {
            printf("Failed to initialize bitmap encoder");
            return;
        }

        // Encode and commit the frame
        {
            winrt::com_ptr<IWICBitmapFrameEncode> frameEncode;
            wicEncoder->CreateNewFrame(frameEncode.put(), nullptr);
            if (FAILED(hr)) {
                printf("Failed to create IWICBitmapFrameEncode\n");
                return;
            }

            hr = frameEncode->Initialize(nullptr);
            if (FAILED(hr)) {
                printf("Failed to initialize IWICBitmapFrameEncode\n");
                return;
            }

            GUID wicFormatGuid = GUID_WICPixelFormat32bppBGRA;

            hr = frameEncode->SetPixelFormat(&wicFormatGuid);
            if (FAILED(hr)) {
                printf("SetPixelFormat failed.\n");
                return;
            }

            hr = frameEncode->SetSize(desc.Width, desc.Height);
            if (FAILED(hr)) {
                printf("SetSize(...) failed.\n");
                return;
            }

            hr = frameEncode->WritePixels(
                desc.Height,
                stride,
                desc.Height * stride,
                reinterpret_cast<BYTE*>(pixels));
            if (FAILED(hr)) {
                printf("frameEncode->WritePixels(...) failed.\n");
            }

            hr = frameEncode->Commit();
            if (FAILED(hr)) {
                printf("Failed to commit frameEncode\n");
                return;
            }
        }

        hr = wicEncoder->Commit();
        if (FAILED(hr)) {
            printf("Failed to commit encoder\n");
            return;
        }
    }

    void ReadPixels(ID3D11Texture2D* texture, char* buffer, unsigned int size) {
        // Copy GPU Resource to CPU
        D3D11_TEXTURE2D_DESC desc{};
        winrt::com_ptr<ID3D11Texture2D> copiedImage;

        texture->GetDesc(&desc);
        if (desc.SampleDesc.Count != 1) {
            throw std::exception("SampleDesc.Count != 1\n");
        }
        if (desc.MipLevels != 1) {
            throw std::exception("MipLevels != 1\n");
        }
        if (desc.ArraySize != 1) {
            throw std::exception("ArraySize != 1\n");
        }

        desc.Usage = D3D11_USAGE_STAGING; // A resource that supports data transfer (copy) from the GPU to the CPU.
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
            throw std::exception("Format != DXGI_FORMAT_B8G8R8A8_UNORM\n");
        }

        HRESULT hr = m_d3dDevice->CreateTexture2D(&desc, NULL, copiedImage.put());
        if (hr != S_OK) {
            debug_hresult(L"failed to create texture", hr, true);
        }

        // Copy the image out of the backbuffer.
        m_d3dContext->CopyResource(copiedImage.get(), texture);

        D3D11_MAPPED_SUBRESOURCE resource{};
        UINT subresource = D3D11CalcSubresource(0 /* slice */, 0 /* array slice */, 1 /* mip levels */); //  desc.MipLevels);
        m_d3dContext->ResolveSubresource(copiedImage.get(), subresource, texture, subresource, desc.Format);
        hr = m_d3dContext->Map(copiedImage.get(), subresource, D3D11_MAP_READ, 0, &resource);
        if (hr != S_OK) {
            debug_hresult(L"failed to map texture", hr, true);
        }

        UINT rowPitch = resource.RowPitch;
        unsigned int captureSize = rowPitch * desc.Height;

        if (m_saveBitmap) {
            SaveBitmap(reinterpret_cast<UCHAR*>(resource.pData), desc, rowPitch);
        }

        int x = m_croppedBounds.left;
        int y = m_croppedBounds.top;
        int w = (m_croppedBounds.right - m_croppedBounds.left);
        int h = m_croppedBounds.bottom - m_croppedBounds.top;
        if (rowPitch != w * 4)
        {
            // ResolveSubresource returns a buffer that is 8 byte aligned (64 bit).
            // Record this in the m_captureBounds so the caller can adjust their buffer accordingly.
            m_captureBounds.right = rowPitch / 4;
        }

        if (buffer) {
            char* ptr = buffer;
            char* src = reinterpret_cast<char*>(resource.pData);
            ::memcpy(ptr, src, min(size, captureSize));
        }

        m_d3dContext->Unmap(copiedImage.get(), subresource);
    }
};

SimpleCapture::SimpleCapture()
{
	m_pimpl = std::make_unique<SimpleCaptureImpl>();
    InitializeCriticalSection(&m_mutex);
}

SimpleCapture::~SimpleCapture()
{
    m_pimpl->Close();
}

RECT SimpleCapture::GetCaptureBounds()
{
	return m_pimpl->GetCaptureBounds();
}

RECT SimpleCapture::GetTextureBounds()
{
    return m_pimpl->m_croppedBounds;
}

std::vector<double> SimpleCapture::GetCaptureTimes()
{
    return m_pimpl->m_arrivalTimes;
}

void SimpleCapture::ReadPixels(ID3D11Texture2D* texture, char* buffer, unsigned int size)
{
    m_pimpl->ReadPixels(texture, buffer, size);
}

void SimpleCapture::StartCapture(
    winrt::IDirect3DDevice const& device,
    winrt::GraphicsCaptureItem const& item,
    winrt::DirectXPixelFormat pixelFormat,
    RECT bounds,
    bool captureCursor)
{
	m_pimpl->StartCapture(device, item, pixelFormat, bounds, captureCursor);
}

bool SimpleCapture::WaitForNextFrame(uint32_t timeout)
{
    return m_pimpl->WaitForNextFrame(timeout);
}

double SimpleCapture::ReadNextFrame(uint32_t timeout, char* buffer, unsigned int size)
{
	return m_pimpl->ReadNextFrame(timeout, buffer, size);
}

double SimpleCapture::ReadNextTexture(uint32_t timeout, winrt::com_ptr<ID3D11Texture2D>& result)
{
	return m_pimpl->ReadNextTexture(timeout, result);
}
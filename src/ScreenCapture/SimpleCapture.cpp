#include "pch.h"
#include "SimpleCapture.h"
#include "Errors.h"

#include <winrt/Windows.Graphics.Capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.capture.h>
#include <winrt/windows.graphics.directx.direct3d11.h>
#include <windows.ui.composition.interop.h>
#include <d2d1_1.h>
#include <dxgi1_6.h>

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

    inline auto CreateDXGISwapChain(winrt::com_ptr<ID3D11Device> const& device, const DXGI_SWAP_CHAIN_DESC1* desc)
    {
        auto dxgiDevice = device.as<IDXGIDevice2>();
        winrt::com_ptr<IDXGIAdapter> adapter;
        winrt::check_hresult(dxgiDevice->GetParent(winrt::guid_of<IDXGIAdapter>(), adapter.put_void()));
        winrt::com_ptr<IDXGIFactory2> factory;
        winrt::check_hresult(adapter->GetParent(winrt::guid_of<IDXGIFactory2>(), factory.put_void()));

        winrt::com_ptr<IDXGISwapChain1> swapchain;
        winrt::check_hresult(factory->CreateSwapChainForComposition(device.get(), desc, nullptr, swapchain.put()));
        return swapchain;
    }

    inline auto CreateDXGISwapChain(winrt::com_ptr<ID3D11Device> const& device,
        uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t bufferCount)
    {
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.BufferCount = bufferCount;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

        return CreateDXGISwapChain(device, &desc);
    }

    inline auto CreateCompositionSurfaceForSwapChain(winrt::Windows::UI::Composition::Compositor const& compositor, ::IUnknown* swapChain)
    {
        winrt::Windows::UI::Composition::ICompositionSurface surface{ nullptr };
        auto compositorInterop = compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>();
        winrt::com_ptr<ABI::Windows::UI::Composition::ICompositionSurface> surfaceInterop;
        winrt::check_hresult(compositorInterop->CreateCompositionSurfaceForSwapChain(swapChain, surfaceInterop.put()));
        winrt::check_hresult(surfaceInterop->QueryInterface(winrt::guid_of<winrt::Windows::UI::Composition::ICompositionSurface>(), winrt::put_abi(surface)));
        return surface;
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

SimpleCapture::SimpleCapture()
{
    InitializeCriticalSection(&m_mutex);
}

int SimpleCapture::StartCapture(
    winrt::IDirect3DDevice const& device,
    winrt::GraphicsCaptureItem const& item,
    winrt::DirectXPixelFormat pixelFormat,
    RECT bounds,
    bool captureCursor)
{
    m_frameId = 0;
    m_buffer = nullptr;
    m_size = 0;
    m_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_item = item;
    m_device = device;
    m_pixelFormat = pixelFormat;
    m_bounds = bounds;
    m_timer.start();

    auto width = m_bounds.right - m_bounds.left;
    auto height = m_bounds.bottom - m_bounds.top;
    uint32_t size = width * height * CHANNELS;

    m_d3dDevice = util::GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
    m_d3dDevice->GetImmediateContext(m_d3dContext.put());

    m_swapChain = util::CreateDXGISwapChain(m_d3dDevice, static_cast<uint32_t>(m_item.Size().Width), static_cast<uint32_t>(m_item.Size().Height),
        static_cast<DXGI_FORMAT>(m_pixelFormat), 2);

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

    m_lastSize = m_item.Size();
    m_frameArrivedToken = m_framePool.FrameArrived({ this, &SimpleCapture::OnFrameArrived });

    m_session.StartCapture();
    return size;
}


unsigned long long SimpleCapture::ReadNextFrame(char* buffer, unsigned int size)
{
    m_buffer = buffer;
    m_size = size;
    // make sure a frame has been written.
    int hr = WaitForMultipleObjects(1, &m_event, TRUE, 10000);
    if (hr == WAIT_TIMEOUT) {
        printf("timeout waiting for FrameArrived event\n");
        return 0;
    }
    return m_frameTime;
}

void SimpleCapture::Close()
{
    if (!m_closed)
    {
        CriticalSectionGuard guard;
        m_closed = true;
        m_size = 0;
        m_buffer = nullptr;
        m_framePool.FrameArrived(m_frameArrivedToken); // Remove the handler
        m_session.Close();
        m_framePool.Close();
        m_swapChain = nullptr;
        m_framePool = nullptr;
        m_session = nullptr;
        m_item = nullptr;
        CloseHandle(m_event);
    }
}

void SimpleCapture::ResizeSwapChain()
{
    winrt::check_hresult(m_swapChain->ResizeBuffers(2, static_cast<uint32_t>(m_lastSize.Width), static_cast<uint32_t>(m_lastSize.Height),
        static_cast<DXGI_FORMAT>(m_pixelFormat), 0));
}

bool SimpleCapture::TryResizeSwapChain(winrt::Direct3D11CaptureFrame const& frame)
{
    auto const contentSize = frame.ContentSize();
    if ((contentSize.Width != m_lastSize.Width) ||
        (contentSize.Height != m_lastSize.Height))
    {
        // The thing we have been capturing has changed size, resize the swap chain to match.
        m_lastSize = contentSize;
        ResizeSwapChain();
        return true;
    }
    return false;
}

void SimpleCapture::OnFrameArrived(winrt::Direct3D11CaptureFramePool const& sender, winrt::IInspectable const&)
{
    // mutex ensures we don't try and shut down this class while it is in the middle of handling a frame.
    CriticalSectionGuard guard;
    {
        if (m_closed || m_size == 0) {
            return;
        }
    }

    auto swapChainResizedToFrame = false;
    {
        auto frame = sender.TryGetNextFrame();
        swapChainResizedToFrame = TryResizeSwapChain(frame);

        winrt::com_ptr<ID3D11Texture2D> backBuffer;
        winrt::check_hresult(m_swapChain->GetBuffer(0, winrt::guid_of<ID3D11Texture2D>(), backBuffer.put_void()));

        auto surfaceTexture = util::GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
        m_d3dContext->CopyResource(backBuffer.get(), surfaceTexture.get());

        ReadPixels(backBuffer.get());
    }

    if (swapChainResizedToFrame)
    {
        m_framePool.Recreate(m_device, m_pixelFormat, 2, m_lastSize);
    }
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

void SimpleCapture::ReadPixels(ID3D11Texture2D* acquiredDesktopImage) {
    // Copy GPU Resource to CPU
    D3D11_TEXTURE2D_DESC desc{};
    winrt::com_ptr<ID3D11Texture2D> copiedImage;

    acquiredDesktopImage->GetDesc(&desc);
    if (desc.SampleDesc.Count != 1) {
        printf("SampleDesc.Count != 1\n");
        return;
    }
    if (desc.MipLevels != 1) {
        printf("MipLevels != 1\n");
        return;
    }
    if (desc.ArraySize != 1) {
        printf("ArraySize != 1\n");
        return;
    }

    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
        printf("Format != DXGI_FORMAT_B8G8R8A8_UNORM\n");
        return;
    }

    HRESULT hr = m_d3dDevice->CreateTexture2D(&desc, NULL, copiedImage.put());
    if (hr != S_OK) {
        printf("failed to create texture\n");
        return;
    }

    m_frameTime = static_cast<unsigned long long>(m_timer.microseconds());
    m_d3dContext->CopyResource(copiedImage.get(), acquiredDesktopImage);

    D3D11_MAPPED_SUBRESOURCE resource{};
    UINT subresource = D3D11CalcSubresource(0 /* slice */, 0 /* array slice */, 1 /* mip levels */); //  desc.MipLevels);
    m_d3dContext->ResolveSubresource(copiedImage.get(), subresource, acquiredDesktopImage, subresource, desc.Format);

    hr = m_d3dContext->Map(copiedImage.get(), subresource, D3D11_MAP_READ, 0, &resource);
    if (hr != S_OK) {
        printf("failed to map texture\n");
        return;
    }

    UINT lBmpRowPitch = resource.RowPitch;
    unsigned int captureSize = lBmpRowPitch * desc.Height;

    if (m_saveBitmap) {
        SaveBitmap(reinterpret_cast<UCHAR*>(resource.pData), desc, lBmpRowPitch);
    }

    if (m_buffer) {
        char* ptr = m_buffer;
        char* src = reinterpret_cast<char*>(resource.pData);
        int x = m_bounds.left;
        int y = m_bounds.top;
        int w = (m_bounds.right - m_bounds.left);
        int h = m_bounds.bottom - m_bounds.top;

        // Handle full screen with a single memcpy, technically it can 
        // handle any height so long as it is full width and starting at top left.
        if (x == 0 && y == 0 && w == desc.Width) 
        {
			::memcpy(ptr, src, min(m_size, captureSize));
		}
        else 
        {
            // Copy the cropped image out of the full monitor frame.
            auto expectedSize = w * h * CHANNELS;
            if (expectedSize != m_size) {
                printf("buffer too small\n");
                return;
            }

            int actualHeight = (int)desc.Height;
            int xBytes = x * CHANNELS;
            int targetRowBytes = w * CHANNELS;
            int srcRowBytes = desc.Width * CHANNELS;
            if (xBytes + targetRowBytes > srcRowBytes) {
                targetRowBytes = srcRowBytes - xBytes;
            }
            src += (lBmpRowPitch * y); // skip to top row.
            for (int row = y; row < actualHeight && row < y + h; row++) {
                ::memcpy(ptr, src + xBytes, targetRowBytes);
                src += lBmpRowPitch;
                ptr += targetRowBytes;
            }
        }

        SetEvent(m_event);
    }

    m_d3dContext->Unmap(copiedImage.get(), subresource);
}

#pragma once
#include <mutex>

class SimpleCaptureImpl;

class SimpleCapture
{
public:
    __declspec(dllexport) SimpleCapture();
    __declspec(dllexport) ~SimpleCapture();

    __declspec(dllexport) void StartCapture(
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& device,
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat pixelFormat,
        RECT bounds,
        bool captureCursor);

    __declspec(dllexport) bool WaitForNextFrame(uint32_t timeout);

    // When copying GPU to CPU data the row pitch can be 64 or 128 bit aligned, which can mean
    // you need to pass in a slightly bigger buffer. This ensures ReadNextFrame can do a single
    // optimized memcpy operation but it also means it is up to you to crop the final image
    // to remove that extra data on the right side of each row.
    __declspec(dllexport) RECT GetCaptureBounds();

    __declspec(dllexport) double ReadNextFrame(uint32_t timeout, char* buffer, unsigned int size);

    __declspec(dllexport) RECT GetTextureBounds();
    __declspec(dllexport) double ReadNextTexture(uint32_t timeout, winrt::com_ptr<ID3D11Texture2D>& result);

    __declspec(dllexport) std::vector<double> GetCaptureTimes();

private:
    std::unique_ptr<SimpleCaptureImpl> m_pimpl;
};
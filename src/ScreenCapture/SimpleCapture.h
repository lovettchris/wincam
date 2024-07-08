#pragma once
#include "Timer.h"
#include <mutex>

class SimpleCapture
{
public:
    SimpleCapture();
    ~SimpleCapture() { Close(); }

    unsigned int StartCapture(
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& device,
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat pixelFormat,
        RECT bounds);

    void Close();

    unsigned long long ReadNextFrame(char* buffer, unsigned int size);

private:
    void OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

    void ResizeSwapChain();
    bool TryResizeSwapChain(winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame const& frame);

private:
    void ReadPixels(ID3D11Texture2D* buffer);

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };
    winrt::Windows::Graphics::SizeInt32 m_lastSize = { 0 };

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device{ nullptr };
    winrt::com_ptr<IDXGISwapChain1> m_swapChain{ nullptr };
    winrt::com_ptr<ID3D11Device> m_d3dDevice{ nullptr };
    winrt::com_ptr<ID3D11DeviceContext> m_d3dContext{ nullptr };
    winrt::Windows::Graphics::DirectX::DirectXPixelFormat m_pixelFormat;

    bool m_closed = false;
    winrt::event_token m_frameArrivedToken;
    util::Timer m_timer;
    RECT m_bounds = { 0 };
    unsigned long long m_frameId = 0;
    unsigned long long m_frameTime = 0;
    char* m_buffer = nullptr;
    unsigned int m_size = 0;
    HANDLE m_event = NULL;
    bool m_saveBitmap = false;
};
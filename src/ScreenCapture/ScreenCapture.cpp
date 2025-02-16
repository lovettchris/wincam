// ScreenCapture.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <pch.h>
#include <iostream>
#include <stdio.h>
#include <mutex>

#include <winrt/Windows.Graphics.Capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.capture.h>
#include <winrt/windows.graphics.directx.direct3d11.h>
#include "SimpleCapture.h"
#include "Errors.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Metadata;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::Graphics::Imaging;
    using namespace Windows::System;
}

extern "C"
{
    HRESULT __stdcall CreateDirect3D11DeviceFromDXGIDevice(::IDXGIDevice* dxgiDevice,
        ::IInspectable** graphicsDevice);

}

inline auto CreateCaptureItemForMonitor(HMONITOR hmon)
{
    auto interop_factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item = { nullptr };
    int hr = interop_factory->CreateForMonitor(hmon, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(item));
    debug_hresult(L"CreateForMonitor", hr);
    return item;
}

typedef int CaptureHandle;

class EnumInfo
{
public:
    HMONITOR hmon;
    int x;
    int y;
    int w;
    int h;
    bool verbose;
};

inline EnumInfo FindMonitor(int x, int y, int width, int height, bool verbose) {

    EnumInfo result{NULL, x, y, width, height, verbose};
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hmon, HDC, LPRECT, LPARAM lparam)
        {
            EnumInfo* info = (EnumInfo*)lparam;
            MONITORINFOEX monitorInfo = { sizeof(monitorInfo) };
            GetMonitorInfo(hmon, &monitorInfo);
            if (monitorInfo.rcMonitor.left <= info->x && monitorInfo.rcMonitor.right >= info->x + info->w &&
                monitorInfo.rcMonitor.top <= info->y && monitorInfo.rcMonitor.bottom >= info->y + info->h)
			{
				info->hmon = hmon;
                // make x,y relative to monitor location.
                info->x -= monitorInfo.rcMonitor.left;
                info->y -= monitorInfo.rcMonitor.top;
				return FALSE; // stop enumerating.
			}
            if (info->verbose) {
                printf("Found monitor at (%d, %d) size (%d x %d)\n",
                    monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                    monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                    monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top);
            }
            return TRUE;
        }, (LPARAM)& result);

    return result;
}

inline auto CreateDirect3DDevice(IDXGIDevice* dxgi_device)
{
    winrt::com_ptr<::IInspectable> d3d_device;
    int hr = CreateDirect3D11DeviceFromDXGIDevice(dxgi_device, d3d_device.put());
    debug_hresult(L"CreateDirect3D11DeviceFromDXGIDevice", hr);
    return d3d_device.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
}

std::mutex m_list_lock;
std::vector<std::shared_ptr<SimpleCapture>> m_captures;

std::shared_ptr<SimpleCapture> get_capture(CaptureHandle h)
{
    std::shared_ptr<SimpleCapture> ptr;
    std::scoped_lock lock(m_list_lock);
    if (h >= 0 && h < m_captures.size()) {
        ptr = m_captures[h];
    }
    return ptr;
}

std::shared_ptr<SimpleCapture> remove_capture(CaptureHandle h)
{
    std::shared_ptr<SimpleCapture> ptr;
    std::scoped_lock lock(m_list_lock);
    if (h >= 0 && h < m_captures.size()) {
        ptr = m_captures[h];
        m_captures[h] = nullptr;
        // trim the end of the list to remove any null pointers.
        while (m_captures.size() > 0) {
            int last = m_captures.size() - 1;
            if (m_captures[last] == nullptr) {
                m_captures.pop_back();
            }
            else 
            {
                break;
            }
        }
    }
    return ptr;
}

CaptureHandle add_capture(std::shared_ptr<SimpleCapture> capture)
{
    std::scoped_lock lock(m_list_lock);
    std::shared_ptr<SimpleCapture> ptr;
    for (int i = 0; i < m_captures.size(); i++) {
        if (m_captures[i] == nullptr) {
            m_captures[i] = capture;
            return i;
        }
    }
    m_captures.push_back(capture);
    return (CaptureHandle)(m_captures.size() - 1);
}

extern "C" {
    void __declspec(dllexport) __stdcall StopCapture(CaptureHandle h)
    {
        std::shared_ptr<SimpleCapture> ptr = remove_capture(h);
        if (ptr != nullptr) {
            ptr->Close();
        }
    }

    unsigned long long __declspec(dllexport) __stdcall ReadNextFrame(CaptureHandle h, char* buffer, unsigned int size)
    {
        std::shared_ptr<SimpleCapture> ptr = get_capture(h);
        if (ptr != nullptr) {
            return ptr->ReadNextFrame(buffer, size);
        }
        return 0;
    }
    
    bool  __declspec(dllexport) __stdcall WaitForFirstFrame(CaptureHandle h, int timeout)
    {
        std::shared_ptr<SimpleCapture> ptr = get_capture(h);
        if (ptr != nullptr) {
            return ptr->WaitForFirstFrame(timeout);
        }
        return false;
    }

    RECT  __declspec(dllexport) __stdcall GetCaptureBounds(CaptureHandle h)
    {
        std::shared_ptr<SimpleCapture> ptr = get_capture(h);
        if (ptr != nullptr) {

            return ptr->GetCaptureBounds();
        }
        return RECT{};
    }

    CaptureHandle __declspec(dllexport) __stdcall StartCapture(int x, int y, int width, int height, bool captureCursor)
    {
        try {
            auto mon = FindMonitor(x, y, width, height, false);
            if (mon.hmon == nullptr)
            {
                printf("Monitor not found that fully contains the bounds (%d, %d) (%d x %d)\n", x, y, width, height);
                FindMonitor(x, y, width, height, true);
                debug_hresult(L"Monitor not found", E_FAIL, true);
            }
            
            winrt::com_ptr<ID3D11Device> d3dDevice;
            HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, d3dDevice.put(), nullptr, nullptr);
            debug_hresult(L"D3D11CreateDevice", hr);

            auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
            auto device = CreateDirect3DDevice(dxgiDevice.get());

            winrt::GraphicsCaptureItem item{ nullptr };
            item = CreateCaptureItemForMonitor(mon.hmon);

            RECT bounds = { mon.x, mon.y, mon.x + width, mon.y + height };
            auto capture = std::make_shared<SimpleCapture>();
            capture->StartCapture(device, item, winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, bounds, captureCursor);            
            return add_capture(capture);
        }
        catch (winrt::hresult_error const& ex) {
            int hr = (int)(ex.code());
            debug_hresult(ex.message().c_str(), hr, true);
        }
        catch (std::exception const& se) {
            std::wstring msg = to_utf16(se.what());
            debug_hresult(msg.c_str(), E_FAIL, true);
        }
        return -1;
    }
}
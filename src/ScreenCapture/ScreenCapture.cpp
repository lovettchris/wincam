// ScreenCapture.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <pch.h>
#include <iostream>
#include <stdio.h>

#include <winrt/Windows.Graphics.Capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.capture.h>
#include <winrt/windows.graphics.directx.direct3d11.h>
#include "SimpleCapture.h"

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
    winrt::check_hresult(interop_factory->CreateForMonitor(hmon, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(item)));
    return item;
}

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
                info->x -= monitorInfo.rcMonitor.left;
                info->y -= monitorInfo.rcMonitor.top;
				return FALSE;
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
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi_device, d3d_device.put()));
    return d3d_device.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
}

std::unique_ptr<SimpleCapture> m_capture;

int ERROR_MONITOR_NOT_FOUND = -1;

extern "C" {
    void __declspec(dllexport) __stdcall StopCapture()
    {
        if (m_capture) {
            m_capture->Close();
            m_capture = NULL;
        }
    }

    unsigned long long __declspec(dllexport) __stdcall ReadNextFrame(char* buffer, unsigned int size)
    {
        if (m_capture) {
            return m_capture->ReadNextFrame(buffer, size);
        }
        return 0;
    }

    int __declspec(dllexport) __stdcall StartCapture(int x, int y, int width, int height, bool captureCursor)
    {
        auto mon = FindMonitor(x, y, width, height, false);
        if (mon.hmon == nullptr)
        {
            printf("Monitor not found that fully contains the bounds (%d, %d) (%d x %d)\n", x, y, width, height);
            FindMonitor(x, y, width, height, true);
            return ERROR_MONITOR_NOT_FOUND;
        }

        try {
            StopCapture();
            winrt::com_ptr<ID3D11Device> d3dDevice;
            HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, d3dDevice.put(), nullptr, nullptr);
            winrt::check_hresult(hr);

            auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
            auto device = CreateDirect3DDevice(dxgiDevice.get());

            winrt::GraphicsCaptureItem item{ nullptr };
            item = CreateCaptureItemForMonitor(mon.hmon);

            RECT bounds = { mon.x, mon.y, mon.x + width, mon.y + height };
            m_capture = std::make_unique<SimpleCapture>();
            return m_capture->StartCapture(device, item, winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, bounds, captureCursor);

        }
        catch (winrt::hresult_error const& ex) {
            int hr = (int)(ex.code());
            printf("Capture failed %d\n", hr);
            return hr;
        }
        return 0;
    }
}
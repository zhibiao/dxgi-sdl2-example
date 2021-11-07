#include <iostream>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <gdiplus.h>
#include "SDL.h"

ID3D11Device *m_hDevice = NULL;
ID3D11DeviceContext *m_hContext = NULL;
IDXGIOutputDuplication *m_hDeskDupl = NULL;
ID3D11Texture2D *hAcquiredDesktopImage = NULL;
DXGI_OUTPUT_DESC m_dxgiOutDesc;
UINT WIDTH = 1920, HEIGHT = 1080;
BYTE *frameDataBuffer = new BYTE[WIDTH * HEIGHT * 4];
UINT frameCount = 0;
UINT frameDataSize = 0;

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;

HRESULT DXGI_Setup()
{
    HRESULT hr = S_OK;
    D3D_DRIVER_TYPE DriverTypes[] =
        {
            D3D_DRIVER_TYPE_HARDWARE,
            D3D_DRIVER_TYPE_WARP,
            D3D_DRIVER_TYPE_REFERENCE,
        };
    UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

    // Feature levels supported
    D3D_FEATURE_LEVEL FeatureLevels[] =
        {
            D3D_FEATURE_LEVEL_9_1,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_12_1};
    UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);

    D3D_FEATURE_LEVEL featureLevel;

    //
    for (UINT DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex)
    {
        hr = D3D11CreateDevice(NULL, DriverTypes[DriverTypeIndex], NULL, 0, FeatureLevels, NumFeatureLevels, D3D11_SDK_VERSION, &m_hDevice, &featureLevel, &m_hContext);
        if (SUCCEEDED(hr))
        {
            break;
        }
    }
    if (FAILED(hr))
    {
        return hr;
    }

    //
    // Get DXGI device
    //
    IDXGIDevice *hDxgiDevice = NULL;
    hr = m_hDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&hDxgiDevice));
    if (FAILED(hr))
    {
        return hr;
    }

    //
    // Get DXGI adapter
    //
    IDXGIAdapter *hDxgiAdapter = NULL;
    hr = hDxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void **>(&hDxgiAdapter));
    hDxgiDevice->Release();
    hDxgiDevice = NULL;
    if (FAILED(hr))
    {
        return hr;
    }

    //
    // Get output
    //
    INT nOutput = 0;
    IDXGIOutput *hDxgiOutput = NULL;
    hr = hDxgiAdapter->EnumOutputs(nOutput, &hDxgiOutput);
    hDxgiAdapter->Release();
    hDxgiAdapter = NULL;
    if (FAILED(hr))
    {
        return hr;
    }

    //
    // get output description struct
    //
    hDxgiOutput->GetDesc(&m_dxgiOutDesc);

    //
    // QI for Output 1
    //
    IDXGIOutput1 *hDxgiOutput1 = NULL;
    hr = hDxgiOutput->QueryInterface(__uuidof(hDxgiOutput1), reinterpret_cast<void **>(&hDxgiOutput1));
    hDxgiOutput->Release();
    hDxgiOutput = NULL;
    if (FAILED(hr))
    {
        return hr;
    }

    //
    // Create desktop duplication
    //
    hr = hDxgiOutput1->DuplicateOutput(m_hDevice, &m_hDeskDupl);
    hDxgiOutput1->Release();
    hDxgiOutput1 = NULL;
    if (FAILED(hr))
    {
        return hr;
    }

    return hr;
}

HRESULT DXGI_AcquireFrame()
{
    HRESULT hr = S_OK;
    IDXGIResource *hDesktopResource = NULL;
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;

    hr = m_hDeskDupl->AcquireNextFrame(0, &FrameInfo, &hDesktopResource);

    if (FAILED(hr))
    {
        //std::cout << "---AcquireNextFrame--Failed--\n";
        if ((hr != DXGI_ERROR_ACCESS_LOST) && (hr != DXGI_ERROR_WAIT_TIMEOUT))
        {
        }
        return hr;
    }

    // If still holding old frame, destroy it
    if (hAcquiredDesktopImage)
    {
        hAcquiredDesktopImage->Release();
        hAcquiredDesktopImage = NULL;
    }

    hr = hDesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&hAcquiredDesktopImage));
    hDesktopResource->Release();
    hDesktopResource = NULL;
    if (FAILED(hr))
    {
        std::cout << "Failed to QI for ID3D11Texture2D from acquired IDXGIResource in DUPLICATIONMANAGER" << std::endl;
        return hr;
    }

    //std::cout << "Get metadata size:" << FrameInfo.TotalMetadataBufferSize << std::endl;
    return hr;
}

HRESULT DXGI_CopyFrame()
{
    HRESULT hr = S_OK;
    D3D11_TEXTURE2D_DESC frameDescriptor;
    ID3D11Texture2D *hNewDesktopImage = NULL;

    hAcquiredDesktopImage->GetDesc(&frameDescriptor);
    frameDescriptor.Usage = D3D11_USAGE_STAGING;
    frameDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    frameDescriptor.BindFlags = 0;
    frameDescriptor.MiscFlags = 0;
    frameDescriptor.MipLevels = 1;
    frameDescriptor.ArraySize = 1;
    frameDescriptor.SampleDesc.Count = 1;
    hr = m_hDevice->CreateTexture2D(&frameDescriptor, NULL, &hNewDesktopImage);

    if (FAILED(hr))
    {
        hAcquiredDesktopImage->Release();
        hAcquiredDesktopImage = NULL;
        return hr;
    }

    m_hContext->CopyResource(hNewDesktopImage, hAcquiredDesktopImage);
    hAcquiredDesktopImage->Release();
    hAcquiredDesktopImage = NULL;
    m_hDeskDupl->ReleaseFrame();

    IDXGISurface *hStagingSurf = NULL;
    hr = hNewDesktopImage->QueryInterface(__uuidof(IDXGISurface), (void **)(&hStagingSurf));
    hNewDesktopImage->Release();
    hNewDesktopImage = NULL;
    if (FAILED(hr))
    {
        return hr;
    }

    DXGI_MAPPED_RECT mappedRect;
    hr = hStagingSurf->Map(&mappedRect, DXGI_MAP_READ);
    if (SUCCEEDED(hr))
    {
        //std::cout << frameCount++ << "-----" << m_dxgiOutDesc.DesktopCoordinates.right << "-----" << m_dxgiOutDesc.DesktopCoordinates.bottom << "-----" << mappedRect.Pitch << std::endl;
        frameDataSize = m_dxgiOutDesc.DesktopCoordinates.right * m_dxgiOutDesc.DesktopCoordinates.bottom * 4;
        memcpy(frameDataBuffer, mappedRect.pBits, frameDataSize);
        hStagingSurf->Unmap();
    }

    hStagingSurf->Release();
    hStagingSurf = NULL;
    return hr;
}

void Init_SDL_Window()
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
    window = SDL_CreateWindow("SDL_WINDOW_TITLE", 1000, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, 0);
    renderer = SDL_CreateRenderer(window, -1, 0);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
}

int main(int argc, char *argv[])
{
    Init_SDL_Window();
    DXGI_Setup();
    BOOL done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event) > 0)
        {
            if (event.type == SDL_QUIT)
            {
                done = 1;
            }

            if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    done = 1;
                }
            }
        }

        if (SUCCEEDED(DXGI_AcquireFrame()))
        {
            if (SUCCEEDED(DXGI_CopyFrame()))
            {
                SDL_Rect rect;
                rect.x = 0;
                rect.y = 0;
                rect.w = WIDTH;
                rect.h = HEIGHT;
                SDL_UpdateTexture(texture, &rect, frameDataBuffer, WIDTH * 4);
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, &rect);
                SDL_RenderPresent(renderer);
            }
        }
    }

    return 0;
}
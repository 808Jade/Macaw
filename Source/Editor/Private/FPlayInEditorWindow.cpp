/**
 * @file FPlayInEditorWindow.cpp
 * @brief 별도 PIE 창과 스왑 체인·게임 뷰포트 및 입력 캡처를 관리합니다.
 * @ingroup Editor
 */

#include "FPlayInEditorWindow.h"
#include "Engine/FGameViewportClient.h"
#include "Engine/FSceneViewport.h"
#include "Engine/FRenderTarget.h"
#include "RenderCore/IRendererModule.h"

namespace Editor {

    FPlayInEditorWindow::FPlayInEditorWindow() = default;

    FPlayInEditorWindow::~FPlayInEditorWindow() {
        Shutdown();
    }

    bool FPlayInEditorWindow::Initialize(RenderCore::IRendererModule* Renderer, Engine::FGameViewportClient* Client, Uint32 Width, Uint32 Height, bool CaptureInput) {
        if (Renderer == nullptr || Client == nullptr || mViewport != nullptr) {
            return false;
        }

        mRenderer = Renderer;
        mPlatform.SetQuitOnClose(false);

        if (!mPlatform.Create(L"Macaw - Play in Editor | Shift+F1: Release input | Esc: Stop", Width, Height, false)) {
            return false;
        }

        mPlatform.SetDarkMode(true);
        mPlatform.SetMinimumClientSize(320, 180);
        mPlatform.SetCenteredClientSize(Width, Height);
        mPlatform.SetMessageHandler([this](HWND, UINT Message, WPARAM, LPARAM) {
            if (Message == WM_CLOSE) {
                mCloseRequested = true;

                return true;
            }

            return false;
        });

        auto* Device{static_cast<ID3D11Device*>(Renderer->GetNativeDevice())};
        Microsoft::WRL::ComPtr<IDXGIDevice> GraphicsDevice{};
        Microsoft::WRL::ComPtr<IDXGIAdapter> Adapter{};
        Microsoft::WRL::ComPtr<IDXGIFactory> Factory{};

        if (Device == nullptr || FAILED(Device->QueryInterface(IID_PPV_ARGS(GraphicsDevice.GetAddressOf()))) || FAILED(GraphicsDevice->GetAdapter(Adapter.GetAddressOf())) || FAILED(Adapter->GetParent(IID_PPV_ARGS(Factory.GetAddressOf())))) {
            Shutdown();

            return false;
        }

        DXGI_SWAP_CHAIN_DESC Description{};
        Description.BufferDesc.Width = mPlatform.GetWidth();
        Description.BufferDesc.Height = mPlatform.GetHeight();
        Description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        Description.SampleDesc.Count = 1;
        Description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        Description.BufferCount = 2;
        Description.OutputWindow = mPlatform.GetWindowHandle();
        Description.Windowed = TRUE;
        Description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        mViewport = std::make_unique<Engine::FSceneViewport>(Renderer, Client);

        if (FAILED(Factory->CreateSwapChain(Device, &Description, mSwapChain.GetAddressOf())) || !mViewport->Resize(Description.BufferDesc.Width, Description.BufferDesc.Height)) {
            Shutdown();

            return false;
        }

        Factory->MakeWindowAssociation(mPlatform.GetWindowHandle(), DXGI_MWA_NO_ALT_ENTER);
        mWidth = Description.BufferDesc.Width;
        mHeight = Description.BufferDesc.Height;
        mInputCaptured = CaptureInput;
        mPlatform.Show();
        mPlatform.Activate();

        return true;
    }

    void FPlayInEditorWindow::Shutdown() {
        mViewport.reset();
        mSwapChain.Reset();
        mPlatform.Shutdown();
        mRenderer = nullptr;
        mInputCaptured = false;
    }

    bool FPlayInEditorWindow::Draw() {
        const Uint32 Width{mPlatform.GetWidth()};
        const Uint32 Height{mPlatform.GetHeight()};

        mPlatform.GetInput().BeginFrame();

        if (!IsOpen() || mViewport == nullptr || mSwapChain == nullptr) {
            return false;
        }

        if (Width == 0 || Height == 0) {
            return true;
        }

        auto* Context{static_cast<ID3D11DeviceContext*>(mRenderer->GetNativeContext())};

        Context->OMSetRenderTargets(0, nullptr, nullptr);

        if (Width != mWidth || Height != mHeight) {
            if (FAILED(mSwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, 0))) {
                return false;
            }

            mWidth = Width;
            mHeight = Height;
        }

        if (!mViewport->Resize(Width, Height)) {
            return false;
        }

        mViewport->Draw();
        Context->OMSetRenderTargets(0, nullptr, nullptr);

        auto* Texture{static_cast<ID3D11ShaderResourceView*>(mViewport->GetRenderTarget()->GetNativeTexture())};
        Microsoft::WRL::ComPtr<ID3D11Resource> Source{};
        Microsoft::WRL::ComPtr<ID3D11Texture2D> Destination{};

        Texture->GetResource(Source.GetAddressOf());

        if (FAILED(mSwapChain->GetBuffer(0, IID_PPV_ARGS(Destination.GetAddressOf())))) {
            return false;
        }

        Context->CopyResource(Destination.Get(), Source.Get());

        return SUCCEEDED(mSwapChain->Present(0, 0));
    }

    bool FPlayInEditorWindow::IsOpen() const {
        return mPlatform.IsRunning() && !mCloseRequested;
    }

    bool FPlayInEditorWindow::IsInputCaptured() {
        const Core::FInput& Input{mPlatform.GetInput()};

        if (!Input.IsFocused() || (Input.IsDown(Core::EKey::Shift) && Input.WasPressed(Core::EKey::F1))) {
            mInputCaptured = false;
        } else if (Input.WasPressed(Core::EKey::MouseLeft)) {
            mInputCaptured = true;
        }

        if (mInputCaptured) {
            const RECT Bounds{0, 0, static_cast<LONG>(mPlatform.GetWidth()), static_cast<LONG>(mPlatform.GetHeight())};
            mInputCaptured = mPlatform.CaptureMouse(Bounds);
        } else {
            mPlatform.ReleaseMouseCapture();
        }

        return mInputCaptured;
    }

    Core::FInput& FPlayInEditorWindow::GetInput() {
        return mPlatform.GetInput();
    }

    void FPlayInEditorWindow::ReleaseInput() {
        mInputCaptured = false;
        mPlatform.ReleaseMouseCapture();
    }

}

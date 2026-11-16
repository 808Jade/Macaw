#include "Platform/Window.h"

#include <d3d11.h>
#include <dwmapi.h>
#include <wrl/client.h>
#include <iostream>
#include <string_view>

namespace {

    int Failures{};
    int Checks{};

    void Expect(bool Condition, std::string_view Name) {
        ++Checks;

        if (!Condition) {
            ++Failures;
            std::cerr << "FAIL: " << Name << '\n';
        }
    }

    bool IsCloaked(const Platform::FWindowsApplication& Application) {
        DWORD Cloaked{};

        return SUCCEEDED(DwmGetWindowAttribute(Application.GetWindowHandle(), DWMWA_CLOAKED, &Cloaked, sizeof(Cloaked))) && (Cloaked & DWM_CLOAKED_APP) != 0;
    }

    bool PresentFrame(Platform::FWindowsApplication& Application) {
        DXGI_SWAP_CHAIN_DESC Description{};
        Description.BufferDesc.Width = Application.GetWidth();
        Description.BufferDesc.Height = Application.GetHeight();
        Description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        Description.SampleDesc.Count = 1;
        Description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        Description.BufferCount = 2;
        Description.OutputWindow = Application.GetWindowHandle();
        Description.Windowed = true;
        Description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        Microsoft::WRL::ComPtr<ID3D11Device> Device{};
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> Context{};
        Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain{};

        if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &Description, &SwapChain, &Device, nullptr, &Context))) {
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> Buffer{};
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> View{};

        if (FAILED(SwapChain->GetBuffer(0, IID_PPV_ARGS(&Buffer))) || FAILED(Device->CreateRenderTargetView(Buffer.Get(), nullptr, &View))) {
            return false;
        }

        constexpr float Color[]{0.04f, 0.10f, 0.16f, 1.0f};
        Context->ClearRenderTargetView(View.Get(), Color);
        const HRESULT Result{SwapChain->Present(1, 0)};
        Context->Flush();
        Expect(Result == S_OK, "a cloaked flip-model window accepts its first rendered frame");

        return Result == S_OK;
    }

    void CheckPresentationBarrier() {
        Platform::FWindowsApplication Application{};
        Expect(Application.Create(L"Macaw Window Presentation Tests", 320, 200, false), "create a hidden window");
        Expect(!IsWindowVisible(Application.GetWindowHandle()), "creation does not expose an unprepared surface");
        const auto ReadyFrame{[]() {
            return true;
        }};
        Expect(!Application.RevealAfterFrame(ReadyFrame), "a presentation must begin before it can be committed");
        Expect(Application.BeginPresentation(), "begin the first presentation");
        Expect(Application.BeginPresentation(), "a pending transition can be reused after loading fails");
        Application.Restore();
        Application.SetCenteredClientSize(320, 200);
        Application.Show();
        DwmFlush();
        Expect(IsCloaked(Application), "show and restore preserve presentation suppression");
        Expect(!Application.RevealAfterFrame({}), "a missing frame presenter cannot expose the window");
        int Calls{};
        Expect(!Application.RevealAfterFrame([&Calls]() {
            ++Calls;

            return false;
        }), "a skipped or failed frame cannot expose the window");
        Expect(Calls == 1 && IsCloaked(Application), "the failed presenter runs while the window remains cloaked");
        Expect(!Application.RevealAfterFrame([&Application]() {
            Application.SetCenteredClientSize(480, 280);

            return true;
        }), "resizing during frame preparation invalidates that frame");
        Expect(IsCloaked(Application), "a frame for an obsolete size remains hidden");
        Expect(!Application.RevealAfterFrame([&Application]() {
            Application.Hide();

            return true;
        }), "a presenter that hides the window cannot commit a presentation");
        Expect(IsCloaked(Application), "hiding during preparation preserves suppression");
        Application.Minimize();
        Expect(!Application.RevealAfterFrame([&Calls]() {
            ++Calls;

            return true;
        }), "a minimized window cannot be revealed");
        Expect(Calls == 1, "a minimized window skips the frame presenter");
        Application.Restore();
        Expect(!Application.RevealAfterFrame([&Application]() {
            Application.Minimize();

            return true;
        }), "minimizing during frame preparation prevents committing that frame");
        Expect(IsCloaked(Application), "minimizing during preparation keeps the old surface suppressed");
        Application.Restore();
        Application.Maximize();
        DwmFlush();
        Expect(IsCloaked(Application), "maximizing does not expose the previous screen");
        Application.Restore();
        Application.SetCenteredClientSize(320, 200);
        bool HiddenDuringFrame{};
        Expect(Application.RevealAfterFrame([&Application, &HiddenDuringFrame]() {
            HiddenDuringFrame = IsCloaked(Application);

            return PresentFrame(Application);
        }), "a rendered and presented frame can expose the window");
        Expect(HiddenDuringFrame, "the first frame is rendered before the window becomes visible");
        Expect(!Application.IsPresentationPending() && !IsCloaked(Application), "a successful presentation releases suppression");
        Expect(Application.BeginPresentation(), "begin another role on the same native window");
        Application.Restore();
        const auto MissingFrame{[]() {
            return false;
        }};
        Expect(!Application.RevealAfterFrame(MissingFrame), "the previous role's frame cannot satisfy a new transition");
        Expect(IsCloaked(Application), "the previous role stays hidden until the next role renders");
        Expect(!Application.RevealAfterFrame([&Application]() {
            Application.Shutdown();

            return true;
        }), "closing during preparation cannot commit a presentation");
        Expect(!Application.IsRunning(), "closing is reflected in the application state");
        Application.PumpMessages();
    }

    void CheckPlatformOwnedState() {
        Platform::FWindowsApplication Application{};
        Expect(Application.Create(L"Macaw Window State Tests", 320, 200, false), "create another window after a closed transition");
        bool Destroyed{};
        Application.SetMessageHandler([&Application, &Destroyed](HWND, UINT Message, WPARAM, LPARAM) {
            if (Message == WM_DESTROY) {
                Destroyed = true;
                Expect(!Application.IsRunning(), "destruction state is updated before notifying an external handler");
            }

            return true;
        });
        Application.SetCenteredClientSize(480, 280);
        RECT Bounds{};
        GetClientRect(Application.GetWindowHandle(), &Bounds);
        Expect(Application.GetWidth() == static_cast<Uint32>(Bounds.right) && Application.GetHeight() == static_cast<Uint32>(Bounds.bottom), "external message handling cannot swallow size state updates");
        Application.SetMinimumClientSize(400, 240);
        MINMAXINFO Limits{};
        SendMessageW(Application.GetWindowHandle(), WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&Limits));
        Expect(Limits.ptMinTrackSize.x >= 400 && Limits.ptMinTrackSize.y >= 240, "the platform owns minimum window sizing");
        DestroyWindow(Application.GetWindowHandle());
        Expect(Destroyed, "external handlers still receive the window destruction notification");
        Expect(!Application.IsRunning() && !Application.BeginPresentation(), "a destroyed window cannot begin a transition");
        Application.Shutdown();
        Application.PumpMessages();
    }
}

int main() {
    CheckPresentationBarrier();
    CheckPlatformOwnedState();
    std::cout << Checks << " checks, " << Failures << " failures\n";

    return Failures == 0 ? 0 : 1;
}

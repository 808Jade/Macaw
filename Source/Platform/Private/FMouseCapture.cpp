/**
 * @file FMouseCapture.cpp
 * @brief Win32 마우스 캡처·커서 제한·숨김과 상대 입력의 등록 및 복원을 관리합니다.
 * @ingroup Platform
 */

#include "FMouseCapture.h"

using namespace Platform;

FMouseCapture::FMouseCapture() = default;

FMouseCapture::~FMouseCapture() {
    Release();
}

bool FMouseCapture::Capture(HWND Window, const RECT& Bounds) {
    RECT Client{};
    RECT Clipped{};

    if (Window == nullptr || GetForegroundWindow() != Window || GetFocus() != Window || !IsWindowVisible(Window) || IsIconic(Window) || !GetClientRect(Window, &Client) || !IntersectRect(&Clipped, &Bounds, &Client)) {
        Release();
        return false;
    }

    POINT Minimum{Clipped.left, Clipped.top};
    POINT Maximum{Clipped.right, Clipped.bottom};

    if (!ClientToScreen(Window, &Minimum) || !ClientToScreen(Window, &Maximum)) {
        Release();
        return false;
    }

    if (!mCaptured) {
        mWindow = Window;
        GetCursorPos(&mRestorePosition);
        mRestoreCursor = GetCursor();
        SetCapture(Window);

        if (GetCapture() != Window) {
            mWindow = nullptr;
            return false;
        }

        mCaptured = true;
        const RAWINPUTDEVICE Mouse{0x01, 0x02, 0, Window};

        if (!RegisterRawInputDevices(&Mouse, 1, sizeof(Mouse))) {
            Release();
            return false;
        }

        mRawInputRegistered = true;

        do {
            ++mHideCount;
        } while (ShowCursor(FALSE) >= 0);
    }

    const RECT ScreenBounds{Minimum.x, Minimum.y, Maximum.x, Maximum.y};
    const POINT Center{Minimum.x + (Maximum.x - Minimum.x) / 2, Minimum.y + (Maximum.y - Minimum.y) / 2};

    if (!ClipCursor(&ScreenBounds) || !SetCursorPos(Center.x, Center.y)) {
        Release();
        return false;
    }

    SetCursor(nullptr);
    return true;
}

void FMouseCapture::Release() {
    if (!mCaptured) {
        return;
    }

    const bool Restore{GetForegroundWindow() == mWindow && IsWindowVisible(mWindow) && !IsIconic(mWindow)};
    mCaptured = false;

    if (mRawInputRegistered) {
        const RAWINPUTDEVICE Mouse{0x01, 0x02, RIDEV_REMOVE, nullptr};
        RegisterRawInputDevices(&Mouse, 1, sizeof(Mouse));
        mRawInputRegistered = false;
    }

    ClipCursor(nullptr);

    if (GetCapture() == mWindow) {
        ReleaseCapture();
    }

    while (mHideCount > 0) {
        ShowCursor(TRUE);
        --mHideCount;
    }

    if (Restore) {
        SetCursor(mRestoreCursor);
        SetCursorPos(mRestorePosition.x, mRestorePosition.y);
    }

    mWindow = nullptr;
}

bool FMouseCapture::IsCaptured() const {
    return mCaptured && GetCapture() == mWindow;
}

void FMouseCapture::ReadInput(LPARAM Handle, Core::FInput& Input) {
    if (!IsCaptured() || !Input.IsFocused()) {
        return;
    }

    RAWINPUT Data{};
    UINT Size{sizeof(Data)};

    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(Handle), RID_INPUT, &Data, &Size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1) || Data.header.dwType != RIM_TYPEMOUSE || (Data.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0) {
        return;
    }

    Input.AddMouseDelta({static_cast<float>(Data.data.mouse.lLastX), static_cast<float>(Data.data.mouse.lLastY)});
}

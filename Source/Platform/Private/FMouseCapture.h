/**
 * @file FMouseCapture.h
 * @brief Win32 마우스 캡처·커서 제한·숨김과 상대 입력의 등록 및 복원을 관리합니다.
 * @ingroup Platform
 */

#pragma once

#include "Core/Input.h"
#include <Windows.h>

namespace Platform {
    /**
     * @brief Win32 마우스 캡처·커서 제한·숨김과 상대 입력의 등록 및 복원을 관리합니다.
     */
    class FMouseCapture {
    public:
        /**
         * @brief FMouseCapture의 초기 상태와 전달받은 의존성을 구성합니다.
         */
        FMouseCapture();

        /**
         * @brief 객체를 소멸하고 소유 멤버와 연결 상태를 정리합니다.
         */
        ~FMouseCapture();

        /**
         * @brief 중복된 수명·연결 상태를 방지하기 위해 복사 생성을 금지합니다.
         */
        FMouseCapture(const Platform::FMouseCapture&) = delete;

        /**
         * @brief 중복된 수명·연결 상태를 방지하기 위해 대입을 금지합니다.
         */
        Platform::FMouseCapture& operator=(const Platform::FMouseCapture&) = delete;

    public:
        /**
         * @brief 활성 창의 지정 영역에 커서를 제한하고 숨김·상대 입력 캡처를 시작합니다.
         * @param[in] Window 마우스 캡처 또는 메시지 처리 대상 Win32 창입니다.
         * @param[in] Bounds 클라이언트 좌표 기준 대상 사각형입니다.
         * @return Win32 캡처와 원시 입력 등록 및 커서 제한에 성공하면 true입니다.
         */
        bool Capture(HWND Window, const RECT& Bounds);

        /**
         * @brief 원시 입력과 커서 제한·캡처를 해제하고 가능한 경우 이전 커서 상태를 복원합니다.
         */
        void Release();

        /**
         * @brief 객체와 Win32의 마우스 캡처 상태가 모두 유효한지 검사합니다.
         * @return 현재 창이 캡처를 유지하고 있으면 true입니다.
         */
        bool IsCaptured() const;

        /**
         * @brief 캡처 중인 WM_INPUT 상대 마우스 이동을 프레임 입력에 누적합니다.
         * @param[in] Handle WM_INPUT 메시지의 원시 입력 핸들입니다.
         * @param[in] Input 이번 프레임의 키·마우스 입력 상태입니다.
         */
        void ReadInput(LPARAM Handle, Core::FInput& Input);

    private:
        /**
         * @brief 마우스를 캡처한 Win32 창 핸들입니다.
         */
        HWND mWindow{};

        /**
         * @brief 캡처 해제 시 복원할 원래 화면 커서 위치입니다.
         */
        POINT mRestorePosition{};

        /**
         * @brief 캡처 해제 시 복원할 원래 커서 핸들입니다.
         */
        HCURSOR mRestoreCursor{};

        /**
         * @brief 캡처 중 호출한 커서 숨김 횟수로 해제 시 같은 횟수만큼 복원합니다.
         */
        Int32 mHideCount{};

        /**
         * @brief 마우스 캡처와 커서 복원 상태를 보관 중인지 나타냅니다.
         */
        bool mCaptured{};

        /**
         * @brief 캡처용 원시 마우스 입력을 등록했는지 나타냅니다.
         */
        bool mRawInputRegistered{};
    };
}

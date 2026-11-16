/**
 * @file Window.h
 * @brief Win32 창과 메시지 루프를 관리합니다.
 * @ingroup Platform
 */

#pragma once

#include "Core/ReflectionMacros.h"
#include "Core/Input.h"
#include "Core/Types.h"

#include <Windows.h>
#include <functional>
#include <memory>
#include <string_view>

namespace Platform {

    class FMouseCapture;

    /**
     * @brief Win32 창 생성, 메시지 펌프와 클라이언트 영역 상태를 관리합니다.
     * @ingroup Platform
     */
    class FWindowsApplication {
    public:
        /**
         * @brief 처리한 Win32 메시지이면 true를 반환하는 외부 메시지 콜백입니다.
         */
        using FMessageHandler = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;

        /**
         * @brief 첫 프레임을 그려 표시하고 성공 여부를 반환하는 콜백입니다.
         */
        using FFramePresenter = std::function<bool()>;

    public:
        /**
         * @brief FWindowsApplication의 초기 상태와 전달받은 의존성을 구성합니다.
         */
        FWindowsApplication();

        /**
         * @brief 객체의 수명을 끝내고 소유한 리소스를 정리합니다.
         */
        ~FWindowsApplication();

        /**
         * @brief 레지스트리에 등록된 고유 객체이므로 복사·이동 생성을 금지합니다.
         */
        FWindowsApplication(const Platform::FWindowsApplication&) = delete;

        /**
         * @brief 고유한 객체 수명을 유지하기 위해 대입을 금지합니다.
         * @return 갱신된 자기 자신(*this)의 참조입니다.
         */
        Platform::FWindowsApplication& operator=(const Platform::FWindowsApplication&) = delete;

    public:
        /**
         * @brief 지정한 제목과 클라이언트 크기로 Win32 창을 생성합니다.
         * @param[in] Title 창 제목입니다.
         * @param[in] Width 창 클라이언트 영역의 가로 픽셀 수입니다.
         * @param[in] Height 창 클라이언트 영역의 세로 픽셀 수입니다.
         * @param[in] Visible 창이나 패널을 표시할지 지정합니다.
         * @return 조건을 만족하거나 작업이 성공하면 true, 그렇지 않으면 false입니다.
         */
        bool Create(const wchar_t* Title, Uint32 Width, Uint32 Height, bool Visible = true);

        /**
         * @brief 대기 중인 Windows 메시지를 처리하고 실행 지속 여부를 반환합니다.
         * @return 조건을 만족하거나 작업이 성공하면 true, 그렇지 않으면 false입니다.
         */
        bool PumpMessages();

        /**
         * @brief 종속 객체를 역순으로 정리하고 여러 번 호출해도 안전한 종료 상태로 만듭니다.
         */
        void Shutdown();

        /**
         * @brief Win32 창을 표시합니다.
         */
        void Show();

        /**
         * @brief Win32 창을 숨깁니다.
         */
        void Hide();

        /**
         * @brief Win32 창을 최소화·최대화 이전의 보통 크기로 복원합니다.
         */
        void Restore();

        /**
         * @brief Win32 창을 최소화합니다.
         */
        void Minimize();

        /**
         * @brief Win32 창을 최대화합니다.
         */
        void Maximize();

        /**
         * @brief Win32 창을 전면으로 가져오고 활성화를 요청합니다.
         */
        void Activate();

        /**
         * @brief Win32 창의 제목을 변경합니다.
         * @param[in] Title 표시할 창 또는 드로어 제목입니다.
         */
        void SetTitle(std::wstring_view Title);

        /**
         * @brief Win32 창 프레임의 어두운 테마 사용 여부를 설정합니다.
         * @param[in] Enabled 해당 기능의 활성 여부입니다.
         */
        void SetDarkMode(bool Enabled);

        /**
         * @brief 에디터가 그리는 사용자 지정 제목 표시줄의 사용 여부를 설정합니다.
         * @param[in] Enabled 해당 기능의 활성 여부입니다.
         */
        void SetCustomTitleBar(bool Enabled);

        /**
         * @brief 사용자 지정 제목 표시줄의 창 끌기 영역을 지정합니다.
         * @param[in] Bounds 클라이언트 좌표 기준 대상 사각형입니다.
         */
        void SetTitleBarDragBounds(const RECT& Bounds);

        /**
         * @brief 클라이언트 크기를 바꾸고 창을 화면 작업 영역 중앙에 배치합니다.
         * @param[in] Width 대상 영역의 너비이며 단위는 픽셀입니다.
         * @param[in] Height 대상 영역의 높이이며 단위는 픽셀입니다.
         */
        void SetCenteredClientSize(Uint32 Width, Uint32 Height);

        /**
         * @brief 창 크기 조절 시 허용할 최소 클라이언트 크기를 지정합니다.
         * @param[in] Width 대상 영역의 너비이며 단위는 픽셀입니다.
         * @param[in] Height 대상 영역의 높이이며 단위는 픽셀입니다.
         */
        void SetMinimumClientSize(Uint32 Width, Uint32 Height);

        /**
         * @brief 창을 닫을 때 메시지 루프 종료를 요청할지 지정합니다.
         * @param[in] QuitOnClose 창을 닫을 때 종료 메시지를 보낼지 지정합니다.
         */
        void SetQuitOnClose(bool QuitOnClose);

        /**
         * @brief 클라이언트 영역 내 지정한 범위에 게임 마우스 입력을 캡처합니다.
         * @param[in] Bounds 클라이언트 좌표 기준 대상 사각형입니다.
         * @return 마우스 캡처에 성공하면 true입니다.
         */
        bool CaptureMouse(const RECT& Bounds);

        /**
         * @brief 게임 마우스 입력 캡처를 해제하고 커서 상태를 복원합니다.
         */
        void ReleaseMouseCapture();

        /**
         * @brief 창의 마우스 입력 캡처 상태를 조회합니다.
         * @return 유효한 캡처를 유지하고 있으면 true입니다.
         */
        bool IsMouseCaptured() const;

        /**
         * @brief 첫 프레임 표시를 위한 창 프레젠테이션을 준비합니다.
         * @return 프레젠테이션을 준비할 수 있으면 true입니다.
         */
        bool BeginPresentation();

        /**
         * @brief 프레임을 표시한 뒤 창 공개를 완료합니다.
         * @param[in] Presenter 첫 프레임을 그려 표시하고 성공 여부를 반환하는 콜백입니다.
         * @return 프레임 표시와 창 공개를 완료했으면 true입니다.
         */
        bool RevealAfterFrame(const FFramePresenter& Presenter);

        /**
         * @brief 첫 프레임 이후 창 공개가 아직 대기 중인지 조회합니다.
         * @return 공개 대기 중이면 true입니다.
         */
        bool IsPresentationPending() const;

        /**
         * @brief 창의 메시지 루프가 실행 중인지 조회합니다.
         * @return 실행 중이면 true입니다.
         */
        bool IsRunning() const;

        /**
         * @brief 메시지가 도착하거나 지정한 대기 시간이 지날 때까지 기다립니다.
         * @param[in] TimeoutMilliseconds 메시지 대기의 최대 시간이며 단위는 밀리초입니다.
         */
        void WaitForMessages(Uint32 TimeoutMilliseconds) const;

        /**
         * @brief 기본 창 처리 전에 호출할 외부 메시지 핸들러를 교체합니다.
         * @param[in] Handler UI 백엔드 등 외부 메시지 처리 콜백입니다.
         */
        void SetMessageHandler(FMessageHandler Handler);

        /**
         * @brief 플랫폼 메시지로 갱신하는 공통 입력 상태를 반환합니다.
         * @return 플랫폼 메시지로 갱신하는 공통 입력 상태입니다.
         */
        Core::FInput& GetInput();

        /**
         * @brief 플랫폼 메시지로 갱신하는 공통 입력 상태를 반환합니다.
         * @return 플랫폼 메시지로 갱신하는 공통 입력 상태입니다.
         */
        const Core::FInput& GetInput() const;

        /**
         * @brief 네이티브 Win32 창 핸들을 반환합니다.
         * @return 네이티브 Win32 창 핸들입니다.
         */
        HWND GetWindowHandle() const;

        /**
         * @brief 가로 픽셀 수를 반환합니다.
         * @return 가로 픽셀 수입니다.
         */
        Uint32 GetWidth() const;

        /**
         * @brief 세로 픽셀 수를 반환합니다.
         * @return 세로 픽셀 수입니다.
         */
        Uint32 GetHeight() const;

        /**
         * @brief 창의 클라이언트 영역이 최소화된 크기인지 검사합니다.
         * @return 조건을 만족하거나 작업이 성공하면 true, 그렇지 않으면 false입니다.
         */
        bool IsMinimized() const;

        /**
         * @brief 창 또는 뷰포트가 최대화 상태인지 검사합니다.
         * @return 최대화 상태이면 true입니다.
         */
        bool IsMaximized() const;

    private:
        /**
         * @brief 화면 좌표를 사용자 지정 프레임의 끌기·크기 조절 영역으로 판정합니다.
         * @param[in] Position 화면 좌표가 패킹된 Win32 위치 값입니다.
         * @return Win32 히트 테스트 코드입니다.
         */
        LRESULT HitTestCustomFrame(LPARAM Position) const;

        /**
         * @brief Win32 입력·포커스 메시지를 공통 입력 상태에 반영합니다.
         * @param[in] Window 메시지를 받은 Win32 창입니다.
         * @param[in] Message 처리할 Win32 메시지 코드입니다.
         * @param[in] WParam Win32 메시지의 키 코드·부가 값입니다.
         * @param[in] LParam Win32 메시지의 추가 상태·좌표 정보입니다.
         */
        void ProcessInputMessage(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);

        /**
         * @brief 창 메시지를 인스턴스에 전달하고 크기 및 종료 상태를 갱신합니다.
         * @param[in] Window 메시지를 받은 Win32 창입니다.
         * @param[in] Message Windows 메시지 번호입니다.
         * @param[in] WParam Windows 메시지의 WPARAM 인수입니다.
         * @param[in] LParam Windows 메시지의 LPARAM 인수입니다.
         * @return Windows에 돌려줄 메시지 처리 결과입니다.
         */
        static LRESULT CALLBACK WindowProcedure(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);

    private:
        /**
         * @brief Win32 창 핸들입니다.
         */
        HWND mWindow{};

        /**
         * @brief 가로 픽셀 수입니다.
         */
        Uint32 mWidth{};

        /**
         * @brief 세로 픽셀 수입니다.
         */
        Uint32 mHeight{};

        /**
         * @brief 메시지 루프가 실행 중인지 여부입니다.
         */
        bool mRunning{};

        /**
         * @brief 창을 닫을 때 종료 메시지를 보낼지 나타냅니다.
         */
        bool mQuitOnClose{true};

        /**
         * @brief 첫 프레임 표시 후 창 공개 완료를 기다리는지 나타냅니다.
         */
        bool mPresentationPending{};

        /**
         * @brief 사용자 지정 제목 표시줄 사용 여부입니다.
         */
        bool mCustomTitleBar{};

        /**
         * @brief 창 끌기로 처리할 클라이언트 좌표의 제목 표시줄 영역입니다.
         */
        RECT mTitleBarDragBounds{};

        /**
         * @brief 최소 클라이언트 너비이며 단위는 픽셀입니다.
         */
        Uint32 mMinimumWidth{};

        /**
         * @brief 최소 클라이언트 높이이며 단위는 픽셀입니다.
         */
        Uint32 mMinimumHeight{};

        /**
         * @brief 외부 UI 백엔드에 메시지를 전달하는 콜백입니다.
         */
        FMessageHandler mMessageHandler{};

        /**
         * @brief 플랫폼 메시지 또는 게임 뷰포트에서 사용하는 공통 입력 상태입니다.
         */
        Core::FInput mInput{};

        /**
         * @brief 창의 커서 제한·숨김·원시 입력을 관리하는 소유 객체입니다.
         */
        std::unique_ptr<Platform::FMouseCapture> mMouseCapture{};
    };

}

#pragma once

#include "Core/Types.h"

namespace PhysicsRuntime {
    /**
     * @brief 프레임 시간을 물리 스텝으로 바꾸는 방식입니다.
     */
    enum class EPhysicsTimeStepMode : Uint8 {
        /** 프레임 시간(상한 적용)을 스텝 1회로 사용합니다. */
        Variable,
        /** 프레임 시간을 같은 크기의 스텝 여러 개로 나눕니다. */
        Substep,
        /** 항상 고정 크기로 스텝하고 남는 시간은 누산기에 이월합니다. */
        Fixed,
    };

    /**
     * @brief 물리 시뮬레이션의 시간 진행 방식과 디버그 옵션입니다. 시간 단위는 초입니다.
     */
    struct FPhysicsSettings {
        EPhysicsTimeStepMode mMode{EPhysicsTimeStepMode::Substep};

        /** Variable 모드에서 스텝 1회의 최대 시간입니다. */
        float mMaxFrameDt{1.0f / 30.0f};

        /** Substep 모드에서 스텝 1회의 최대 시간과 프레임당 최대 스텝 수입니다. */
        float mMaxSubstepDt{1.0f / 60.0f};
        Int32 mMaxSubsteps{6};

        /** Fixed 모드의 스텝 크기와 프레임당 최대 스텝 수입니다. */
        float mFixedDt{1.0f / 60.0f};
        Int32 mMaxFixedSteps{6};

        /** Fixed 모드에서 마지막 두 물리 상태를 섞어 그릴지 나타냅니다. 보간은 Transform 동기화 단계에서 사용합니다. */
        bool mInterpolate{true};

        /** 1초마다 시간 진행 통계를 로그로 남깁니다. */
        bool mLogStepStats{false};

        /** 디버그용입니다. 0보다 크면 Advance 호출 간격을 이 시간(ms)에 맞추도록 대기해 프레임을 느리게 만듭니다. */
        float mTargetFrameMs{0.0f};

        /** 디버그용입니다. 목표 프레임 시간에 더해지는 무작위 흔들림의 최대 크기(ms)입니다. */
        float mJitterMs{0.0f};
    };
}

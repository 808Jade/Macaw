/**
 * @file PhysicsStats.cpp
 * @brief 물리 시뮬레이션 처리 시간과 바디·접촉 개수 통계를 정의합니다.
 * @ingroup PhysicsRuntime
 */

#include "Core/Types.h"
#include "PhysicsRuntime/PhysicsStats.h"

namespace PhysicsRuntime {

    namespace PhysicsStats {
        Core::FStat Simulate{"physics", "Simulate CPU (incl. fetch)", Core::EStatUnit::Milliseconds};
        Core::FStat Dispatch{"physics", "Result dispatch CPU", Core::EStatUnit::Milliseconds};
        Core::FStat Steps{"physics", "Simulation steps"};
        Core::FStat MovedBodies{"physics", "Moved bodies"};
        Core::FStat Contacts{"physics", "New contacts"};
        Core::FStat DynamicBodies{"physics", "Dynamic bodies (last scene)", Core::EStatUnit::Count, Core::EStatMode::Gauge};
        Core::FStat StaticBodies{"physics", "Static bodies (last scene)", Core::EStatUnit::Count, Core::EStatMode::Gauge};
        Core::FStat ClothSimulate{"cloth", "Cloth simulate CPU", Core::EStatUnit::Milliseconds};
        Core::FStat ClothParticles{"cloth", "Cloth particles", Core::EStatUnit::Count, Core::EStatMode::Gauge};
    }

}

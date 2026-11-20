#pragma once

#include "PhysicsRuntime/UBodySetupCore.h"
#include "UBodySetup.generated.h"

namespace PhysicsRuntime {
    UCLASS()
    class UBodySetup : public PhysicsRuntime::UBodySetupCore {
        GENERATED_BODY()

    public:
        UBodySetup();
        ~UBodySetup() override;
    };
}

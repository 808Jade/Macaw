#pragma once

#include "CoreUObject/UObject.h"
#include "UBodySetupCore.generated.h"

namespace PhysicsRuntime {
    UCLASS()
    class UBodySetupCore : public CoreUObject::UObject {
        GENERATED_BODY()

    public:
        UBodySetupCore();
        ~UBodySetupCore() override;
    };
}

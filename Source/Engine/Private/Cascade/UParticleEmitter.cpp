/**
 * @file UParticleEmitter.cpp
 * @brief LOD별 모듈 정의와 파티클·인스턴스 페이로드 배치를 소유하는 이미터입니다.
 * @ingroup Engine
 */

#include "Engine/Cascade/UParticleEmitter.h"
#include "Engine/Cascade/FBaseParticle.h"
#include "Engine/Cascade/FParticleEmitterInstance.h"
#include "Engine/Cascade/UParticleLODLevel.h"
#include "Engine/Cascade/UParticleModule.h"
#include "Engine/Cascade/UParticleModuleRequired.h"
#include "Core/Archive.h"
#include "CoreUObject/Reflection/NewObject.h"

#include <limits>
#include <stdexcept>
#include <utility>

using namespace Engine;
using namespace Core;
using namespace CoreUObject;

/**
 * @brief UParticleEmitter 구현에 한정되는 내부 형식·상수·도우미 함수입니다.
 */
namespace {

    /**
     * @brief 파티클 이미터를 읽을 때 허용하는 최대 LOD 수입니다.
     */
    constexpr Uint32 MaximumLODLevels{64};

    /**
     * @brief 페이로드 크기를 다음 16바이트 정렬 경계로 올립니다.
     * @param[in] Size 정렬할 현재 페이로드 바이트 수입니다.
     * @return 16바이트 단위로 정렬된 크기입니다.
     * @throws std::length_error 정렬한 크기가 지원하는 오프셋 범위를 넘는 경우입니다.
     */
    Uint32 AlignPayload(Uint32 Size) {
        constexpr Uint32 MaximumSize{static_cast<Uint32>(std::numeric_limits<Int32>::max())};

        if (Size > MaximumSize - 15) {
            throw std::length_error{"Particle payload exceeds the supported offset range"};
        }

        return (Size + 15) & ~Uint32{15};
    }

    /**
     * @brief 정렬된 페이로드 영역을 배정하고 누적 크기를 늘립니다.
     * @param[in,out] Size 새 영역을 배정한 뒤 갱신할 누적 페이로드 바이트 수입니다.
     * @param[in] RequiredSize 추가로 배정할 페이로드 바이트 수입니다.
     * @return 16바이트 정렬된 추가 영역의 바이트 오프셋입니다.
     * @throws std::length_error 새 영역을 포함한 크기가 지원하는 오프셋 범위를 넘는 경우입니다.
     */
    Index_t AllocatePayload(Uint32& Size, Uint32 RequiredSize) {
        const Index_t Offset{static_cast<Index_t>(AlignPayload(Size))};

        if (RequiredSize > static_cast<Uint32>(std::numeric_limits<Int32>::max()) - Offset) {
            throw std::length_error{"Particle payload exceeds the supported offset range"};
        }

        Size = Offset + RequiredSize;

        return Offset;
    }

}

UParticleEmitter::UParticleEmitter() {
    SetName("Emitter");
    AddLODLevel(NewObject<UParticleLODLevel>());
}

UParticleEmitter::~UParticleEmitter() {
    ClearLODLevels();
}

bool UParticleEmitter::AddLODLevel(std::unique_ptr<UParticleLODLevel> LODLevel) {
    if (LODLevel == nullptr || mLODLevels.size() >= MaximumLODLevels) {
        return false;
    }

    if ((LODLevel->GetOuter() != nullptr && LODLevel->GetOuter() != this) || !LODLevel->SetOuter(this)) {
        return false;
    }

    mLODLevels.push_back(std::move(LODLevel));
    InvalidateBuildData();

    return true;
}

bool UParticleEmitter::RemoveLODLevel(TObjectRef<const UParticleLODLevel> LODLevel) {
    for (auto Iterator{mLODLevels.begin()}; Iterator != mLODLevels.end(); ++Iterator) {
        if (Iterator->get() == LODLevel) {
            (*Iterator)->SetOuter(nullptr);
            mLODLevels.erase(Iterator);
            InvalidateBuildData();

            return true;
        }
    }

    return false;
}

void UParticleEmitter::ClearLODLevels() {
    for (const auto& LODLevel : mLODLevels) {
        LODLevel->SetOuter(nullptr);
    }

    mLODLevels.clear();
    InvalidateBuildData();
}

TObjectRef<UParticleLODLevel> UParticleEmitter::GetLODLevel(Index_t Index) {
    return Index >= 0 && static_cast<std::size_t>(Index) < mLODLevels.size() ? mLODLevels[Index].get() : nullptr;
}

TObjectRef<const UParticleLODLevel> UParticleEmitter::GetLODLevel(Index_t Index) const {
    return Index >= 0 && static_cast<std::size_t>(Index) < mLODLevels.size() ? mLODLevels[Index].get() : nullptr;
}

std::size_t UParticleEmitter::GetLODLevelCount() const {
    return mLODLevels.size();
}

const TArray<std::unique_ptr<UParticleLODLevel>>& UParticleEmitter::GetLODLevels() const {
    return mLODLevels;
}

TArray<TObjectRef<UParticleModule>> UParticleEmitter::GetAllModules() const {
    TArray<TObjectRef<UParticleModule>> Modules{};

    for (const auto& LODLevel : mLODLevels) {
        Modules.push_back(LODLevel->GetRequiredModule());

        const auto LODModules{LODLevel->GetModules()};

        Modules.insert(Modules.end(), LODModules.begin(), LODModules.end());
    }

    return Modules;
}

void UParticleEmitter::Build() {
    for (const auto& LODLevel : mLODLevels) {
        LODLevel->UpdateModuleLists();
    }

    CacheEmitterModuleInfo();
}

void UParticleEmitter::CacheEmitterModuleInfo() {
    ObjectHashMap<TObjectRef<const UParticleModule>, Index_t> ParticleOffsets{};
    ObjectHashMap<TObjectRef<const UParticleModule>, Index_t> InstanceOffsets{};
    Uint32 ParticleSize{static_cast<Uint32>(sizeof(FBaseParticle))};
    Uint32 InstanceSize{};

    for (const auto& Reference : GetAllModules()) {
        UParticleModule* Module{Reference.Get()};
        const Uint32 ParticleBytes{Module->RequiredBytes(nullptr)};
        const Uint32 InstanceBytes{Module->RequiredBytesPerInstance()};

        if (ParticleBytes != 0) {
            ParticleOffsets.emplace(Reference, AllocatePayload(ParticleSize, ParticleBytes));
        }

        if (InstanceBytes != 0) {
            InstanceOffsets.emplace(Reference, AllocatePayload(InstanceSize, InstanceBytes));
        }
    }

    mParticleSize = AlignPayload(ParticleSize);
    mInstancePayloadSize = AlignPayload(InstanceSize);
    mModuleDataOffsets = std::move(ParticleOffsets);
    mModuleInstanceDataOffsets = std::move(InstanceOffsets);
}

std::unique_ptr<FParticleEmitterInstance> UParticleEmitter::CreateInstance(IParticleEmitterInstanceOwner& Owner) {
    auto Instance{std::make_unique<FParticleEmitterInstance>(Owner)};

    Instance->InitParameters(this);
    Instance->Init();

    return Instance;
}

Uint32 UParticleEmitter::GetParticleSize() const {
    return mParticleSize;
}

Uint32 UParticleEmitter::GetInstancePayloadSize() const {
    return mInstancePayloadSize;
}

const ObjectHashMap<TObjectRef<const UParticleModule>, Index_t>& UParticleEmitter::GetModuleDataOffsets() const {
    return mModuleDataOffsets;
}

const ObjectHashMap<TObjectRef<const UParticleModule>, Index_t>& UParticleEmitter::GetModuleInstanceDataOffsets() const {
    return mModuleInstanceDataOffsets;
}

void UParticleEmitter::InvalidateBuildData() {
    mModuleDataOffsets.clear();
    mModuleInstanceDataOffsets.clear();
    mParticleSize = 0;
    mInstancePayloadSize = 0;
}

void UParticleEmitter::Serialize(FArchive& Archive) {
    if (Archive.HasError()) {
        return;
    }

    FString Name{GetName()};

    Archive.Serialize("Name", Name);

    Uint32 Count{static_cast<Uint32>(mLODLevels.size())};

    if (!Archive.BeginArray("LODLevels", Count)) {
        return;
    }

    if (Count > MaximumLODLevels) {
        Archive.SetError("Too many particle LOD levels");
    }

    TArray<std::unique_ptr<UParticleLODLevel>> LODLevels{};

    for (Index_t Index{}; Index < static_cast<Index_t>(Count) && !Archive.HasError(); ++Index) {
        if (!Archive.BeginObject(std::to_string(Index))) {
            break;
        }

        auto Loaded{Archive.IsLoading() ? NewObject<UParticleLODLevel>() : nullptr};
        UParticleLODLevel* LODLevel{Archive.IsLoading() ? Loaded.get() : mLODLevels[Index].get()};

        LODLevel->Serialize(Archive);
        Archive.EndObject();

        if (Archive.IsLoading() && !Archive.HasError()) {
            LODLevels.push_back(std::move(Loaded));
        }
    }

    Archive.EndArray();

    if (Archive.IsLoading() && !Archive.HasError()) {
        UParticleEmitter Candidate{};

        if (!Candidate.SetName(Name)) {
            Archive.SetError("Invalid particle emitter name");

            return;
        }

        Candidate.ClearLODLevels();
        Candidate.mLODLevels = std::move(LODLevels);

        for (const auto& LODLevel : Candidate.mLODLevels) {
            LODLevel->SetOuter(&Candidate);
        }

        try {
            Candidate.Build();
            ClearLODLevels();
            SetName(Name);
            mLODLevels = std::move(Candidate.mLODLevels);
            mModuleDataOffsets = std::move(Candidate.mModuleDataOffsets);
            mModuleInstanceDataOffsets = std::move(Candidate.mModuleInstanceDataOffsets);
            mParticleSize = Candidate.mParticleSize;
            mInstancePayloadSize = Candidate.mInstancePayloadSize;

            for (const auto& LODLevel : mLODLevels) {
                LODLevel->SetOuter(this);
            }
        } catch (const std::exception& Error) {
            Archive.SetError(Error.what());
        }
    }
}

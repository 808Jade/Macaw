/**
 * @file UPhysicsAsset.cpp
 * @brief 물리 에셋에서 뼈 이름과 관절 이름으로 설정을 찾습니다.
 * @ingroup PhysicsRuntime
 */

#include "PhysicsRuntime/UPhysicsAsset.h"

namespace PhysicsRuntime {
    /**
     * @brief 바디와 관절이 없는 빈 에셋을 구성합니다.
     */
    UPhysicsAsset::UPhysicsAsset() = default;

    /**
     * @brief 소유한 설계도와 관절 설정을 정리합니다.
     */
    UPhysicsAsset::~UPhysicsAsset() = default;

    /**
     * @brief 뼈 이름으로 바디 설계도의 번호를 찾습니다.
     */
    Index_t UPhysicsAsset::FindBodyIndex(const Core::FString& BodyName) const {
        const auto Found{mBodySetupIndexMap.find(BodyName)};

        return Found != mBodySetupIndexMap.end() ? Found->second : InvalidIndex;
    }

    /**
     * @brief 관절 이름으로 관절 설정의 번호를 찾습니다.
     */
    Index_t UPhysicsAsset::FindConstraintIndex(const Core::FString& ConstraintName) const {
        for (Index_t Index{}; Index < mConstraintSetup.size(); ++Index) {
            if (mConstraintSetup[Index] != nullptr && mConstraintSetup[Index]->mDefaultInstance.mJointName == ConstraintName) {
                return Index;
            }
        }

        return InvalidIndex;
    }

    /**
     * @brief 뼈 이름에서 설계도 번호를 찾는 표를 다시 만듭니다.
     */
    void UPhysicsAsset::UpdateBodySetupIndexMap() {
        mBodySetupIndexMap.clear();

        for (Index_t Index{}; Index < mSkeletalBodySetups.size(); ++Index) {
            if (mSkeletalBodySetups[Index] != nullptr) {
                mBodySetupIndexMap[mSkeletalBodySetups[Index]->GetBoneName()] = Index;
            }
        }
    }
}

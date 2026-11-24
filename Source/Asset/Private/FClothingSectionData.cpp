/**
 * @file FClothingSectionData.cpp
 * @brief 스켈레탈 메시의 섹션 하나가 천 에셋을 따르기 위한 연결 데이터를 구현합니다.
 * @ingroup Asset
 */

#include "Asset/FClothingSectionData.h"

namespace Asset {

    /**
     * @brief 천 에셋과 상세 단계가 지정되어 있는지 검사합니다.
     */
    bool FClothingSectionData::IsValid() const {
        return mAssetGuid.IsValid() && mAssetLodIndex != InvalidIndex;
    }
}

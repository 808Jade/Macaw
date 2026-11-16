/**
 * @file TObjectRef.cpp
 * @brief 객체를 소유하지 않고 세대 핸들과 GUID를 보관하는 형식 지정 객체 참조입니다.
 * @ingroup CoreUObject
 */

#include "CoreUObject/TObjectRef.h"
#include "CoreUObject/FObjectRegistry.h"
#include "CoreUObject/UObject.h"

CoreUObject::UObject* CoreUObject::ObjectRefPrivate::Resolve(CoreUObject::FObjectHandle Handle, const CoreUObject::UClass* Class) {
    CoreUObject::UObject* Object{CoreUObject::FObjectRegistry::Get()->Resolve(Handle)};

    return Object != nullptr && Object->IsA(Class) ? Object : nullptr;
}

Core::FGuid CoreUObject::ObjectRefPrivate::GetGuid(CoreUObject::FObjectHandle Handle) {
    const CoreUObject::UObject* Object{CoreUObject::FObjectRegistry::Get()->Resolve(Handle)};

    return Object != nullptr ? Object->GetGuid() : Core::FGuid::Null();
}

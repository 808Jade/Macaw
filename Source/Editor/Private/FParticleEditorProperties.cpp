#include "FParticleEditorToolkit.h"

#include "CoreUObject/Reflection/FArrayOps.h"
#include "CoreUObject/Reflection/FProperty.h"
#include "CoreUObject/Reflection/UClass.h"
#include "CoreUObject/Reflection/UScriptStruct.h"
#include "CoreUObject/UObject.h"
#include "Editor/FEditorContext.h"
#include "Engine/Particles/FParticleDistribution.h"
#include "Engine/Particles/Module/UParticleModuleRequired.h"
#include "Engine/Particles/UParticleEmitter.h"
#include "Engine/Particles/UParticleLODLevel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <imgui.h>

using namespace Editor;

namespace {

    bool EditText(const char* Label, Core::FString& Value) {
        std::array<char, 1024> Buffer{};

        if (Value.size() >= Buffer.size()) {
            ImGui::TextWrapped("%s", Value.c_str());
            return false;
        }

        std::ranges::copy(Value, Buffer.begin());

    if (!ImGui::InputText(Label, Buffer.data(), Buffer.size())) {
            return false;
        }

        Value = Buffer.data();
        return true;
    }

    bool EditVector(const char* Label, FVector& Value) {
        std::array<float, 3> Coordinates{Value.x, Value.y, Value.z};

        if (!ImGui::DragFloat3(Label, Coordinates.data(), 0.01f)) {
            return false;
        }

        const FVector Edited{Coordinates[0], Coordinates[1], Coordinates[2]};

        if (!Edited.IsFinite()) {
            return false;
        }

        Value = Edited;
        return true;
    }

    bool IsDistribution(const CoreUObject::FProperty& Property) {
        const auto* Struct{Property.GetStruct()};

        return Struct == Engine::FFloatDistribution::StaticStruct() || Struct == Engine::FVectorDistribution::StaticStruct();
    }

    bool EditDistributionMode(Engine::EDistributionMode& Mode) {
        int Selection{static_cast<int>(Mode)};

        if (!ImGui::Combo("Mode", &Selection, "Constant\0Uniform\0Curve\0")) {
            return false;
        }

        Mode = static_cast<Engine::EDistributionMode>(Selection);
        return true;
    }

    FVector4 GetCurveRange(const Core::TArray<Engine::FFloatCurveKey>& Keys) {
        FVector4 Range{0.0f, 1.0f, 0.0f, 1.0f};

        for (const auto& Key : Keys) {
            Range.x = std::min(Range.x, Key.mTime);
            Range.y = std::max(Range.y, Key.mTime);
            Range.z = std::min(Range.z, Key.mValue);
            Range.w = std::max(Range.w, Key.mValue);
        }

        const float Margin{(Range.w - Range.z) * 0.10f};
        Range.z -= Margin;
        Range.w += Margin;

        return Range;
    }

}

void FParticleEditorToolkit::DrawDetails() {
    auto* Object{GetSelectedObject()};

    if (Object == nullptr) {
        ImGui::TextUnformatted("Select an emitter or module.");
        return;
    }

    ImGui::BeginDisabled(!mContext.CanEditWorld());

    if (mSelection == ESelection::Emitter) {
        Core::FString Name{Object->GetName()};

        if (EditText("Emitter Name", Name) && Object->SetName(Name)) {
            MarkDirty();
        }
    } else {
        ImGui::TextUnformatted(Object->GetClass()->GetName().c_str());
        ImGui::Separator();
    }

    Core::FString Category{};

    for (const auto* Property : Object->GetClass()->GetAllProperties()) {
        if (!Property->HasFlag(Core::EPropertyFlags::Edit) || Property->HasFlag(Core::EPropertyFlags::ReadOnly) || Property->GetName() == "Name" || Property->GetName() == "LODDistances") {
            continue;
        }

        const Core::FString Next{Property->GetMetaData("Category")};

        if (Category != Next) {
            Category = Next;

            if (!Category.empty()) {
                ImGui::SeparatorText(Category.c_str());
            }
        }

        ImGui::PushID(Property);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.54f);
        if (DrawProperty(Object, *Property)) {
            if (mSelection == ESelection::Required && Property->GetName() == "UseLocalSpace") {
                const auto* Required{GetLevel()->GetRequiredModule()};

                for (const auto& Level : GetEmitter()->GetLODLevels()) {
                    Level->GetRequiredModule()->SetUseLocalSpace(Required->IsUseLocalSpace());
                }
            }

            MarkDirty();
        }
        ImGui::PopID();
    }

    ImGui::EndDisabled();
}

bool FParticleEditorToolkit::DrawProperty(void* Object, const CoreUObject::FProperty& Property) {
    if (IsDistribution(Property)) {
        return DrawDistribution(Object, Property);
    }

    if (Property.GetArrayOps() != nullptr) {
        return DrawArray(Object, Property);
    }

    auto Value{Property.ReadValue(Object)};
    const char* Label{Property.GetName().c_str()};
    bool Changed{};

    if (auto* Boolean{std::get_if<bool>(&Value)}) {
        Changed = ImGui::Checkbox(Label, Boolean);
    } else if (auto* Signed{std::get_if<Int32>(&Value)}) {
        const auto& Enums{Property.GetEnumValues()};

        if (Enums.empty()) {
            Changed = ImGui::DragInt(Label, Signed, 0.2f);
        } else {
            const auto Current{std::ranges::find_if(Enums, [Signed](const auto& Entry) {
                return Entry.mValue == *Signed;
            })};
            if (ImGui::BeginCombo(Label, Current != Enums.end() ? Current->mName.c_str() : "")) {
                for (const auto& Entry : Enums) {
                    if (ImGui::Selectable(Entry.mName.c_str(), Entry.mValue == *Signed)) {
                        *Signed = Entry.mValue;
                        Changed = true;
                    }
                }
                ImGui::EndCombo();
            }
        }
    } else if (auto* Unsigned{std::get_if<Uint32>(&Value)}) {
        Changed = ImGui::DragScalar(Label, ImGuiDataType_U32, Unsigned, 0.2f);
    } else if (auto* Text{std::get_if<Core::FString>(&Value)}) {
        const std::string_view AssetType{Property.GetMetaData("AssetType")};

        if (AssetType == "Material" || AssetType == "Mesh") {
            const auto Type{AssetType == "Material" ? Asset::EAssetType::Material : Asset::EAssetType::Mesh};
            Changed = mPicker.Draw(mCatalog, mThumbnails, Label, *Text, Type);
        } else {
            Changed = EditText(Label, *Text);
        }
    } else if (auto* Vector{std::get_if<FVector>(&Value)}) {
        Changed = EditVector(Label, *Vector);
    } else if (auto Number{Property.ToNumericValue(Value)}) {
        Core::FString NumericLabel{Label};
        const std::string_view DisplayUnit{Property.GetMetaData("DisplayUnit")};

        if (!DisplayUnit.empty()) {
            NumericLabel += " (";
            NumericLabel += DisplayUnit;
            NumericLabel += ')';
        } else if (std::holds_alternative<CoreUObject::FQuantityPropertyValue>(Value)) {
            NumericLabel += " (s)";
        }

        Changed = ImGui::DragFloat(NumericLabel.c_str(), &*Number, 0.01f) && std::isfinite(*Number) && Property.FromNumericValue(*Number, Value);
    }

    return Changed && Property.WriteValue(Object, Value);
}

bool FParticleEditorToolkit::DrawDistribution(void* Object, const CoreUObject::FProperty& Property) {
    if (!ImGui::TreeNodeEx(Property.GetName().c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        return false;
    }

    const auto Value{Property.ReadValue(Object)};
    const auto* Address{std::get<const void*>(Value)};
    const bool Scalar{Property.GetStruct() == Engine::FFloatDistribution::StaticStruct()};
    bool Changed{};
    Engine::EDistributionMode Mode{};

    if (Scalar) {
        auto Distribution{*static_cast<const Engine::FFloatDistribution*>(Address)};
        Changed = EditDistributionMode(Distribution.mMode);

        if (Distribution.mMode == Engine::EDistributionMode::Constant) {
            Changed |= ImGui::DragFloat("Constant", &Distribution.mConstant, 0.01f);
        } else if (Distribution.mMode == Engine::EDistributionMode::Uniform) {
            Changed |= ImGui::DragFloat("Minimum", &Distribution.mMinValue, 0.01f);
            Changed |= ImGui::DragFloat("Maximum", &Distribution.mMaxValue, 0.01f);
        }

        if (Distribution.mMode == Engine::EDistributionMode::Curve && Distribution.mCurveKeys.empty() && Changed) {
            Distribution.mCurveKeys.push_back({0, 0.0f, Distribution.mConstant});
            Distribution.mCurveKeys.push_back({1, 1.0f, Distribution.mConstant});
        }

        Mode = Distribution.mMode;
        Changed = Changed && std::isfinite(Distribution.mConstant) && std::isfinite(Distribution.mMinValue) && std::isfinite(Distribution.mMaxValue) && Property.WriteValue(Object, static_cast<const void*>(&Distribution));
    } else {
        auto Distribution{*static_cast<const Engine::FVectorDistribution*>(Address)};
        Changed = EditDistributionMode(Distribution.mMode);

        if (Distribution.mMode == Engine::EDistributionMode::Constant) {
            Changed |= EditVector("Constant", Distribution.mConstant);
        } else if (Distribution.mMode == Engine::EDistributionMode::Uniform) {
            Changed |= EditVector("Minimum", Distribution.mMinValue);
            Changed |= EditVector("Maximum", Distribution.mMaxValue);
            Changed |= ImGui::Checkbox("Lock Axes", &Distribution.mLockAxes);
        }

        if (Distribution.mMode == Engine::EDistributionMode::Curve && Changed) {
            const std::array Axes{&Distribution.mCurveKeysX, &Distribution.mCurveKeysY, &Distribution.mCurveKeysZ};
            const std::array Values{Distribution.mConstant.x, Distribution.mConstant.y, Distribution.mConstant.z};

            for (Index_t Index{}; Index < Axes.size(); ++Index) {
                if (Axes[Index]->empty()) {
                    Axes[Index]->push_back({0, 0.0f, Values[Index]});
                    Axes[Index]->push_back({1, 1.0f, Values[Index]});
                }
            }
        }

        Mode = Distribution.mMode;
        Changed = Changed && Property.WriteValue(Object, static_cast<const void*>(&Distribution));
    }

    if (Mode == Engine::EDistributionMode::Curve && ImGui::SmallButton("Edit Curve")) {
        ResetCurveSelection();
        mCurveProperty = &Property;
    }

    ImGui::TreePop();
    return Changed;
}

bool FParticleEditorToolkit::DrawArray(void* Object, const CoreUObject::FProperty& Property) {
    const auto& Ops{*Property.GetArrayOps()};
    const auto* Struct{Ops.mElement->GetStruct()};

    if (Struct == nullptr || !ImGui::TreeNode(Property.GetName().c_str())) {
        return false;
    }

    const auto Value{Property.ReadValue(Object)};
    const auto* Source{std::get<const void*>(Value)};
    const Uint32 Count{Ops.mSize(Source)};
    std::unique_ptr<void, void (*)(void*)> Draft{Ops.mCreate(), Ops.mDestroy};
    Ops.mResize(Draft.get(), Count);
    const auto Copy{[Struct](void* Destination, const void* Entry) {
        for (const auto* Field : Struct->GetProperties()) {
            Field->CopyValue(Destination, Entry);
        }
    }};

    bool Changed{};
    std::optional<Uint32> Removed{};

    for (Uint32 Index{}; Index < Count; ++Index) {
        ImGui::PushID(static_cast<int>(Index));
        auto* Entry{Ops.mAt(Draft.get(), Index)};
        Copy(Entry, Ops.mAt(const_cast<void*>(Source), Index));
        const Core::FString Label{"Entry " + std::to_string(Index + 1)};

        if (ImGui::TreeNode(Label.c_str())) {
            for (const auto* Field : Struct->GetProperties()) {
                if (Field->HasFlag(Core::EPropertyFlags::Edit) && !Field->HasFlag(Core::EPropertyFlags::ReadOnly)) {
                    Changed |= DrawProperty(Entry, *Field);
                }
            }

            if (ImGui::SmallButton("Remove Entry")) {
                Removed = Index;
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    if (Removed) {
        for (Uint32 Index{*Removed}; Index + 1 < Count; ++Index) {
            Copy(Ops.mAt(Draft.get(), Index), Ops.mAt(Draft.get(), Index + 1));
        }
        Ops.mResize(Draft.get(), Count - 1);
        Changed = true;
    }

    if (ImGui::SmallButton("Add Entry")) {
        Ops.mResize(Draft.get(), Ops.mSize(Draft.get()) + 1);
        Changed = true;
    }

    ImGui::TreePop();
    return Changed && Property.WriteValue(Object, static_cast<const void*>(Draft.get()));
}

void FParticleEditorToolkit::DrawCurve() {
    auto* Object{GetSelectedObject()};

    if (Object == nullptr) {
        ImGui::TextUnformatted("Select a module with a curve distribution.");
        return;
    }

    Core::TArray<const CoreUObject::FProperty*> Tracks{};

    for (const auto* Property : Object->GetClass()->GetAllProperties()) {
        if (!IsDistribution(*Property)) {
            continue;
        }

        const auto Value{Property->ReadValue(Object)};
        const auto* Address{std::get<const void*>(Value)};
        const auto Mode{Property->GetStruct() == Engine::FFloatDistribution::StaticStruct() ? static_cast<const Engine::FFloatDistribution*>(Address)->mMode : static_cast<const Engine::FVectorDistribution*>(Address)->mMode};

        if (Mode == Engine::EDistributionMode::Curve) {
            Tracks.push_back(Property);
        }
    }

    if (Tracks.empty()) {
        ImGui::TextUnformatted("Set a module distribution to Curve to edit its keys.");
        return;
    }

    if (std::ranges::find(Tracks, mCurveProperty) == Tracks.end()) {
        ResetCurveSelection();
        mCurveProperty = Tracks.front();
    }

    ImGui::BeginDisabled(!mContext.CanEditWorld());
    ImGui::SetNextItemWidth(180.0f);

    if (ImGui::BeginCombo("##CurveTrack", mCurveProperty->GetName().c_str())) {
        for (const auto* Track : Tracks) {
            if (ImGui::Selectable(Track->GetName().c_str(), Track == mCurveProperty)) {
                ResetCurveSelection();
                mCurveProperty = Track;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::PushID(mCurveProperty);
    const auto Value{mCurveProperty->ReadValue(Object)};
    const auto* Address{std::get<const void*>(Value)};

    if (mCurveProperty->GetStruct() == Engine::FFloatDistribution::StaticStruct()) {
        auto Distribution{*static_cast<const Engine::FFloatDistribution*>(Address)};

        if (DrawCurveTrack(Distribution.mCurveKeys) && mCurveProperty->WriteValue(Object, static_cast<const void*>(&Distribution))) {
            MarkDirty();
        }
    } else {
        auto Distribution{*static_cast<const Engine::FVectorDistribution*>(Address)};
        const std::array Axes{&Distribution.mCurveKeysX, &Distribution.mCurveKeysY, &Distribution.mCurveKeysZ};

        ImGui::SameLine();
        ImGui::SetNextItemWidth(65.0f);
        if (ImGui::Combo("Axis", &mCurveAxis, "X\0Y\0Z\0")) {
            mCurveKey = -1;
            mCurveDragging = false;
        }

        ImGui::PushID(mCurveAxis);
        if (DrawCurveTrack(*Axes[static_cast<Index_t>(mCurveAxis)]) && mCurveProperty->WriteValue(Object, static_cast<const void*>(&Distribution))) {
            MarkDirty();
        }
        ImGui::PopID();
    }

    ImGui::PopID();
    ImGui::EndDisabled();
}

bool FParticleEditorToolkit::DrawCurveTrack(Core::TArray<Engine::FFloatCurveKey>& Keys) {
    Engine::CurveKeys::EnsureKeyIds(Keys);
    Engine::CurveKeys::SortKeys(Keys);
    bool Changed{};
    const auto AddKey{[this, &Keys](float Time, float Value) {
        mCurveKey = Engine::CurveKeys::NewKeyId(Keys);
        Keys.push_back({mCurveKey, Time, Value});
        Engine::CurveKeys::SortKeys(Keys);
    }};

    ImGui::SameLine();
    if (ImGui::SmallButton("Add Key")) {
        const float Time{Keys.empty() ? 0.0f : Keys.back().mTime + 0.1f};
        AddKey(Time, Engine::CurveKeys::Evaluate(Keys, Time));
        Changed = true;
    }

    ImGui::SameLine();
    Int32 Selected{Engine::CurveKeys::FindKeyIndex(Keys, mCurveKey)};
    ImGui::BeginDisabled(Selected < 0);
    if (ImGui::SmallButton("Delete Key")) {
        Keys.erase(Keys.begin() + Selected);
        mCurveKey = -1;
        Selected = -1;
        Changed = true;
    }
    ImGui::EndDisabled();

    if (Selected >= 0) {
        auto& Key{Keys[static_cast<Index_t>(Selected)]};
        ImGui::SetNextItemWidth(72.0f);
        float Time{Key.mTime};
        if (ImGui::DragFloat("Time", &Time, 0.01f) && std::isfinite(Time)) {
            Key.mTime = std::max(Time, 0.0f);
            Changed = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(72.0f);
        float Value{Key.mValue};
        if (ImGui::DragFloat("Value", &Value, 0.01f) && std::isfinite(Value)) {
            Key.mValue = Value;
            Changed = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        int Mode{static_cast<int>(Key.mInterpMode)};
        if (ImGui::Combo("Interpolation", &Mode, "Constant\0Linear\0Cubic\0")) {
            Key.mInterpMode = static_cast<Engine::ECurveInterpMode>(Mode);
            Changed = true;
        }

        if (Key.mInterpMode == Engine::ECurveInterpMode::Cubic) {
            ImGui::SetNextItemWidth(75.0f);
            float Arrive{Key.mArriveTangent};
            if (ImGui::DragFloat("Arrive", &Arrive, 0.01f) && std::isfinite(Arrive)) {
                Key.mArriveTangent = Arrive;
                Changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(75.0f);
            float Leave{Key.mLeaveTangent};
            if (ImGui::DragFloat("Leave", &Leave, 0.01f) && std::isfinite(Leave)) {
                Key.mLeaveTangent = Leave;
                Changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Auto Tangent")) {
                Engine::CurveKeys::SetAutoTangent(Keys, static_cast<Index_t>(Selected));
                Changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Flat Tangent")) {
                Engine::CurveKeys::SetFlatTangent(Key);
                Changed = true;
            }
        }
    }

    Engine::CurveKeys::SortKeys(Keys);
    const ImVec2 Size{std::max(ImGui::GetContentRegionAvail().x, 64.0f), std::max(ImGui::GetContentRegionAvail().y, 95.0f)};
    const ImVec2 Origin{ImGui::GetCursorScreenPos()};
    const ImVec2 End{Origin.x + Size.x, Origin.y + Size.y};
    const ImVec2 Plot{Origin.x + 38.0f, Origin.y + 10.0f};
    const ImVec2 Extent{std::max(Size.x - 48.0f, 1.0f), Size.y - 30.0f};
    const FVector4 Range{mCurveDragging ? mCurveDragRange : GetCurveRange(Keys)};
    const float TimeSpan{Range.y - Range.x};
    const float ValueSpan{Range.w - Range.z};
    const auto ToScreen{[&](float Time, float Value) {
        return ImVec2{Plot.x + Extent.x * ((Time - Range.x) / TimeSpan), Plot.y + Extent.y * ((Range.w - Value) / ValueSpan)};
    }};

    ImGui::InvisibleButton("Curve Graph", Size);
    const bool Hovered{ImGui::IsItemHovered()};
    const auto& IO{ImGui::GetIO()};
    const auto Mouse{IO.MousePos};

    if (Hovered && ImGui::IsItemClicked()) {
        mCurveKey = -1;

        for (const auto& Key : Keys) {
            const auto Point{ToScreen(Key.mTime, Key.mValue)};
            if (std::abs(Mouse.x - Point.x) <= 7.0f && std::abs(Mouse.y - Point.y) <= 7.0f) {
                mCurveKey = Key.mKeyId;
                mCurveDragging = true;
                mCurveDragRange = Range;
                break;
            }
        }

        if (mCurveKey < 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            const float Time{std::max(Range.x + (Mouse.x - Plot.x) / Extent.x * TimeSpan, 0.0f)};
            const float Value{Range.w - (Mouse.y - Plot.y) / Extent.y * ValueSpan};
            AddKey(Time, Value);
            Changed = true;
        }
    }

    if (mCurveDragging && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
        const Int32 Key{Engine::CurveKeys::FindKeyIndex(Keys, mCurveKey)};
        if (Key >= 0) {
            auto& Current{Keys[static_cast<Index_t>(Key)]};
            Current.mTime = std::max(Current.mTime + IO.MouseDelta.x / Extent.x * TimeSpan, 0.0f);
            Current.mValue -= IO.MouseDelta.y / Extent.y * ValueSpan;
            Engine::CurveKeys::SortKeys(Keys);
            Changed = true;
        }
    }

    if (!ImGui::IsItemActive()) {
        mCurveDragging = false;
    }

    auto* Draw{ImGui::GetWindowDrawList()};
    Draw->AddRectFilled(Origin, End, ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f);

    for (int Line{}; Line <= 4; ++Line) {
        const float Alpha{static_cast<float>(Line) / 4.0f};
        const float X{Plot.x + Extent.x * Alpha};
        const float Y{Plot.y + Extent.y * Alpha};
        const ImU32 Grid{ImGui::GetColorU32(ImGuiCol_Border)};
        Draw->AddLine({X, Plot.y}, {X, Plot.y + Extent.y}, Grid);
        Draw->AddLine({Plot.x, Y}, {Plot.x + Extent.x, Y}, Grid);
    }

    std::array<char, 48> Label{};
    const ImU32 Text{ImGui::GetColorU32(ImGuiCol_TextDisabled)};
    std::snprintf(Label.data(), Label.size(), "%.3g", Range.w);
    Draw->AddText({Origin.x + 4.0f, Plot.y}, Text, Label.data());
    std::snprintf(Label.data(), Label.size(), "%.3g", Range.z);
    Draw->AddText({Origin.x + 4.0f, Plot.y + Extent.y - ImGui::GetTextLineHeight()}, Text, Label.data());
    std::snprintf(Label.data(), Label.size(), "%.3g", Range.x);
    Draw->AddText({Plot.x, Plot.y + Extent.y + 2.0f}, Text, Label.data());
    std::snprintf(Label.data(), Label.size(), "%.3g", Range.y);
    Draw->AddText({Plot.x + Extent.x - ImGui::CalcTextSize(Label.data()).x, Plot.y + Extent.y + 2.0f}, Text, Label.data());

    Draw->PushClipRect(Plot, {Plot.x + Extent.x, Plot.y + Extent.y}, true);
    const ImU32 Curve{ImGui::GetColorU32(ImGuiCol_PlotLinesHovered)};

    for (Index_t Index{1}; Index < Keys.size(); ++Index) {
        const auto& Left{Keys[Index - 1]};
        const auto& Right{Keys[Index]};
        ImVec2 Previous{ToScreen(Left.mTime, Left.mValue)};

        for (int Sample{1}; Sample <= 24; ++Sample) {
            const float Time{std::lerp(Left.mTime, Right.mTime, static_cast<float>(Sample) / 24.0f)};
            const auto Point{ToScreen(Time, Engine::CurveKeys::EvaluateSegment(Left, Right, Time))};
            Draw->AddLine(Previous, Point, Curve, 1.7f);
            Previous = Point;
        }
    }

    for (const auto& Key : Keys) {
        const auto Point{ToScreen(Key.mTime, Key.mValue)};
        Draw->AddCircleFilled(Point, Key.mKeyId == mCurveKey ? 5.0f : 3.5f, Key.mKeyId == mCurveKey ? ImGui::GetColorU32(ImGuiCol_Text) : Curve);
    }

    Draw->PopClipRect();

    if (Hovered) {
        ImGui::SetTooltip("Drag a key to move it. Double-click the graph to add a key.");
    }

    return Changed;
}

/**
 * @file FMeshViewerCloth.cpp
 * @brief 메시 뷰어의 천 칸(섹션 표, 설정, 칠하기, 충돌, 저장)과 미리 보기 천 시뮬레이션을 구현합니다.
 * @ingroup Editor
 */

#include "Core/Types.h"
#include "Editor/FMeshViewer.h"
#include "Viewer/FMeshViewerContent.h"
#include "Core/FilePath.h"
#include "Core/Math/FVector4.h"
#include "Asset/FAssetManager.h"
#include "Asset/FAssetFile.h"
#include "Asset/FClothingSectionData.h"
#include "Asset/USkeletalMesh.h"
#include "Asset/USkeleton.h"

#include "ClothEditing.h"
#include "PhysicsRuntime/FClothingSystemRuntimeModuleNv.h"
#include "PhysicsRuntime/IClothingSimulation.h"
#include "PhysicsRuntime/UClothConfigNv.h"
#include "PhysicsRuntime/UClothingAssetCommon.h"

#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>

namespace Editor {

    namespace {
        /**
         * @brief 결과 문장을 보여 주는 시간(초)입니다.
         */
        constexpr double ClothMessageSeconds{6.0};

        /**
         * @brief 기본값과 다른 설정 칸의 이름 색입니다.
         */
        constexpr ImU32 ChangedLabelColor{IM_COL32(255, 200, 90, 255)};

        /**
         * @brief 설정 칸이 기본값과 다른지 검사합니다.
         * @param[in] Field 설정 항목입니다.
         * @param[in] Config 값을 가진 설정입니다.
         * @return 기본값과 다르면 true입니다.
         */
        bool IsChangedFromDefault(const ClothEditing::FClothConfigField& Field, PhysicsRuntime::UClothConfigNv& Config) {
            constexpr float Tolerance{1.e-6f};

            if (Field.mGetFloat != nullptr) {
                return std::abs(*Field.mGetFloat(Config) - Field.mDefault.x) > Tolerance;
            }

            const FVector& Value{*Field.mGetVector(Config)};

            return std::abs(Value.x - Field.mDefault.x) > Tolerance || std::abs(Value.y - Field.mDefault.y) > Tolerance || std::abs(Value.z - Field.mDefault.z) > Tolerance;
        }

        /**
         * @brief 설정 칸을 기본값으로 되돌립니다.
         * @param[in] Field 설정 항목입니다.
         * @param[in,out] Config 값을 바꿀 설정입니다.
         */
        void ResetToDefault(const ClothEditing::FClothConfigField& Field, PhysicsRuntime::UClothConfigNv& Config) {
            if (Field.mGetFloat != nullptr) {
                *Field.mGetFloat(Config) = Field.mDefault.x;
            } else {
                *Field.mGetVector(Config) = Field.mDefault;
            }
        }

        /**
         * @brief 설정 항목의 설명에 허용 범위를 붙여 툴팁으로 보여 줍니다.
         * @param[in] Field 설정 항목입니다.
         */
        void ShowFieldTooltip(const ClothEditing::FClothConfigField& Field) {
            if (!ImGui::BeginItemTooltip()) {
                return;
            }

            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
            ImGui::TextUnformatted(Field.mLabel);
            ImGui::Separator();
            ImGui::TextUnformatted(Field.mTooltip);
            ImGui::TextDisabled("허용 범위: %g ~ %g", Field.mMin * Field.mDisplayScale, Field.mMax * Field.mDisplayScale);
            ImGui::TextDisabled("오른쪽 클릭: 기본값으로 되돌리기 | Ctrl+클릭: 숫자 입력");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        /**
         * @brief 이름과 설명을 툴팁으로 보여 줍니다.
         * @param[in] Label 이름과 설명입니다.
         */
        void ShowLabelTooltip(const ClothEditing::FClothLabel& Label) {
            if (!ImGui::BeginItemTooltip()) {
                return;
            }

            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
            ImGui::TextUnformatted(Label.mLabel);
            ImGui::Separator();
            ImGui::TextUnformatted(Label.mTooltip);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        /**
         * @brief 긴 설명 문장을 툴팁으로 보여 줍니다.
         * @param[in] Text 설명입니다.
         */
        void ShowTextTooltip(const char* Text) {
            if (!ImGui::BeginItemTooltip()) {
                return;
            }

            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
            ImGui::TextUnformatted(Text);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        /**
         * @brief 설정 칸 하나의 값 위젯을 그립니다.
         * @details 0~1처럼 좁은 범위는 슬라이더, 넓은 범위는 끌기 칸을 쓰고, 기본값과 다르면 칸을 물들입니다.
         * @param[in] Field 설정 항목입니다.
         * @param[in,out] Config 값을 가진 설정입니다.
         * @return 값 편집을 마쳐 미리 보기에 반영해야 하면 true입니다.
         */
        bool DrawFieldWidget(const ClothEditing::FClothConfigField& Field, PhysicsRuntime::UClothConfigNv& Config) {
            const bool Changed{IsChangedFromDefault(Field, Config)};
            const float Scale{Field.mDisplayScale};
            const float Min{Field.mMin * Scale};
            const float Max{Field.mMax * Scale};
            // 0~1처럼 좁은 범위는 슬라이더로 위치를 보여 주고, 아주 작은 값을 다루는 칸은 끌기 칸으로 둡니다.
            const bool UseSlider{Field.mMin >= 0.0f && (Field.mMax - Field.mMin) * Scale <= 1.0f && Field.mSpeed >= 0.001f};
            const ImGuiSliderFlags Flags{ImGuiSliderFlags_AlwaysClamp};
            bool Committed{};

            if (Changed) {
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(90, 70, 30, 255));
            }

            ImGui::PushID(Field.mKey);
            ImGui::SetNextItemWidth(-FLT_MIN);

            if (Field.mGetFloat != nullptr) {
                float* const Value{Field.mGetFloat(Config)};
                float Display{*Value * Scale};
                const bool Edited{UseSlider ? ImGui::SliderFloat("##Value", &Display, Min, Max, Field.mFormat, Flags) : ImGui::DragFloat("##Value", &Display, Field.mSpeed, Min, Max, Field.mFormat, Flags)};

                if (Edited) {
                    *Value = std::clamp(Display / Scale, Field.mMin, Field.mMax);
                }
            } else {
                FVector* const Value{Field.mGetVector(Config)};
                float Display[3]{Value->x * Scale, Value->y * Scale, Value->z * Scale};
                const bool Edited{UseSlider ? ImGui::SliderFloat3("##Value", Display, Min, Max, Field.mFormat, Flags) : ImGui::DragFloat3("##Value", Display, Field.mSpeed, Min, Max, Field.mFormat, Flags)};

                if (Edited) {
                    *Value = FVector{std::clamp(Display[0] / Scale, Field.mMin, Field.mMax), std::clamp(Display[1] / Scale, Field.mMin, Field.mMax), std::clamp(Display[2] / Scale, Field.mMin, Field.mMax)};
                }
            }

            // 끄는 동안 매 프레임 천을 다시 만들면 천이 계속 처음 모양으로 돌아가므로 손을 뗄 때 한 번 반영합니다.
            Committed = ImGui::IsItemDeactivatedAfterEdit();
            ShowFieldTooltip(Field);

            if (ImGui::BeginPopupContextItem("Reset")) {
                if (ImGui::MenuItem("Reset to Default", nullptr, false, Changed)) {
                    ResetToDefault(Field, Config);
                    Committed = true;
                }

                ImGui::EndPopup();
            }

            ImGui::PopID();

            if (Changed) {
                ImGui::PopStyleColor();
            }

            return Committed;
        }

        /**
         * @brief 이름, 값, 되돌리기 버튼을 표의 한 행으로 그립니다.
         * @param[in] Field 설정 항목입니다.
         * @param[in,out] Config 값을 가진 설정입니다.
         * @param[in] Disabled 다른 설정 때문에 쓰지 않는 칸이면 true입니다.
         * @return 값 편집을 마쳐 미리 보기에 반영해야 하면 true입니다.
         */
        bool DrawFieldRow(const ClothEditing::FClothConfigField& Field, PhysicsRuntime::UClothConfigNv& Config, bool Disabled) {
            const bool Changed{IsChangedFromDefault(Field, Config)};
            bool Committed{};

            ImGui::TableNextRow();
            ImGui::BeginDisabled(Disabled);

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();

            if (Changed) {
                ImGui::PushStyleColor(ImGuiCol_Text, ChangedLabelColor);
            }

            ImGui::TextUnformatted(Field.mLabel);

            if (Changed) {
                ImGui::PopStyleColor();
            }

            ShowFieldTooltip(Field);

            ImGui::TableSetColumnIndex(1);
            Committed = DrawFieldWidget(Field, Config);

            ImGui::TableSetColumnIndex(2);

            if (Changed) {
                ImGui::PushID(Field.mKey);

                if (ImGui::SmallButton("Reset")) {
                    ResetToDefault(Field, Config);
                    Committed = true;
                }

                ImGui::SetItemTooltip("기본값으로 되돌립니다.");
                ImGui::PopID();
            }

            ImGui::EndDisabled();

            return Committed;
        }

        /**
         * @brief 값 하나를 칠하기 대상의 단위 형식으로 문자열로 만듭니다.
         * @param[in] Format 단위를 포함한 printf 형식입니다.
         * @param[in] Value 값입니다.
         * @return 형식에 맞춘 문자열입니다.
         */
        Core::FString FormatValue(const char* Format, float Value) {
            char Buffer[64]{};
            std::snprintf(Buffer, sizeof(Buffer), Format, Value);

            return Buffer;
        }
    }

    /**
     * @brief 미리 보기에서 천을 움직일 천 시스템을 연결합니다.
     */
    void Editor::FMeshViewer::SetClothingSystem(PhysicsRuntime::FClothingSystemRuntimeModuleNv* ClothingSystem) {
        ReleaseClothPreview();
        mClothingSystem = ClothingSystem;
        mClothPreviewStale = true;
    }

    /**
     * @brief 고른 천 에셋을 반환합니다.
     */
    PhysicsRuntime::UClothingAssetCommon* Editor::FMeshViewer::GetSelectedClothingAsset() const {
        if (mSkeletalMesh == nullptr) {
            return nullptr;
        }

        const auto& ClothingAssets{mSkeletalMesh->GetMeshClothingAssets()};

        return mClothSelectedAsset < ClothingAssets.size() ? ClothingAssets[mClothSelectedAsset].get() : nullptr;
    }

    /**
     * @brief 천 칠하기를 쓸 수 있는 상태인지 검사합니다.
     */
    bool Editor::FMeshViewer::IsClothPainting() const {
        return mClothPaintEnabled && GetSelectedClothingAsset() != nullptr;
    }

    /**
     * @brief 천 칸 아래에 결과 문장을 잠시 보여 줍니다.
     */
    void Editor::FMeshViewer::ShowClothMessage(const Core::FString& Message) {
        mClothMessage = Message;
        mClothMessageTime = ImGui::GetTime();
    }

    /**
     * @brief 천 에셋이 바뀌었음을 기록하고 미리 보기 천을 다시 만들게 합니다.
     */
    void Editor::FMeshViewer::MarkClothChanged(const Core::FString& Message) {
        mClothUnsaved = true;
        mClothPreviewStale = true;
        ShowClothMessage(Message);
    }

    /**
     * @brief 천 칸 전체를 그립니다.
     */
    void Editor::FMeshViewer::DrawClothing() {
        Asset::USkeletalMesh& Mesh{*mSkeletalMesh.Get()};

        if (mSkeleton == nullptr) {
            ImGui::TextColored(ImVec4{1.0f, 0.75f, 0.3f, 1.0f}, "Pick a Skeleton above first.");
            ShowTextTooltip("천 입자는 뼈를 따라 움직이므로, 천을 만들거나 미리 보려면 스켈레톤이 있어야 합니다.");
        }

        DrawClothSections();

        const auto& ClothingAssets{Mesh.GetMeshClothingAssets()};

        if (ClothingAssets.empty()) {
            ImGui::TextDisabled("No cloth yet. Press Create Cloth on the section that should move like cloth.");
        } else {
            mClothSelectedAsset = std::min(mClothSelectedAsset, ClothingAssets.size() - 1);

            // 미리 보기에서 천을 움직일지 고르고, 처음 모양에서 다시 시작하게 합니다.
            ImGui::BeginDisabled(mClothingSystem == nullptr);

            if (ImGui::Checkbox("Simulate in Preview", &mClothSimulate)) {
                mClothPreviewStale = true;
            }

            ShowTextTooltip("켜면 미리 보기에서 천이 중력과 뼈 움직임에 따라 움직입니다.\n설정을 바꾸거나 칠하면 바로 다시 만들어 결과를 보여 줍니다.\nSkeleton 칸에서 뼈를 골라 돌리면 천이 따라옵니다.");
            ImGui::SameLine();

            if (ImGui::Button("Restart")) {
                mClothPreviewStale = true;
            }

            ShowTextTooltip("천을 지금 포즈의 스키닝 위치로 되돌려 처음부터 다시 움직입니다.");
            ImGui::EndDisabled();

            if (mClothingSystem == nullptr) {
                ImGui::SameLine();
                ImGui::TextDisabled("(not available in this window)");
            } else if (IsClothPainting()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(paused while painting)");
            }

            const PhysicsRuntime::UClothingAssetCommon& Selected{*ClothingAssets[mClothSelectedAsset]};
            const Index_t NumParticles{Selected.mLodData.empty() ? 0 : Selected.mLodData[0].mPhysicalMeshData.mVertices.size()};
            const Core::FString Title{"Cloth " + std::to_string(mClothSelectedAsset) + "  (" + std::to_string(NumParticles) + " particles)"};

            ImGui::SeparatorText(Title.c_str());

            if (ImGui::BeginTabBar("ClothTabs")) {
                bool PaintTabOpen{};

                // 탭 설명은 탭 내용을 그리기 전에 붙여야 탭 머리에 마우스를 올렸을 때 나옵니다.
                const bool SettingsOpen{ImGui::BeginTabItem("Settings")};
                ShowTextTooltip("천이 얼마나 늘어나고, 처지고, 흔들리고, 바람을 받는지 정합니다.");

                if (SettingsOpen) {
                    if (PhysicsRuntime::UClothConfigNv* Config{ClothingAssets[mClothSelectedAsset]->GetClothConfig()}) {
                        DrawClothSettings(*Config);
                    }

                    ImGui::EndTabItem();
                }

                PaintTabOpen = ImGui::BeginTabItem("Paint");
                ShowTextTooltip("입자마다 얼마나 움직일 수 있는지 같은 값을 미리 보기에 붓으로 칠합니다.");

                if (PaintTabOpen) {
                    DrawClothPaintSettings(*ClothingAssets[mClothSelectedAsset]);
                    ImGui::EndTabItem();
                }

                const bool CollisionOpen{ImGui::BeginTabItem("Collision")};
                ShowTextTooltip("천이 부딪힐 몸 충돌체(물리 에셋)를 고릅니다.");

                if (CollisionOpen) {
                    DrawClothCollision(*ClothingAssets[mClothSelectedAsset]);
                    ImGui::EndTabItem();
                }

                // 칠하기 탭을 떠나면 왼쪽 버튼을 다시 카메라와 뼈 선택에 돌려줍니다.
                if (!PaintTabOpen) {
                    mClothPaintEnabled = false;
                }

                ImGui::EndTabBar();
            }
        }

        ImGui::Separator();

        // 바꾼 것이 있을 때만 저장할 수 있게 해 저장 상태를 한눈에 알 수 있게 합니다.
        ImGui::BeginDisabled(!mClothUnsaved);

        if (ImGui::Button("Save Mesh")) {
            Core::FString Error{};

            // 뼈를 파일 안에 넣은 예전 메시는 새 형식으로 저장하면 뼈가 사라지므로 막습니다.
            if (mAssets->FindLegacySkeleton(Core::FilePath::FromUtf8(mPath)) != nullptr) {
                ShowClothMessage("This mesh uses an embedded skeleton, so its file cannot be saved safely.");
            } else if (mAssets->SaveSkeletalMesh(Core::FilePath::FromUtf8(mPath), Mesh, Error)) {
                mClothUnsaved = false;
                ShowClothMessage("Saved " + mPath + ".");
            } else {
                ShowClothMessage("Cannot save the skeletal mesh. " + Error);
            }
        }

        ImGui::EndDisabled();
        ShowTextTooltip("천 에셋, 설정, 칠한 값을 메시 파일에 저장합니다.\n저장하지 않으면 에디터를 닫을 때 사라집니다.\n레벨에 놓인 같은 메시는 레벨을 다시 열거나 PIE를 다시 시작하면 바뀐 천을 씁니다.");
        ImGui::SameLine();

        if (mClothUnsaved) {
            ImGui::TextColored(ImVec4{1.0f, 0.78f, 0.35f, 1.0f}, "Unsaved changes");
        } else {
            ImGui::TextDisabled("No unsaved changes");
        }

        if (!mClothMessage.empty() && ImGui::GetTime() - mClothMessageTime < ClothMessageSeconds) {
            ImGui::TextWrapped("%s", mClothMessage.c_str());
        }
    }

    /**
     * @brief 섹션마다 머티리얼, 천 상태, 천 만들기 버튼을 표로 그리고 천을 고르게 합니다.
     */
    void Editor::FMeshViewer::DrawClothSections() {
        Asset::USkeletalMesh& Mesh{*mSkeletalMesh.Get()};
        const auto& SubMeshes{Mesh.GetMeshData().GetSubMeshes()};
        const auto& Slots{Mesh.GetMeshData().GetMaterialSlots()};
        const auto& ClothingAssets{Mesh.GetMeshClothingAssets()};
        const ImGuiTableFlags Flags{ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp};

        if (!ImGui::BeginTable("ClothSections", 4, Flags)) {
            return;
        }

        ImGui::TableSetupColumn("Section", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Material");
        ImGui::TableSetupColumn("Cloth");
        ImGui::TableSetupColumn("##Action", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        for (Index_t SubMeshIndex{}; SubMeshIndex < SubMeshes.size(); ++SubMeshIndex) {
            const Asset::FClothingSectionData* SectionData{Mesh.GetSectionClothingData(SubMeshIndex)};
            const bool HasCloth{SectionData != nullptr && SectionData->IsValid() && SectionData->mCorrespondClothAssetIndex < ClothingAssets.size()};
            const Index_t MaterialIndex{SubMeshes[SubMeshIndex].GetMaterialIndex()};

            ImGui::PushID(static_cast<int>(SubMeshIndex));
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%zu", SubMeshIndex);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(MaterialIndex < Slots.size() ? Slots[MaterialIndex].GetName().c_str() : "-");

            ImGui::TableSetColumnIndex(2);

            if (HasCloth) {
                const Index_t AssetIndex{SectionData->mCorrespondClothAssetIndex};
                const PhysicsRuntime::UClothingAssetCommon& ClothingAsset{*ClothingAssets[AssetIndex]};
                const Index_t NumParticles{ClothingAsset.mLodData.empty() ? 0 : ClothingAsset.mLodData[0].mPhysicalMeshData.mVertices.size()};
                const Core::FString Label{"Cloth " + std::to_string(AssetIndex) + "  (" + std::to_string(NumParticles) + " particles)"};

                if (ImGui::Selectable(Label.c_str(), AssetIndex == mClothSelectedAsset)) {
                    mClothSelectedAsset = AssetIndex;
                }

                ShowTextTooltip("누르면 아래 칸에서 이 천의 설정, 칠하기, 충돌을 편집합니다.");
            } else {
                ImGui::TextDisabled("-");
            }

            ImGui::TableSetColumnIndex(3);

            if (!HasCloth) {
                ImGui::BeginDisabled(mSkeleton == nullptr);

                if (ImGui::SmallButton("Create Cloth")) {
                    // 메시가 쓰는 물리 에셋과 부딪히게 만들고, 저장한 물리 에셋이면 경로도 남깁니다.
                    Core::FString Error{};
                    PhysicsRuntime::UPhysicsAsset* PhysicsAsset{Mesh.GetPhysicsAssetPath().empty() ? Mesh.GetGeneratedPhysicsAsset(*mSkeleton.Get()) : mAssets->LoadPhysicsAsset(Core::FilePath::FromUtf8(Mesh.GetPhysicsAssetPath()), Error)};

                    if (Editor::ClothEditing::CreateClothFromSection(Mesh, *mSkeleton.Get(), SubMeshIndex, PhysicsAsset, Mesh.GetPhysicsAssetPath(), Error) != nullptr) {
                        const Asset::FClothingSectionData* Created{Mesh.GetSectionClothingData(SubMeshIndex)};
                        mClothSelectedAsset = Created != nullptr ? Created->mCorrespondClothAssetIndex : 0;
                        MarkClothChanged("Created cloth on section " + std::to_string(SubMeshIndex) + ". Paint Max Distance to choose what moves.");
                    } else {
                        ShowClothMessage(Error);
                    }
                }

                ImGui::EndDisabled();
                ShowTextTooltip("이 섹션의 정점으로 천을 만듭니다.\n같은 자리의 정점을 합쳐 입자로 만들고, 위쪽 20%는 몸에 고정(최대 거리 0), 나머지는 0.5 m까지 움직이게 시작합니다.\n어디를 고정할지는 Paint 탭에서 고칩니다.");
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    /**
     * @brief 고른 천 에셋의 설정을 묶음별 표로 그리고 바꾼 값을 미리 보기에 반영합니다.
     */
    void Editor::FMeshViewer::DrawClothSettings(PhysicsRuntime::UClothConfigNv& Config) {
        const auto& Fields{ClothEditing::GetConfigFields()};
        const float Scale{ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi};
        bool Committed{};

        ImGui::TextDisabled("Hover a name for help. Changed values are orange.");

        // 강성은 네 종류 연결마다 같은 네 값이 있으므로 행과 열로 된 표로 보여 줍니다.
        if (ImGui::CollapsingHeader(ClothEditing::StiffnessGroup, ImGuiTreeNodeFlags_DefaultOpen)) {
            ShowTextTooltip("천을 이루는 네 종류 연결(세로, 가로, 굽힘, 전단)의 세기와 느슨한 구간입니다.\n행 이름과 열 이름에 마우스를 올리면 설명이 나옵니다.");

            const auto& Rows{ClothEditing::GetStiffnessRows()};
            const auto& Columns{ClothEditing::GetStiffnessColumns()};

            if (ImGui::BeginTable("StiffnessTable", 1 + static_cast<int>(Columns.size()), ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("##Row", ImGuiTableColumnFlags_WidthFixed, 80.0f * Scale);

                for (const ClothEditing::FClothLabel& Column : Columns) {
                    ImGui::TableSetupColumn(Column.mLabel);
                }

                ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

                for (Index_t ColumnIndex{}; ColumnIndex < Columns.size(); ++ColumnIndex) {
                    ImGui::TableSetColumnIndex(static_cast<int>(ColumnIndex) + 1);
                    ImGui::TableHeader(Columns[ColumnIndex].mLabel);
                    ShowLabelTooltip(Columns[ColumnIndex]);
                }

                for (Index_t RowIndex{}; RowIndex < Rows.size(); ++RowIndex) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(Rows[RowIndex].mLabel);
                    ShowLabelTooltip(Rows[RowIndex]);

                    for (Index_t ColumnIndex{}; ColumnIndex < Columns.size(); ++ColumnIndex) {
                        const Index_t FieldIndex{RowIndex * Columns.size() + ColumnIndex};

                        ImGui::TableSetColumnIndex(static_cast<int>(ColumnIndex) + 1);

                        if (FieldIndex < Fields.size() && Core::FString{Fields[FieldIndex].mGroup} == ClothEditing::StiffnessGroup) {
                            Committed = DrawFieldWidget(Fields[FieldIndex], Config) || Committed;
                        }
                    }
                }

                ImGui::EndTable();
            }
        }

        for (const ClothEditing::FClothLabel& Group : ClothEditing::GetConfigGroups()) {
            const Core::FString GroupName{Group.mLabel};
            const ImGuiTreeNodeFlags HeaderFlags{GroupName == "Simulation" ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None};

            if (!ImGui::CollapsingHeader(Group.mLabel, HeaderFlags)) {
                ShowTextTooltip(Group.mTooltip);

                continue;
            }

            ShowTextTooltip(Group.mTooltip);

            // 바람 방식은 실수 값이 아니므로 표 위에 따로 고르게 합니다.
            if (GroupName == "Wind") {
                const char* const Methods[]{"Legacy", "Accurate"};
                int Method{Config.mClothingWindMethod == PhysicsRuntime::EClothingWindMethodNv::Accurate ? 1 : 0};

                if (ImGui::Combo("Wind Method", &Method, Methods, 2)) {
                    Config.mClothingWindMethod = Method == 1 ? PhysicsRuntime::EClothingWindMethodNv::Accurate : PhysicsRuntime::EClothingWindMethodNv::Legacy;
                    Committed = true;
                }

                ShowTextTooltip(ClothEditing::GetWindMethodTooltip());
            }

            if (!ImGui::BeginTable(Group.mLabel, 3, ImGuiTableFlags_SizingStretchProp)) {
                continue;
            }

            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 150.0f * Scale);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, 50.0f * Scale);

            // 중력 덮어쓰기는 켜고 끄는 값이므로 중력 칸 바로 앞에 체크 상자로 둡니다.
            if (GroupName == "Simulation") {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Override Gravity");
                ShowTextTooltip("켜면 월드 중력과 Gravity Scale 대신 아래 Gravity 값을 중력으로 씁니다.\n기본값은 꺼짐입니다.");
                ImGui::TableSetColumnIndex(1);

                if (ImGui::Checkbox("##OverrideGravity", &Config.mUseGravityOverride)) {
                    Committed = true;
                }
            }

            for (const ClothEditing::FClothConfigField& Field : Fields) {
                if (GroupName != Field.mGroup) {
                    continue;
                }

                // 다른 설정 때문에 쓰지 않는 칸은 잠가서 바꿔도 아무 일이 없는 칸을 만지지 않게 합니다.
                const Core::FString Key{Field.mKey};
                const bool UsesOverride{Config.mUseGravityOverride};
                const bool Legacy{Config.mClothingWindMethod == PhysicsRuntime::EClothingWindMethodNv::Legacy};
                const bool Disabled{(Key == "gravityscale" && UsesOverride) || (Key == "gravityoverride" && !UsesOverride) || ((Key == "winddrag" || Key == "windlift") && Legacy)};

                Committed = DrawFieldRow(Field, Config, Disabled) || Committed;
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();

        if (ImGui::Button("Reset All to Defaults")) {
            for (const ClothEditing::FClothConfigField& Field : Fields) {
                ResetToDefault(Field, Config);
            }

            Config.mUseGravityOverride = false;
            Config.mClothingWindMethod = PhysicsRuntime::EClothingWindMethodNv::Legacy;
            Committed = true;
        }

        ShowTextTooltip("이 천의 모든 설정을 기본값으로 되돌립니다.");

        if (Committed) {
            // 천끼리 충돌 거리가 바뀌었을 수 있으므로 부딪힐 입자를 다시 고릅니다.
            if (PhysicsRuntime::UClothingAssetCommon* ClothingAsset{GetSelectedClothingAsset()}) {
                ClothingAsset->BuildSelfCollisionData();
            }

            MarkClothChanged("Settings applied to the preview. Save the mesh to keep them.");
        }
    }

    /**
     * @brief 칠하기 켜기, 대상, 도구, 값, 붓 크기, 색 범례를 그립니다.
     */
    void Editor::FMeshViewer::DrawClothPaintSettings(PhysicsRuntime::UClothingAssetCommon&) {
        ImGui::Checkbox("Paint in Viewport", &mClothPaintEnabled);
        ShowTextTooltip("켜면 미리 보기에 천 입자가 색 점으로 보이고, 왼쪽 버튼이 칠하기로 바뀝니다.\n카메라 회전은 오른쪽 버튼, 이동은 가운데 버튼으로 합니다.\n칠하는 동안 천은 처음 모양으로 멈춰 있습니다.");

        const ClothEditing::FPaintTargetInfo& Info{ClothEditing::GetPaintTargetInfo(mClothPaintTarget)};

        if (ImGui::BeginCombo("Target", Info.mName)) {
            for (int TargetIndex{}; TargetIndex < ClothEditing::NumPaintTargets; ++TargetIndex) {
                const ClothEditing::FPaintTargetInfo& Candidate{ClothEditing::GetPaintTargetInfo(TargetIndex)};

                if (ImGui::Selectable(Candidate.mName, TargetIndex == mClothPaintTarget)) {
                    mClothPaintTarget = TargetIndex;
                }

                ShowTextTooltip(Candidate.mTooltip);
            }

            ImGui::EndCombo();
        }

        ShowTextTooltip(Info.mTooltip);

        // 도구는 한 줄에 나란히 놓고 각각 무엇을 하는지 툴팁으로 알려 줍니다.
        const auto& Tools{ClothEditing::GetPaintTools()};

        for (Index_t ToolIndex{}; ToolIndex < Tools.size(); ++ToolIndex) {
            if (ToolIndex > 0) {
                ImGui::SameLine();
            }

            ImGui::RadioButton(Tools[ToolIndex].mLabel, &mClothPaintTool, static_cast<int>(ToolIndex));
            ShowLabelTooltip(Tools[ToolIndex]);
        }

        const auto Tool{static_cast<ClothEditing::EClothPaintTool>(mClothPaintTool)};

        if (Tool != ClothEditing::EClothPaintTool::Smooth) {
            ImGui::DragFloat("Value", &mClothPaintValue, Info.mSpeed, 0.0f, Info.mMax, Info.mFormat, ImGuiSliderFlags_AlwaysClamp);
            ShowTextTooltip("칠할 값입니다. 단위와 뜻은 Target에 마우스를 올리면 나옵니다.");
        } else {
            ImGui::SliderFloat("Strength", &mClothSmoothStrength, 0.0f, 1.0f, "%.2f");
            ShowTextTooltip("한 프레임에 이웃 평균 쪽으로 옮기는 비율입니다.\n높이면 빨리 고르게 되고, 낮추면 조금씩 다듬어집니다.");
        }

        if (Tool == ClothEditing::EClothPaintTool::Fill) {
            ImGui::DragFloat("Threshold", &mClothFillThreshold, Info.mSpeed, 0.0f, Info.mMax, Info.mFormat, ImGuiSliderFlags_AlwaysClamp);
            ShowTextTooltip("누른 입자 값과 이만큼까지 차이 나는 이어진 입자를 함께 채웁니다.\n0이면 값이 똑같은 입자만 채웁니다.");
        } else {
            ImGui::SliderFloat("Brush Radius", &mClothPaintRadius, 2.0f, 300.0f, "%.0f px");
            ShowTextTooltip("미리 보기 화면에서 붓 원의 반지름입니다(픽셀).");
        }

        // 색 범례: 왼쪽 빨강이 0, 오른쪽 초록이 지금 칠한 값 중 가장 큰 값입니다.
        ImGui::Spacing();
        ImGui::TextUnformatted("0");
        ImGui::SameLine();

        const ImVec2 BarMin{ImGui::GetCursorScreenPos()};
        const float BarWidth{std::max(ImGui::GetContentRegionAvail().x - 90.0f, 40.0f)};
        const float BarHeight{ImGui::GetTextLineHeight()};
        const ImU32 Red{IM_COL32(255, 0, 60, 255)};
        const ImU32 Green{IM_COL32(0, 255, 60, 255)};

        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(BarMin, ImVec2{BarMin.x + BarWidth, BarMin.y + BarHeight}, Red, Green, Green, Red);
        ImGui::Dummy(ImVec2{BarWidth, BarHeight});
        ShowTextTooltip("미리 보기의 점 색입니다. 빨강은 0, 초록은 지금 칠한 값 중 가장 큰 값입니다.");
        ImGui::SameLine();
        ImGui::TextUnformatted(FormatValue(Info.mFormat, mClothPaintMaxValue).c_str());

        if (IsClothPainting() && mClothHoveredParticle != InvalidIndex) {
            ImGui::Text("Particle %zu: %s", mClothHoveredParticle, FormatValue(Info.mFormat, mClothHoveredValue).c_str());
        } else {
            ImGui::TextDisabled("Hover a point in the preview to read its value.");
        }

        ImGui::TextDisabled("Left drag: paint | Shift + drag: paint 0 | Right drag: orbit | Middle: pan");
    }

    /**
     * @brief 천이 부딪힐 물리 에셋을 고르는 칸을 그립니다.
     */
    void Editor::FMeshViewer::DrawClothCollision(PhysicsRuntime::UClothingAssetCommon& ClothingAsset) {
        // 천 전용 물리 에셋을 고릅니다. 메시가 쓰는 물리 에셋과 따로 정할 수 있습니다.
        const Core::FString Current{ClothingAsset.mPhysicsAssetPath};

        ImGui::BeginDisabled(mSkeleton == nullptr);

        if (ImGui::BeginCombo("Physics Asset##Cloth", Current.empty() ? "None" : Current.c_str())) {
            Core::FString ListError{};

            if (ImGui::Selectable("None", Current.empty())) {
                ClothingAsset.SetPhysicsAsset(nullptr, Editor::ClothEditing::GetSkeletonBoneNames(*mSkeleton.Get()));
                ClothingAsset.mPhysicsAssetPath.clear();
                MarkClothChanged("The cloth no longer collides with a physics asset.");
            }

            for (const auto& Candidate : mAssets->GetAssetPaths(ListError)) {
                Asset::EAssetType Type{};

                if (!Asset::FAssetFile::GetType(mAssets->ResolvePath(Candidate, ListError), Type, ListError) || Type != Asset::EAssetType::PhysicsAsset) {
                    continue;
                }

                const Core::FString Relative{Core::FilePath::ToGenericUtf8(Candidate)};

                if (ImGui::Selectable(Relative.c_str(), Relative == Current)) {
                    Core::FString LoadError{};
                    PhysicsRuntime::UPhysicsAsset* PhysicsAsset{mAssets->LoadPhysicsAsset(Candidate, LoadError)};

                    if (PhysicsAsset != nullptr) {
                        ClothingAsset.SetPhysicsAsset(PhysicsAsset, Editor::ClothEditing::GetSkeletonBoneNames(*mSkeleton.Get()));
                        ClothingAsset.mPhysicsAssetPath = Relative;
                        MarkClothChanged("The cloth now collides with " + Relative + ".");
                    } else {
                        ShowClothMessage(LoadError);
                    }
                }
            }

            ImGui::EndCombo();
        }

        ImGui::EndDisabled();
        ShowTextTooltip("이 물리 에셋의 구와 캡슐이 천을 밀어냅니다. 상자와 볼록 껍질은 쓰지 않습니다.\n구는 최대 32개, 캡슐도 최대 32개까지 쓰고 나머지는 버립니다.\n천이 닿는 부위(다리, 엉덩이, 몸통)만 담은 작은 천 전용 물리 에셋을 쓰는 것이 좋습니다.");

        ImGui::TextWrapped("Spheres and capsules of this physics asset push the cloth away. Up to 32 spheres and 32 capsules are used.");
        ImGui::TextDisabled("Thickness and Friction are in Settings > Collision.");
    }

    /**
     * @brief 칠하기 대상 천 입자를 가중치 색으로 그리고, 왼쪽 버튼으로 고른 도구를 적용합니다.
     */
    void Editor::FMeshViewer::DrawClothPaint(const Engine::FSceneView& View, const FVector2& Origin, const FVector2& Mouse, bool Hovered, const Core::FInput& Input) {
        using Core::EKey;

        PhysicsRuntime::UClothingAssetCommon* const ClothingAsset{GetSelectedClothingAsset()};
        mClothHoveredParticle = InvalidIndex;

        if (ClothingAsset == nullptr || ClothingAsset->mLodData.empty()) {
            return;
        }

        PhysicsRuntime::FClothPhysicalMeshData& PhysMesh{ClothingAsset->mLodData[0].mPhysicalMeshData};
        PhysicsRuntime::FPointWeightMap& Map{PhysMesh.AddWeightMap(Editor::ClothEditing::GetPaintTarget(mClothPaintTarget))};

        // 처음 칠하는 맵은 입자 수만큼 0으로 채웁니다.
        if (Map.mValues.size() != PhysMesh.mVertices.size()) {
            Map.mValues.resize(PhysMesh.mVertices.size(), 0.0f);
        }

        const auto Tool{static_cast<ClothEditing::EClothPaintTool>(mClothPaintTool)};
        const bool Dragging{Hovered && Input.IsDown(EKey::MouseLeft) && !Input.IsDown(EKey::MouseRight) && !Input.IsDown(EKey::MouseMiddle)};
        const bool Pressed{Hovered && Input.WasPressed(EKey::MouseLeft) && !Input.IsDown(EKey::MouseRight) && !Input.IsDown(EKey::MouseMiddle)};
        const float PaintValue{Input.IsDown(EKey::Shift) ? 0.0f : mClothPaintValue};
        const float RadiusSquared{mClothPaintRadius * mClothPaintRadius};
        ImDrawList* const DrawList{ImGui::GetWindowDrawList()};

        // 입자를 화면 좌표로 한 번 투영해 두고 붓 안의 입자와 마우스에 가장 가까운 입자를 찾습니다.
        Core::TArray<FVector2> ScreenPositions(PhysMesh.mVertices.size());
        Core::TArray<bool> Visible(PhysMesh.mVertices.size(), false);
        Core::TArray<Index_t> InBrush{};
        float ClosestSquared{RadiusSquared};

        for (Index_t VertIndex{}; VertIndex < PhysMesh.mVertices.size(); ++VertIndex) {
            const FVector4 Clip{FVector4{PhysMesh.mVertices[VertIndex], 1.0f} * View.mViewProjection};

            // 카메라 뒤의 입자는 건너뜁니다.
            if (Clip.w <= 1.e-6f) {
                continue;
            }

            const FVector2 Screen{(Clip.x / Clip.w * 0.5f + 0.5f) * View.mSize.x, (0.5f - Clip.y / Clip.w * 0.5f) * View.mSize.y};
            const float DistanceSquared{(Screen.x - Mouse.x) * (Screen.x - Mouse.x) + (Screen.y - Mouse.y) * (Screen.y - Mouse.y)};

            ScreenPositions[VertIndex] = Screen;
            Visible[VertIndex] = true;

            if (Hovered && DistanceSquared <= RadiusSquared) {
                InBrush.push_back(VertIndex);
            }

            if (Hovered && DistanceSquared <= ClosestSquared) {
                ClosestSquared = DistanceSquared;
                mClothHoveredParticle = VertIndex;
            }
        }

        // 고른 도구를 적용합니다. 다듬기와 채우기는 삼각형 변으로 이어진 이웃이 필요합니다.
        if (Tool == ClothEditing::EClothPaintTool::Brush && Dragging && !InBrush.empty()) {
            for (const Index_t VertIndex : InBrush) {
                Map.mValues[VertIndex] = PaintValue;
            }

            mClothPaintDirty = true;
        } else if (Tool == ClothEditing::EClothPaintTool::Smooth && Dragging && !InBrush.empty()) {
            ClothEditing::SmoothWeights(Map.mValues, ClothEditing::BuildParticleNeighbors(PhysMesh), InBrush, mClothSmoothStrength);
            mClothPaintDirty = true;
        } else if (Tool == ClothEditing::EClothPaintTool::Fill && Pressed && mClothHoveredParticle != InvalidIndex) {
            const Index_t NumFilled{ClothEditing::FillWeights(Map.mValues, ClothEditing::BuildParticleNeighbors(PhysMesh), mClothHoveredParticle, PaintValue, mClothFillThreshold)};
            mClothPaintDirty = NumFilled > 0;
        }

        float MaxValue{1.e-4f};

        for (const float Value : Map.mValues) {
            MaxValue = std::max(MaxValue, Value);
        }

        mClothPaintMaxValue = MaxValue;

        // 0은 빨강, 가장 큰 값은 초록으로 그리고 마우스 아래 입자는 크게 그립니다.
        for (Index_t VertIndex{}; VertIndex < PhysMesh.mVertices.size(); ++VertIndex) {
            if (!Visible[VertIndex]) {
                continue;
            }

            const float Ratio{std::clamp(Map.mValues[VertIndex] / MaxValue, 0.0f, 1.0f)};
            const ImU32 Color{IM_COL32(static_cast<int>(255.0f * (1.0f - Ratio)), static_cast<int>(255.0f * Ratio), 60, 255)};
            const ImVec2 Center{Origin.x + ScreenPositions[VertIndex].x, Origin.y + ScreenPositions[VertIndex].y};

            DrawList->AddCircleFilled(Center, VertIndex == mClothHoveredParticle ? 5.0f : 2.5f, Color);
        }

        if (Hovered) {
            if (Tool != ClothEditing::EClothPaintTool::Fill) {
                DrawList->AddCircle(ImVec2{Origin.x + Mouse.x, Origin.y + Mouse.y}, mClothPaintRadius, IM_COL32(255, 255, 255, 200), 0, 1.5f);
            }

            // 마우스 옆에 아래 입자의 값을 보여 줍니다.
            if (mClothHoveredParticle != InvalidIndex) {
                mClothHoveredValue = Map.mValues[mClothHoveredParticle];

                const Core::FString Text{FormatValue(ClothEditing::GetPaintTargetInfo(mClothPaintTarget).mFormat, mClothHoveredValue)};
                const ImVec2 TextPosition{Origin.x + Mouse.x + 14.0f, Origin.y + Mouse.y + 14.0f};
                const ImVec2 TextSize{ImGui::CalcTextSize(Text.c_str())};

                DrawList->AddRectFilled(ImVec2{TextPosition.x - 3.0f, TextPosition.y - 2.0f}, ImVec2{TextPosition.x + TextSize.x + 3.0f, TextPosition.y + TextSize.y + 2.0f}, IM_COL32(0, 0, 0, 180), 3.0f);
                DrawList->AddText(TextPosition, IM_COL32(255, 255, 255, 255), Text.c_str());
            }
        }

        // 고정 입자와 역질량 계산은 무거우므로 버튼을 놓을 때 한 번 반영합니다.
        if (mClothPaintDirty && !Input.IsDown(EKey::MouseLeft)) {
            Editor::ClothEditing::ApplyPaintedWeights(*mSkeletalMesh.Get(), *ClothingAsset);
            mClothPaintDirty = false;
            MarkClothChanged("Painted " + Core::FString{ClothEditing::GetPaintTargetInfo(mClothPaintTarget).mName} + ". Turn off Paint in Viewport to see the cloth move.");
        }
    }

    /**
     * @brief 미리 보기 포즈로 천 시뮬레이션 입력을 채웁니다.
     */
    void Editor::FMeshViewer::UpdateClothPreviewContext(float DeltaTime) {
        // 한 프레임이 너무 길면 천이 튀므로 물리 한 번에 진행할 최대 시간으로 자릅니다.
        constexpr float MaxPhysicsDelta{1.0f / 30.0f};
        const Engine::FSkeletalMeshPose& Pose{mContent->GetPose()};

        // 미리 보기 메시는 원점에 놓이므로 컴포넌트 변환은 단위 행렬입니다.
        mClothPreviewContext.mBoneTransforms = Pose.GetGlobalTransforms();
        mClothPreviewContext.mRefToLocals = Pose.GetSkinMatrices();
        mClothPreviewContext.mComponentToWorld = FMatrix::Identity();
        mClothPreviewContext.mWorldGravity = FVector{0.0f, 0.0f, -9.81f};
        mClothPreviewContext.mDeltaSeconds = std::min(DeltaTime, MaxPhysicsDelta);
        mClothPreviewContext.mPredictedLod = 0;
        mClothPreviewContext.mMaxDistanceScale = 1.0f;
        mClothPreviewContext.mWindVelocity = FVector{};
        mClothPreviewContext.mWindAdaption = 2.0f;
        mClothPreviewContext.mTeleportMode = PhysicsRuntime::EClothingTeleportMode::None;
    }

    /**
     * @brief 미리 보기 천 시뮬레이션을 해제하고 천 섹션을 스키닝 위치로 그리게 합니다.
     */
    void Editor::FMeshViewer::ReleaseClothPreview() {
        if (mClothPreview != nullptr) {
            mClothPreview->Shutdown();
            mClothPreview.reset();
        }

        if (mContent != nullptr) {
            mContent->ClearClothSimulationData();
        }
    }

    /**
     * @brief 미리 보기 천 시뮬레이션을 지금 천 에셋과 포즈로 다시 만듭니다.
     */
    void Editor::FMeshViewer::RecreateClothPreview() {
        ReleaseClothPreview();
        mClothPreviewStale = false;

        if (!mClothSimulate || mClothingSystem == nullptr || mContent == nullptr || mSkeletalMesh == nullptr || mSkeleton == nullptr || !mContent->GetPose().GetRevision().IsValid()) {
            return;
        }

        const auto& ClothingAssets{mSkeletalMesh->GetMeshClothingAssets()};

        if (ClothingAssets.empty()) {
            return;
        }

        // 천 에셋의 사용 뼈를 지금 고른 스켈레톤의 뼈 번호로 다시 찾습니다.
        const Core::TArray<Core::FString> SkeletonBoneNames{Editor::ClothEditing::GetSkeletonBoneNames(*mSkeleton.Get())};

        for (const auto& ClothingAsset : ClothingAssets) {
            ClothingAsset->RefreshBoneMapping(SkeletonBoneNames);
        }

        mClothPreview = mClothingSystem->CreateSimulation();

        if (mClothPreview == nullptr) {
            return;
        }

        // 섹션에 연결된 천 에셋만 천으로 만듭니다.
        Core::TArray<const PhysicsRuntime::UClothingAssetCommon*> AssetsInUse{};
        mSkeletalMesh->GetClothingAssetsInUse(AssetsInUse);
        UpdateClothPreviewContext(0.0f);

        for (Index_t AssetIndex{}; AssetIndex < ClothingAssets.size(); ++AssetIndex) {
            const PhysicsRuntime::UClothingAssetCommon* const ClothingAsset{ClothingAssets[AssetIndex].get()};

            if (std::find(AssetsInUse.begin(), AssetsInUse.end(), ClothingAsset) != AssetsInUse.end()) {
                mClothPreview->CreateActor(*ClothingAsset, AssetIndex, mClothPreviewContext);
            }
        }

        Core::TMap<Index_t, PhysicsRuntime::FClothSimulData> Data{};
        mClothPreview->GetSimulationData(Data, mClothPreviewContext);
        mContent->SetClothSimulationData(std::move(Data));
    }

    /**
     * @brief 미리 보기 천을 한 프레임 진행하고 결과를 미리 보기 장면에 넣습니다.
     */
    void Editor::FMeshViewer::TickClothPreview(float DeltaTime) {
        if (mContent == nullptr || mSkeletalMesh == nullptr) {
            return;
        }

        // 칠하는 동안에는 점이 처음 모양 위에 있으므로 천도 처음 모양으로 멈춰 둡니다.
        if (IsClothPainting()) {
            if (mClothPreview != nullptr) {
                ReleaseClothPreview();
            }

            mClothPreviewStale = true;

            return;
        }

        if (mClothPreviewStale) {
            RecreateClothPreview();
        }

        // 시간이 0이면 솔버가 속도를 구할 수 없으므로 진행하지 않습니다.
        if (mClothPreview == nullptr || DeltaTime <= 0.0f) {
            return;
        }

        UpdateClothPreviewContext(DeltaTime);
        mClothPreview->Simulate(mClothPreviewContext);

        Core::TMap<Index_t, PhysicsRuntime::FClothSimulData> Data{};
        mClothPreview->GetSimulationData(Data, mClothPreviewContext);
        mContent->SetClothSimulationData(std::move(Data));
    }
}

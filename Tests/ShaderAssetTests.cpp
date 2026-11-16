#include "Asset/FAssetManager.h"
#include "Asset/UMaterial.h"
#include "RenderCore/Shader/UShader.h"
#include "Asset/UTexture2D.h"
#include "Asset/Sequence/ULevelSequence.h"
#include "Asset/Animation/UAnimGraph.h"
#include "Asset/Animation/UBlendSpace1D.h"
#include "CoreUObject/FObjectReference.h"
#include "CoreUObject/Reflection/NewObject.h"
#include "FMaterialAssetData.h"
#include "FMaterialFile.h"
#include "Renderer/Renderer.h"
#include "RenderCore/Shader/FShaderManager.h"
#include "Serialization/MemoryArchive.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

    void Require(bool Condition, const Core::FString& Message) {
        if (!Condition) {
            throw std::runtime_error{Message};
        }

        std::cout << "PASS " << Message << '\n';
    }

    class FFixture {
    public:
        FFixture();
        ~FFixture();

    public:
        const std::filesystem::path& GetRoot() const;

    private:
        std::filesystem::path mRoot{};
    };

    FFixture::FFixture()
		: mRoot{std::filesystem::absolute("Build/ShaderAssetTests") / Core::FGuid{}.ToString()} {
        std::filesystem::create_directories(mRoot / "Sources");
        std::filesystem::create_directories(mRoot / "Moved");
        std::ofstream Source{mRoot / "Sources/Color.hlsl"};
        Source << "float4 ShadePixel() : SV_TARGET { return float4(0.2, 0.4, 0.6, 1.0); }\n";
    }

    FFixture::~FFixture() {
        std::error_code Code{};
        std::filesystem::remove_all(mRoot, Code);
    }

    const std::filesystem::path& FFixture::GetRoot() const {
        return mRoot;
    }

    CoreUObject::TObjectRef<RenderCore::UShader> MakeReference(const Core::FGuid& Guid) {
        return *CoreUObject::FObjectReference{Guid}.ToObjectRef<RenderCore::UShader>();
    }

    void WriteLegacyMaterial(const std::filesystem::path& Path, const Core::FString& Shader) {
        Core::TArray<Uint8> Payload{};
        Serialization::FMemoryArchive Archive{Payload};
        Uint32 Version{1};
        Core::FString Name{"Legacy"};
        Core::FString ShaderPath{Shader};
        Core::FString Empty{};
        FVector4 Color{0.2f, 0.4f, 0.6f, 1.0f};
        float Specular{0.35f};
        float Shininess{32.0f};
        float NormalStrength{1.0f};
        bool TwoSided{};
        Archive.Serialize("Version", Version);
        Archive.Serialize("Name", Name);
        Archive.Serialize("Shader", ShaderPath);
        Archive.Serialize("BaseColor", Color);
        Archive.Serialize("BaseColorTexture", Empty);
        Archive.Serialize("NormalMap", Empty);
        Archive.Serialize("SpecularStrength", Specular);
        Archive.Serialize("Shininess", Shininess);
        Archive.Serialize("NormalStrength", NormalStrength);
        Archive.Serialize("TwoSided", TwoSided);
        Core::FString Error{};
        Require(Asset::FAssetFile::Save(Path, Asset::EAssetType::Material, Payload, Error), "Write legacy material");
    }

    void TestIdentityAndReferences() {
        FFixture Fixture{};
        Core::FGuid ShaderGuid{Core::FGuid::Null()};
        Core::FGuid MaterialGuid{Core::FGuid::Null()};
        Core::FGuid TextureGuid{Core::FGuid::Null()};
        CoreUObject::TObjectRef<RenderCore::UShader> SurvivingReference{};
        Core::FString Error{};

        {
            Asset::FAssetManager Assets{Fixture.GetRoot()};
            auto Shader{CoreUObject::NewObject<RenderCore::UShader>()};
            Require(Shader->SetSourcePath("Sources/Color.hlsl") && Shader->SetEntryPoint("ShadePixel") && Shader->SetUnlit(true), "Configure shader definition");
            Require(!Shader->SetSourcePath("../Outside.hlsl") && !Shader->SetEntryPoint("invalid entry"), "Reject invalid shader paths and entry points");
            ShaderGuid = Shader->GetGuid();
            Require(Assets.SaveShader("Definitions/Color.masset", *Shader, Error), "Save shader asset");
            Require(!Assets.SaveShader("Definitions/Duplicate.masset", *Shader, Error), "Reject saving one shader GUID to two files");

            auto Material{CoreUObject::NewObject<Asset::UMaterial>()};
            MaterialGuid = Material->GetGuid();
            Require(Material->SetShader(Shader.get()) && Assets.SaveMaterial("Materials/Color.masset", *Material, Error), "Save material with shader GUID");
            Require(!Assets.SaveMaterial("Materials/Duplicate.masset", *Material, Error), "Reject saving one material GUID to two files");

            auto Texture{CoreUObject::NewObject<Asset::UTexture2D>()};
            TextureGuid = Texture->GetGuid();
            Require(Texture->SetPixels(1, 1, Asset::ETextureFormat::RGBA8, {255, 255, 255, 255}) && Assets.SaveTexture("Textures/White.masset", *Texture, Error), "Save persistent identity for other asset types");
            Require(!Assets.SaveTexture("Textures/Duplicate.masset", *Texture, Error), "Reject duplicate GUIDs for other asset types");

            auto* Loaded{Assets.LoadShader("Definitions/Color.masset", Error)};
            SurvivingReference = Loaded;
            Require(Loaded != nullptr && Loaded->GetGuid() == ShaderGuid && Loaded->GetEntryPoint() == "ShadePixel" && Loaded->IsUnlit(), "Restore shader identity and reflected settings");
            auto* LoadedMaterial{Assets.LoadMaterial("Materials/Color.masset", Error)};
            Require(LoadedMaterial != nullptr && LoadedMaterial->GetShader() == Loaded, "Material loader binds shader GUID to cached UObject");
            Require(Assets.LoadAsset(MaterialGuid, Error) == LoadedMaterial, "Load material directly by GUID");

            Core::TArray<Uint8> Bytes{};
            Serialization::FMemoryArchive Writer{Bytes};
            LoadedMaterial->Serialize(Writer);
            auto Copy{CoreUObject::NewObject<Asset::UMaterial>()};
            Serialization::FMemoryArchive Reader{static_cast<const Core::TArray<Uint8>&>(Bytes)};
            Copy->Serialize(Reader);
            Require(!Reader.HasError() && Copy->GetShader() == nullptr && Copy->GetShader().GetGuid() == ShaderGuid, "Reflection restores shader GUID before binding");
            Require(Copy->ResolveShader(Assets, Error) && Copy->GetShader() == Loaded, "Resolve reflected TObjectRef through asset loader");

            auto WrongType{MakeReference(MaterialGuid)};
            Require(!Assets.ResolveReference(WrongType, Error) && Error.find("type") != Core::FString::npos, "Reject material GUID used as shader before recursive loading");
            auto Missing{MakeReference(Core::FGuid{})};
            Require(!Assets.ResolveReference(Missing, Error) && !Error.empty(), "Report missing referenced asset");

            Require(Shader->SetUnlit(false) && Assets.SaveShader("Definitions/Color.masset", *Shader, Error) && !Loaded->IsUnlit() && SurvivingReference == Loaded, "Saving shader updates cached definition without breaking references");

            std::filesystem::rename(Fixture.GetRoot() / "Definitions/Color.masset", Fixture.GetRoot() / "Moved/Renamed.masset");
            auto Moved{MakeReference(ShaderGuid)};
            Require(Assets.ResolveReference(Moved, Error) && Moved == Loaded && Assets.GetShaderPath(Loaded) == "moved/renamed.masset", "Find renamed shader by GUID and preserve loaded object");
        }

        Require(SurvivingReference == nullptr && SurvivingReference.GetGuid() == ShaderGuid, "Non-owning shader reference retains GUID after manager destruction");

        {
            Asset::FAssetManager Assets{Fixture.GetRoot()};
            Require(Assets.ResolveReference(SurvivingReference, Error) && SurvivingReference->GetGuid() == ShaderGuid, "Reload asset by retained GUID after restart");
            auto* Material{Assets.LoadMaterial("Materials/Color.masset", Error)};
            Require(Material != nullptr && Material->GetShader() == SurvivingReference && Material->GetGuid() == MaterialGuid, "Cold load restores material after shader file rename");
            CoreUObject::TObjectRef<Asset::UTexture2D> Texture{*CoreUObject::FObjectReference{TextureGuid}.ToObjectRef<Asset::UTexture2D>()};
            Require(Assets.ResolveReference(Texture, Error) && Texture->GetGuid() == TextureGuid, "Generic GUID resolver loads texture identity");
            Require(Assets.LoadAsset(RenderCore::UShader::GetUnlit()->GetGuid(), Error) == RenderCore::UShader::GetUnlit(), "Resolve builtin shader GUID without a project file");

            std::filesystem::copy_file(Fixture.GetRoot() / "Moved/Renamed.masset", Fixture.GetRoot() / "Moved/Duplicate.masset");
            Require(!Assets.RefreshAssetRegistry(Error) && Error.find("Duplicate") != Core::FString::npos, "Reject duplicate asset GUIDs");
            std::filesystem::remove(Fixture.GetRoot() / "Moved/Duplicate.masset");

            auto CyclicMaterial{CoreUObject::NewObject<Asset::UMaterial>()};
            CyclicMaterial->SetShader(MakeReference(CyclicMaterial->GetGuid()));
            Require(Asset::FMaterialFile::Save(Fixture.GetRoot() / "Materials/Cyclic.masset", Assets, *CyclicMaterial, Error), "Write malformed self-reference fixture");
            Asset::FAssetManager ColdAssets{Fixture.GetRoot()};
            Require(ColdAssets.LoadMaterial("Materials/Cyclic.masset", Error) == nullptr && Error.find("type") != Core::FString::npos, "Reject self-referencing material without recursive loading");

            Core::TArray<Uint8> Payload{};
            Require(Asset::FAssetFile::Load(Fixture.GetRoot() / "Moved/Renamed.masset", Asset::EAssetType::Shader, Payload, Error), "Read shader payload for corruption test");
            Require(Asset::FAssetFile::Save(Fixture.GetRoot() / "Moved/Mismatch.masset", Asset::EAssetType::Shader, Payload, Error, Core::FGuid{}), "Write mismatched shader identity fixture");
            Require(Assets.LoadShader("Moved/Mismatch.masset", Error) == nullptr, "Reject disagreement between shader header and UObject GUID");
        }

        std::filesystem::remove(Fixture.GetRoot() / "Moved/Renamed.masset");
        Asset::FAssetManager MissingAssets{Fixture.GetRoot()};
        Require(MissingAssets.LoadMaterial("Materials/Color.masset", Error) == nullptr && !Error.empty(), "Fail explicitly when saved shader GUID has no asset");
    }

    void TestLegacyAndBuiltins() {
        FFixture Fixture{};
        Core::FString Error{};
        WriteLegacyMaterial(Fixture.GetRoot() / "LegacyLit.masset", "Shaders/Lit.hlsl");
        WriteLegacyMaterial(Fixture.GetRoot() / "LegacyUnlit.masset", "Shaders/Unlit.hlsl");
        WriteLegacyMaterial(Fixture.GetRoot() / "LegacyCustom.masset", "Sources/Color.hlsl");
        Asset::FAssetManager Assets{Fixture.GetRoot()};
        auto* Lit{Assets.LoadMaterial("LegacyLit.masset", Error)};
        auto* Unlit{Assets.LoadMaterial("LegacyUnlit.masset", Error)};
        auto* Custom{Assets.LoadMaterial("LegacyCustom.masset", Error)};
        Require(Lit != nullptr && Lit->GetShader() == RenderCore::UShader::GetDefault(), "Migrate legacy Lit shader path");
        Require(Unlit != nullptr && Unlit->GetShader() == RenderCore::UShader::GetUnlit() && Unlit->GetShader()->IsUnlit(), "Migrate legacy Unlit shader path");
        Require(Custom != nullptr && Custom->GetShader() != nullptr && !Assets.GetShaderPath(Custom->GetShader()).empty(), "Create shader asset for legacy custom HLSL reference");
        Require(Assets.SaveMaterial("LegacyCustom.masset", *Custom, Error), "Save migrated material with GUID reference");
        Require(!RenderCore::UShader::GetUnlit()->SetSourcePath("Sources/Color.hlsl") && RenderCore::UShader::GetUnlit()->GetSourcePath() == "Shaders/Unlit.hlsl", "Builtin definitions retain immutable identity and source");
        auto Transient{CoreUObject::NewObject<RenderCore::UShader>()};
        auto Material{CoreUObject::NewObject<Asset::UMaterial>()};
        Material->SetShader(Transient.get());
        Require(!Assets.SaveMaterial("Transient.masset", *Material, Error), "Reject saving a reference to an unsaved shader");
        Asset::FAssetManager EmptyAssets{Fixture.GetRoot() / "NewContent"};
        auto DefaultMaterial{CoreUObject::NewObject<Asset::UMaterial>()};
        Require(EmptyAssets.SaveMaterial("Default.masset", *DefaultMaterial, Error), "Save first asset into a new content root");

        std::filesystem::path Imported{};
        Require(Assets.ImportAsset(Fixture.GetRoot() / "Sources/Color.hlsl", "Imported", Imported, Error), "Import HLSL as a shader asset");
        auto* ImportedShader{Assets.LoadShader(Imported, Error)};
        Require(ImportedShader != nullptr && std::filesystem::is_regular_file(Fixture.GetRoot() / ImportedShader->GetSourcePath()), "Imported shader points to project source");
    }

    void TestShaderPipelines() {
        FFixture Fixture{};
        HWND Window{CreateWindowExW(0, L"STATIC", L"Shader asset tests", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr)};
        Require(Window != nullptr, "Create hidden renderer test window");
        Renderer::FRenderer Renderer{};
        Renderer.SetProjectPaths(Fixture.GetRoot(), Fixture.GetRoot() / "ShaderCache");
        Require(Renderer.Initialize(Window, 64, 64), "Initialize shader asset renderer");
        auto Shader{CoreUObject::NewObject<RenderCore::UShader>()};
        Shader->SetSourcePath("Sources/Color.hlsl");
        Shader->SetEntryPoint("ShadePixel");
        auto Material{CoreUObject::NewObject<Asset::UMaterial>()};
        Material->SetShader(Shader.get());
        auto* Pipeline{Renderer.GetMaterialPipeline(*Material)};
        Require(Pipeline != nullptr, "Compile material pipeline from referenced shader definition and entry point");
        Require(Material->GetShader() == Shader.get() && Shader->GetCompilationCount() == 1 && !Shader->GetByteCode().empty() && Shader->GetRevision() != 0, "Material references the UObject that owns the actual compiled bytecode and revision");
        Shader->SetUnlit(true);
        Require(Renderer.GetMaterialPipeline(*Material) != nullptr && Renderer.GetMaterialPipeline(*Material) != Pipeline, "Shader definition edits select the correct compiled variant");
        Require(Shader->GetCompilationCount() == 2, "One material shader UObject retains both compiled variants");
        const auto Revision{Shader->GetRevision()};

        {
            std::ofstream Source{Fixture.GetRoot() / "Sources/Color.hlsl"};
            Source << "float4 ShadePixel() : SV_TARGET { return float4(0.12345, 0.54321, 0.98765, 1.0); }\n";
        }

        Require(Renderer.ReloadChangedShaders() && Shader->GetRevision() != Revision && Material->GetShader() == Shader.get(), "Renderer hot reload updates the material's shader UObject directly");
        auto* Previous{Renderer.GetMaterialPipeline(*Material)};
        auto Replacement{CoreUObject::NewObject<RenderCore::UShader>()};
        Replacement->SetSourcePath(Shader->GetSourcePath());
        Replacement->SetEntryPoint(Shader->GetEntryPoint());
        Replacement->SetUnlit(Shader->IsUnlit());
        Replacement->RestoreGuid(Shader->GetGuid());
        Material->SetShader(Replacement.get());
        Require(Renderer.GetMaterialPipeline(*Material) != Previous && Replacement->GetCompilationCount() == 1, "A recreated UObject with the same asset GUID receives its own compiled pipeline");
        Shader.reset();
        Require(Renderer.ReloadChangedShaders(), "Expired material shader objects do not block shader reload");
        Renderer.Shutdown();
        DestroyWindow(Window);
    }

    void TestCompiledShaderObjects() {
        FFixture Fixture{};
        Core::FString Diagnostics{};
        RenderCore::FShaderManager Manager{Fixture.GetRoot() / "CodeCache"};
        RenderCore::FShaderDescription Description{};
        Description.mSourcePath = Fixture.GetRoot() / "Sources/Color.hlsl";
        Description.mEntryPoint = "ShadePixel";
        Description.mTargetProfile = "ps_5_0";
        auto Code{Manager.Load(Description, Diagnostics)};
        Require(Code != nullptr && Code->GetClass() == RenderCore::UShader::StaticClass(), "Shader manager creates compiled UShader UObjects");
        Require(Manager.Load(Description, Diagnostics) == Code && Code->GetCompilation() != nullptr && !Code->GetByteCode().empty(), "Compiled UObject cache returns the same typed object reference");
        Asset::FAssetManager Assets{Fixture.GetRoot()};
        Require(Assets.SaveShader("Compiled.masset", *Code, Diagnostics), "Save the compiled shader UObject directly as an asset");
        auto* Restored{Assets.LoadShader("Compiled.masset", Diagnostics)};
        Require(Restored != nullptr && Restored->GetGuid() == Code->GetGuid() && Restored->GetSourcePath() == "Sources/Color.hlsl" && Restored->GetEntryPoint() == "ShadePixel", "Restore the compiled shader object's GUID and source after asset loading");
        const auto Guid{Code->GetGuid()};
        const auto Handle{Code.GetHandle()};
        const auto Snapshot{Code->GetCompilation()};
        const auto Revision{Code->GetRevision()};
        Core::TArray<CoreUObject::TObjectRef<RenderCore::UShader>> Changed{};

        {
            std::ofstream Source{Description.mSourcePath};
            Source << "float4 ShadePixel() : SV_TARGET { return float4(0.123456, 0.765432, 0.987654, 1.0); }\n";
        }

        Require(Manager.HasChanges(*Code) && Manager.ReloadChanged(Changed, Diagnostics), "Detect and reload modified compiled shader UObject");
        Require(Changed.size() == 1 && Changed.front() == Code && Code->GetGuid() == Guid && Code.GetHandle() == Handle && Code->GetRevision() != Revision, "Recompilation preserves UObject identity and handle while updating revision");
        Require(Code->GetCompilation() != Snapshot && !Snapshot->mByteCode.empty(), "Recompilation preserves the lifetime of previous bytecode snapshots");
        const auto ValidSnapshot{Code->GetCompilation()};
        const auto ValidRevision{Code->GetRevision()};

        {
            std::ofstream Source{Description.mSourcePath};
            Source << "invalid shader source\n";
        }

        Require(!Manager.ReloadChanged(Changed, Diagnostics) && Changed.empty() && Code->GetCompilation() == ValidSnapshot && Code->GetRevision() == ValidRevision, "Failed recompilation retains the last valid compiled UObject state");

        {
            std::ofstream Source{Description.mSourcePath};
            Source << "float4 ShadePixel() : SV_TARGET { return float4(0.999999, 0.111111, 0.222222, 1.0); }\n";
        }

        Require(Manager.ReloadChanged(Changed, Diagnostics), "Recover shader UObject after fixing a compile error");
        auto MaterialShader{CoreUObject::NewObject<RenderCore::UShader>()};
        Core::FString FirstKey{};
        Core::FString SecondKey{};
        Require(Manager.Load(*MaterialShader, Description, FirstKey, Diagnostics) && MaterialShader->GetCompilation(FirstKey) != nullptr, "Compile directly into a caller-owned shader UObject");
        Description.mMacros.push_back({"MATERIAL_VARIANT", "1"});
        Require(Manager.Load(*MaterialShader, Description, SecondKey, Diagnostics) && FirstKey != SecondKey && MaterialShader->GetCompilationCount() == 2, "Caller-owned UShader stores independent macro variants");
        MaterialShader.reset();
        Require(Manager.ReloadChanged(Changed, Diagnostics), "Shader manager discards expired non-owning UObject references");
        Manager.Clear();
        Require(Code == nullptr && Code.GetGuid() == Guid && !ValidSnapshot->mByteCode.empty(), "Manager destruction invalidates shader object references while bytecode snapshots remain valid");
        Description.mMacros.clear();
        auto Cached{Manager.Load(Description, Diagnostics)};
        Require(Cached != nullptr && Cached->IsLoadedFromCache(), "Reload shader UObject from the compiled disk cache");
    }

    void TestShaderCacheReplacement() {
        FFixture Fixture{};
        Asset::FAssetManager Assets{Fixture.GetRoot()};
        Core::FString Error{};
        auto First{CoreUObject::NewObject<RenderCore::UShader>()};
        auto Second{CoreUObject::NewObject<RenderCore::UShader>()};
        Require(Assets.SaveShader("First.masset", *First, Error) && Assets.SaveShader("Second.masset", *Second, Error), "Save shader cache replacement fixtures");
        CoreUObject::TObjectRef<RenderCore::UShader> FirstReference{Assets.LoadShader("First.masset", Error)};
        CoreUObject::TObjectRef<RenderCore::UShader> SecondReference{Assets.LoadShader("Second.masset", Error)};
        std::filesystem::remove(Fixture.GetRoot() / "Second.masset");
        std::filesystem::rename(Fixture.GetRoot() / "First.masset", Fixture.GetRoot() / "Second.masset");
        auto* Replaced{Assets.LoadShader("Second.masset", Error)};
        Require(Replaced != nullptr && FirstReference == Replaced && SecondReference != nullptr && SecondReference != Replaced, "Replacing a cached path preserves moved and retired object lifetimes");
    }

    void TestOverwriteAndJsonIdentities() {
        FFixture Fixture{};
        Core::FString Error{};
        Core::FGuid TextureGuid{Core::FGuid::Null()};
        Core::FGuid SequenceGuid{Core::FGuid::Null()};
        Core::FGuid GraphGuid{Core::FGuid::Null()};
        Core::FGuid BlendSpaceGuid{Core::FGuid::Null()};

        {
            Asset::FAssetManager Assets{Fixture.GetRoot()};
            auto Texture{CoreUObject::NewObject<Asset::UTexture2D>()};
            TextureGuid = Texture->GetGuid();
            Require(Texture->SetPixels(1, 1, Asset::ETextureFormat::RGBA8, {255, 255, 255, 255}) && Assets.SaveTexture("White.masset", *Texture, Error), "Save initial asset identity for overwrite test");
            auto Sequence{CoreUObject::NewObject<Asset::ULevelSequence>()};
            SequenceGuid = Sequence->GetGuid();
            Require(Assets.SaveSequence("Sequence.masset", *Sequence, Error), "Save sequence identity in file metadata");
            auto Graph{CoreUObject::NewObject<Asset::UAnimGraph>()};
            GraphGuid = Graph->GetGuid();
            Require(Assets.SaveAnimGraph("Graph.masset", *Graph, Error), "Save animation graph identity in file metadata");
            auto BlendSpace{CoreUObject::NewObject<Asset::UBlendSpace1D>()};
            BlendSpaceGuid = BlendSpace->GetGuid();
            Require(Assets.SaveBlendSpace("BlendSpace.masset", *BlendSpace, Error), "Save blend space identity in file metadata");
        }

        {
            Asset::FAssetManager Assets{Fixture.GetRoot()};
            auto Replacement{CoreUObject::NewObject<Asset::UTexture2D>()};
            Require(Replacement->SetPixels(1, 1, Asset::ETextureFormat::RGBA8, {0, 0, 0, 255}) && Assets.SaveTexture("White.masset", *Replacement, Error), "Existing overwrite API accepts new object data");
        }

        Asset::FAssetManager Assets{Fixture.GetRoot()};
        Require(Assets.LoadAsset(TextureGuid, Error) != nullptr, "Overwrite preserves the destination file GUID");
        Require(Assets.LoadAsset(SequenceGuid, Error) != nullptr, "Restore sequence GUID independently of its JSON payload");
        Require(Assets.LoadAsset(GraphGuid, Error) != nullptr, "Restore animation graph GUID independently of its JSON payload");
        Require(Assets.LoadAsset(BlendSpaceGuid, Error) != nullptr, "Restore blend space GUID independently of its JSON payload");
    }

}

int main() {
    try {
        TestIdentityAndReferences();
        TestLegacyAndBuiltins();
        TestShaderCacheReplacement();
        TestOverwriteAndJsonIdentities();
        TestCompiledShaderObjects();
        TestShaderPipelines();
    } catch (const std::exception& Error) {
        std::cerr << "FAIL " << Error.what() << '\n';

        return 1;
    }

    return 0;
}

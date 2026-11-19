/**
 * @file FShaderCompiler.cpp
 * @brief HLSL 컴파일과 진단 및 include 의존성 수집에 대한 구현입니다.
 * @ingroup RenderCore
 */

#include "RenderCore/Shader/FShaderCompiler.h"
#include "Core/FilePath.h"
#include "FShaderIncludeHandler.h"
#include "FShaderFile.h"

#include <wrl/client.h>

#include <utility>

using namespace RenderCore;

namespace {
    bool CompileSource(const FShaderDescription& Description, const std::filesystem::path& EditedPath, std::string_view EditedSource, FShaderCompilation* Compilation, Core::FString& Diagnostics) {
        FShaderDescription Normalized{Description};
        if (!Normalized.Normalize(Diagnostics)) {
            return false;
        }

        std::filesystem::path OverridePath{};
        if (!EditedPath.empty() && !ShaderFile::NormalizePath(EditedPath, OverridePath, Diagnostics)) {
            return false;
        }

        Core::TArray<Uint8> DiskSource{};
        FShaderDependency RootDependency{};
        std::string_view Source{EditedSource};
        if (!OverridePath.empty() && Core::FilePath::ToLower(Normalized.mSourcePath) == Core::FilePath::ToLower(OverridePath)) {
            RootDependency.mPath = Normalized.mSourcePath;
        } else {
            if (!ShaderFile::Read(Normalized.mSourcePath, DiskSource, RootDependency, Diagnostics)) {
                return false;
            }

            Source = {reinterpret_cast<const char*>(DiskSource.data()), DiskSource.size()};
        }

        if (Source.empty()) {
            Diagnostics = "Shader source is empty: " + Core::FilePath::ToGenericUtf8(Normalized.mSourcePath);

            return false;
        }

        Core::TArray<D3D_SHADER_MACRO> Macros{};
        Macros.reserve(Normalized.mMacros.size() + 1);

        for (const RenderCore::FShaderMacro& Macro : Normalized.mMacros) {
            Macros.push_back({Macro.mName.c_str(), Macro.mValue.c_str()});
        }

        Macros.push_back({nullptr, nullptr});

        FShaderIncludeHandler Includes{Normalized, Source.data(), RootDependency, OverridePath, EditedSource};
        Microsoft::WRL::ComPtr<ID3DBlob> Code{};
        Microsoft::WRL::ComPtr<ID3DBlob> Errors{};
        const Core::FString SourceName{Core::FilePath::ToGenericUtf8(Normalized.mSourcePath)};
        const HRESULT Result{D3DCompile(Source.data(), Source.size(), SourceName.c_str(), Macros.data(), &Includes, Normalized.mEntryPoint.c_str(), Normalized.mTargetProfile.c_str(), Normalized.mCompileFlags, 0, Code.GetAddressOf(), Errors.GetAddressOf())};

        if (Errors != nullptr) {
            Diagnostics.assign(static_cast<const char*>(Errors->GetBufferPointer()), Errors->GetBufferSize());

            while (!Diagnostics.empty() && Diagnostics.back() == '\0') {
                Diagnostics.pop_back();
            }
        }

        if (FAILED(Result) || Code == nullptr) {
            if (!Includes.GetDiagnostics().empty()) {
                Diagnostics += "\n" + Includes.GetDiagnostics();
            }

            if (Diagnostics.empty()) {
                Diagnostics = "D3DCompile failed: " + SourceName;
            }

            return false;
        }

        if (Compilation == nullptr) {
            return true;
        }

        const auto BuildTime{std::filesystem::file_time_type::clock::now()};

        if (!RenderCore::ShaderFile::IsSnapshotCurrent(Includes.GetDependencies())) {
            Diagnostics = "Shader dependencies changed during compilation; retry the request: " + SourceName;

            return false;
        }

        RenderCore::FShaderCompilation Candidate{};
        const auto* Begin{static_cast<const Uint8*>(Code->GetBufferPointer())};

        Candidate.mByteCode.assign(Begin, Begin + Code->GetBufferSize());
        Candidate.mDependencies = Includes.GetDependencies();
        Candidate.mBuildTime = BuildTime;
        *Compilation = std::move(Candidate);

        return true;
    }
}

/**
 * @brief 정규화한 설명으로 HLSL을 컴파일하고 바이트코드와 의존성을 수집합니다.
 */
bool FShaderCompiler::Compile(const FShaderDescription& Description, FShaderCompilation& Compilation, Core::FString& Diagnostics) const {
    return CompileSource(Description, {}, {}, &Compilation, Diagnostics);
}

bool FShaderCompiler::ValidateSource(const FShaderDescription& Description, const std::filesystem::path& EditedPath, std::string_view EditedSource, Core::FString& Diagnostics) const {
    return CompileSource(Description, EditedPath, EditedSource, nullptr, Diagnostics);
}

#include "FParticleRenderer.h"
#include "RendererStats.h"
#include "Asset/UMaterial.h"
#include "Engine/FSceneView.h"
#include "CoreUObject/Reflection/NewObject.h"

#include <algorithm>
#include <cmath>

namespace Renderer {

    namespace {

        constexpr Uint32 DefaultTextureSize{64};

    }

    Renderer::FParticleRenderer::FParticleRenderer() = default;

    Renderer::FParticleRenderer::~FParticleRenderer() = default;

    bool Renderer::FParticleRenderer::Initialize(ID3D11Device* Device) {
        D3D11_SAMPLER_DESC Sampler{};
        Sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        Sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        Sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        Sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        Sampler.MaxLOD = D3D11_FLOAT32_MAX;

        Core::FString Error{};

        if (!mConstants.Create(Device, Error) || FAILED(Device->CreateSamplerState(&Sampler, mSampler.GetAddressOf()))) {
            return false;
        }

        // 머티리얼이 없는 파티클도 보이도록 가장자리로 갈수록 투명해지는 흰 원을 만듭니다.
        Core::TArray<Uint8> Pixels(static_cast<Index_t>(DefaultTextureSize) * DefaultTextureSize * 4);

        for (Uint32 Y{}; Y < DefaultTextureSize; ++Y) {
            for (Uint32 X{}; X < DefaultTextureSize; ++X) {
                const float U{(static_cast<float>(X) + 0.5f) / static_cast<float>(DefaultTextureSize) * 2.0f - 1.0f};
                const float V{(static_cast<float>(Y) + 0.5f) / static_cast<float>(DefaultTextureSize) * 2.0f - 1.0f};
                const float Falloff{std::clamp(1.0f - std::sqrt(U * U + V * V), 0.0f, 1.0f)};
                const Index_t Offset{(static_cast<Index_t>(Y) * DefaultTextureSize + X) * 4};

                Pixels[Offset] = 255;
                Pixels[Offset + 1] = 255;
                Pixels[Offset + 2] = 255;
                Pixels[Offset + 3] = static_cast<Uint8>(std::lround(255.0f * Falloff * Falloff));
            }
        }

        mDefaultTexture = CoreUObject::NewObject<Asset::UTexture2D>();

        return mDefaultTexture->SetPixels(DefaultTextureSize, DefaultTextureSize, Asset::ETextureFormat::RGBA8, std::move(Pixels)) && mDefaultTexture->CreateResources(Device);
    }

    bool Renderer::FParticleRenderer::CreateShaders(ID3D11Device* Device, std::span<const Uint8> VertexCode, std::span<const Uint8> PixelCode) {
        Microsoft::WRL::ComPtr<ID3D11VertexShader> Vertex{};
        Microsoft::WRL::ComPtr<ID3D11PixelShader> Pixel{};

        if (FAILED(Device->CreateVertexShader(VertexCode.data(), VertexCode.size(), nullptr, Vertex.GetAddressOf())) || FAILED(Device->CreatePixelShader(PixelCode.data(), PixelCode.size(), nullptr, Pixel.GetAddressOf()))) {
            return false;
        }

        mVertexShader = std::move(Vertex);
        mPixelShader = std::move(Pixel);

        return true;
    }

    bool Renderer::FParticleRenderer::HasShaders() const {
        return mVertexShader != nullptr && mPixelShader != nullptr;
    }

    bool Renderer::FParticleRenderer::Draw(ID3D11DeviceContext* Context, const Engine::FParticleSpriteBatch& Batch, const Engine::FSceneView& View) {
        if (!HasShaders() || Batch.mInstances.empty()) {
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11Device> Device{};

        Context->GetDevice(Device.GetAddressOf());

        Asset::UMaterial* Material{Batch.mMaterial.Get()};
        Asset::UTexture2D* Texture{mDefaultTexture.get()};
        FVector4 Tint{1.0f, 1.0f, 1.0f, 1.0f};

        if (Material != nullptr && Material->CreateResources(Device.Get(), Context)) {
            Tint = Material->GetBaseColor();

            if (Material->GetBaseColorTexture() != nullptr) {
                Texture = Material->GetBaseColorTexture();
            }
        }

        if (Texture == nullptr || (!Texture->HasResources() && !Texture->CreateResources(Device.Get()))) {
            return false;
        }

        SortInstances(Batch, View);

        Core::FString Error{};

        if (!mInstances.Upload(Context, std::span<const Engine::FParticleSpriteInstance>{mScratch}, Error)) {
            return false;
        }

        const float Horizontal{static_cast<float>(std::max<Uint32>(Batch.mSubImagesHorizontal, 1))};
        const float Vertical{static_cast<float>(std::max<Uint32>(Batch.mSubImagesVertical, 1))};
        FConstants Constants{};

        Constants.mViewProjection = View.mViewProjection;
        Constants.mCameraRight = {View.mView(0, 0), View.mView(1, 0), View.mView(2, 0), 0.0f};
        Constants.mCameraUp = {View.mView(0, 1), View.mView(1, 1), View.mView(2, 1), 0.0f};
        Constants.mSubImageSize = {Horizontal, Vertical, 1.0f / Horizontal, 1.0f / Vertical};
        Constants.mTint = Tint;
        mConstants.Update(Context, Constants);

        ID3D11ShaderResourceView* Resource{Texture->GetShaderResourceView()};
        ID3D11SamplerState* Sampler{mSampler.Get()};

        Context->IASetInputLayout(nullptr);
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Context->VSSetShader(mVertexShader.Get(), nullptr, 0);
        Context->PSSetShader(mPixelShader.Get(), nullptr, 0);
        mConstants.Bind(Context, RenderCore::EShaderStage::Vertex | RenderCore::EShaderStage::Pixel, 0);
        mInstances.Bind(Context, RenderCore::EShaderStage::Vertex, 1);
        Context->PSSetShaderResources(0, 1, &Resource);
        Context->PSSetSamplers(0, 1, &Sampler);
        Context->DrawInstanced(6, static_cast<UINT>(mScratch.size()), 0, 0);
        Renderer::RendererStats::Triangles.Add(2.0 * static_cast<double>(mScratch.size()));

        ID3D11ShaderResourceView* Empty{};

        Context->PSSetShaderResources(0, 1, &Empty);
        RenderCore::TStructuredBuffer<Engine::FParticleSpriteInstance, RenderCore::EBufferUpdate::Dynamic>::Unbind(Context, RenderCore::EShaderStage::Vertex, 1);

        return true;
    }

    std::size_t Renderer::FParticleRenderer::GetResourceBytes() const {
        return sizeof(FConstants) + mInstances.GetResourceBytes() + (mDefaultTexture != nullptr ? mDefaultTexture->GetResourceBytes() : 0);
    }

    void Renderer::FParticleRenderer::SortInstances(const Engine::FParticleSpriteBatch& Batch, const Engine::FSceneView& View) {
        const Engine::EParticleSortMode Mode{Batch.mSortMode};

        if (Mode == Engine::EParticleSortMode::None) {
            mScratch.assign(Batch.mInstances.begin(), Batch.mInstances.end());

            return;
        }

        mSortKeys.resize(Batch.mInstances.size());

        for (Index_t Index{}; Index < Batch.mInstances.size(); ++Index) {
            const Engine::FParticleSpriteInstance& Instance{Batch.mInstances[Index]};
            const FVector Delta{FVector{Instance.mPosition[0], Instance.mPosition[1], Instance.mPosition[2]} - View.mLocation};
            float Key{};

            switch (Mode) {
            case Engine::EParticleSortMode::ViewProjDepth:
                Key = Delta.Dot(View.mForward);
                break;
            case Engine::EParticleSortMode::DistanceToView:
                Key = Delta.LengthSquared();
                break;
            default:
                Key = static_cast<float>(Instance.mCounter);
                break;
            }

            mSortKeys[Index] = {Key, static_cast<Uint32>(Index)};
        }

        // 거리와 깊이는 먼 것부터 그려야 알파 합성이 맞습니다. 나이순은 순번으로 정합니다.
        if (Mode == Engine::EParticleSortMode::AgeOldestFirst) {
            std::ranges::sort(mSortKeys, [](const auto& A, const auto& B) {
                return A.first < B.first;
            });
        } else {
            std::ranges::sort(mSortKeys, [](const auto& A, const auto& B) {
                return A.first > B.first;
            });
        }

        mScratch.resize(mSortKeys.size());

        for (Index_t Index{}; Index < mSortKeys.size(); ++Index) {
            mScratch[Index] = Batch.mInstances[mSortKeys[Index].second];
        }
    }

}

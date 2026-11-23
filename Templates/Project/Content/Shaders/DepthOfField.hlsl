/**
 * @file DepthOfField.hlsl
 * @brief 전경과 배경을 절반 해상도에서 분리하고 흐리게 한 뒤 원본 장면에 합성합니다.
 */

#include "PostProcess.hlsl"

Texture2D<float4> ForegroundLayer : register(t2);
Texture2D<float4> BackgroundLayer : register(t3);

struct FDepthOfFieldLayers {
    float4 mForeground : SV_TARGET0;
    float4 mBackground : SV_TARGET1;
};

// 원판에서 반경 제곱이 일정하게 증가하도록 배치한 표본입니다. 중심 표본은 별도로 읽습니다.
static const float2 DiskOffsets[24] = {
    float2(-0.15051f, 0.13788f),
    float2(0.02524f, -0.28757f),
    float2(0.21512f, 0.28058f),
    float2(-0.40201f, -0.07111f),
    float2(0.38512f, -0.24498f),
    float2(-0.12980f, 0.48286f),
    float2(-0.24892f, -0.47928f),
    float2(0.54232f, 0.19805f),
    float2(-0.56604f, 0.23365f),
    float2(0.27359f, -0.58465f),
    float2(0.20262f, 0.64597f),
    float2(-0.61180f, -0.35455f),
    float2(0.71881f, -0.15803f),
    float2(-0.43926f, 0.62481f),
    float2(-0.10160f, -0.78401f),
    float2(0.62433f, 0.52619f),
    float2(-0.84091f, 0.03477f),
    float2(0.61386f, -0.61088f),
    float2(-0.04110f, 0.88881f),
    float2(-0.58488f, -0.70089f),
    float2(0.92706f, 0.12473f),
    float2(-0.78591f, 0.54682f),
    float2(0.21486f, -0.95508f),
    float2(0.49718f, 0.86765f)
};

float2 GetLayerWeights(float Depth) {
    if (Depth >= 1.0f) {
        return float2(0.0f, 1.0f);
    }

    return ComputeDepthOfFieldWeights(ReconstructViewDepth(Depth), DepthOfFieldParameters.x, DepthOfFieldParameters.y, DepthOfFieldParameters.z);
}

FDepthOfFieldLayers SeparateDepthOfField(FVertexOutput Input) {
    const int2 FirstPixel = {int2(Input.mPosition.xy) * 2};
    const int2 LastPixel = {int2(ScreenSize.xy) - 1};
    FDepthOfFieldLayers Output = {(FDepthOfFieldLayers)0};

    [unroll]
    for (int Y = 0; Y < 2; ++Y) {
        [unroll]
        for (int X = 0; X < 2; ++X) {
            const int3 Pixel = {int3(min(FirstPixel + int2(X, Y), LastPixel), 0)};
            const float2 Weights = {GetLayerWeights(SceneDepth.Load(Pixel))};
            const float4 Color = {float4(SceneColor.Load(Pixel).rgb, 1.0f)};

            // 색상과 커버리지를 함께 필터링해 레이어 경계에서 색이 섞이거나 어두워지는 현상을 줄입니다.
            Output.mForeground += Color * Weights.x;
            Output.mBackground += Color * Weights.y;
        }
    }

    Output.mForeground *= 0.25f;
    Output.mBackground *= 0.25f;

    return Output;
}

FDepthOfFieldLayers BlurDepthOfField(FVertexOutput Input) {
    const float2 HalfSize = {ceil(ScreenSize.xy * 0.5f)};
    const float2 RadiusUV = {0.5f * DepthOfFieldParameters.w / HalfSize};
    FDepthOfFieldLayers Output = {(FDepthOfFieldLayers)0};
    Output.mForeground = ForegroundLayer.SampleLevel(LinearClamp, Input.mUV, 0.0f);
    Output.mBackground = BackgroundLayer.SampleLevel(LinearClamp, Input.mUV, 0.0f);

    [unroll]
    for (int Index = 0; Index < 24; ++Index) {
        const float2 UV = {Input.mUV + DiskOffsets[Index] * RadiusUV};
        Output.mForeground += ForegroundLayer.SampleLevel(LinearClamp, UV, 0.0f);
        Output.mBackground += BackgroundLayer.SampleLevel(LinearClamp, UV, 0.0f);
    }

    Output.mForeground /= 25.0f;
    Output.mBackground /= 25.0f;

    return Output;
}

float4 CompositeDepthOfField(FVertexOutput Input) : SV_TARGET {
    const int3 Pixel = {int3(int2(Input.mPosition.xy), 0)};
    const float4 Original = {SceneColor.Load(Pixel)};
    const float2 HalfSize = {ceil(ScreenSize.xy * 0.5f)};
    const float2 LayerUV = {Input.mPosition.xy * 0.5f / HalfSize};
    const float BackgroundWeight = {GetLayerWeights(SceneDepth.Load(Pixel)).y};
    float3 Color = {Original.rgb};

    // 전체 해상도 깊이로 배경을 합성해 초점 물체의 경계를 보존합니다.
    [branch]
    if (BackgroundWeight > 0.0f) {
        const float4 Background = {BackgroundLayer.SampleLevel(LinearClamp, LayerUV, 0.0f)};

        if (Background.a > 0.0001f) {
            Color = lerp(Color, Background.rgb / Background.a, BackgroundWeight);
        }
    }

    const float4 Foreground = {ForegroundLayer.SampleLevel(LinearClamp, LayerUV, 0.0f)};
    Color = Foreground.rgb + Color * (1.0f - saturate(Foreground.a));

    // 반경이 1px보다 작으면 절반 해상도 필터의 영향을 원본으로 부드럽게 줄입니다.
    return float4(lerp(Original.rgb, Color, saturate(DepthOfFieldParameters.w)), Original.a);
}

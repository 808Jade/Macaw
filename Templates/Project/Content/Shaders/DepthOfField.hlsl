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

// 외접원 반경 1인 조리개에서 화면 좌표 Y의 가로 반폭을 계산합니다.
static const float2 PentagonShoulder = {0.9510565f, -0.3090170f};
static const float2 PentagonBaseCorner = {0.5877853f, 0.8090170f};

float ApertureHalfWidth(float Y, float Sides) {
    if (Sides == 5.0f) {
        const float UpperWidth = {PentagonShoulder.x * (Y + 1.0f) / (PentagonShoulder.y + 1.0f)};
        const float LowerT = {saturate((Y - PentagonShoulder.y) / (PentagonBaseCorner.y - PentagonShoulder.y))};
        const float LowerWidth = {lerp(PentagonShoulder.x, PentagonBaseCorner.x, LowerT)};

        return min(UpperWidth, LowerWidth);
    }

    if (Sides == 6.0f) {
        return 0.8660254f * saturate(2.0f * (1.0f - abs(Y)));
    }

    return sqrt(saturate(1.0f - Y * Y));
}

float ApertureSampleCoverage(float X, float HalfWidth) {
    return max(min(X + 0.5f, HalfWidth) - max(X - 0.5f, -HalfWidth), 0.0f);
}

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
    const float HalfRadius = {max(0.5f * DepthOfFieldParameters.w, 0.001f)};
    // 큰 반경에서는 표본 간격을 넓혀 커널이 읽는 텍셀 수를 제한합니다.
    const float SampleSpacing = {max(1.0f, HalfRadius / 10.0f)};
    const float Radius = {HalfRadius / SampleSpacing};
    const float ApertureBottom = {Radius * (Settings.y == 5.0f ? PentagonBaseCorner.y : 1.0f)};
    const float2 SampleStepUV = {SampleSpacing / HalfSize};
    const int RowExtent = {(int)ceil(Radius)};
    const bool PairSamples = {SampleSpacing == 1.0f};
    const int ColumnStep = {PairSamples ? 2 : 1};
    FDepthOfFieldLayers Output = {(FDepthOfFieldLayers)0};
    float TotalCoverage = {0.0f};

    [loop]
    for (int Y = -RowExtent; Y <= RowExtent; ++Y) {
        const float Top = {max(Y - 0.5f, -Radius)};
        const float Bottom = {min(Y + 0.5f, ApertureBottom)};
        const float RowCoverage = {Bottom - Top};

        if (RowCoverage <= 0.0f) {
            continue;
        }

        const float HalfWidth = {Radius * ApertureHalfWidth((Top + Bottom) * 0.5f / Radius, Settings.y)};
        const int ColumnExtent = {(int)ceil(HalfWidth - 0.5f)};

        [loop]
        for (int X = -ColumnExtent; X <= ColumnExtent; X += ColumnStep) {
            const float First = {ApertureSampleCoverage(X, HalfWidth)};
            const float Second = {PairSamples ? ApertureSampleCoverage(X + 1, HalfWidth) : 0.0f};
            const float Coverage = {First + Second};
            const float Weight = {RowCoverage * Coverage};

            // 한 텍셀 간격의 두 표본을 선형 필터로 묶어 같은 가중합을 한 번에 읽습니다.
            const float SampleX = {X + Second / max(Coverage, 0.000001f)};
            const float2 UV = {Input.mUV - float2(SampleX, Y) * SampleStepUV};
            Output.mForeground += ForegroundLayer.SampleLevel(LinearClamp, UV, 0.0f) * Weight;
            Output.mBackground += BackgroundLayer.SampleLevel(LinearClamp, UV, 0.0f) * Weight;
            TotalCoverage += Weight;
        }
    }

    Output.mForeground /= TotalCoverage;
    Output.mBackground /= TotalCoverage;

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

#include "PostProcess.hlsli"

// --- 定数定義 ---
static const float32_t PI = 3.14159265f;

// --- リソース ---
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// --- Gauss関数の実装 (スライドの定義に基づく) ---
float32_t gauss(float32_t x, float32_t y, float32_t sigma)
{
    float32_t exponent = -(x * x + y * y) * rcp(2.0f * sigma * sigma);
    float32_t denominator = 2.0f * PI * sigma * sigma;
    return exp(exponent) * rcp(denominator);
}

// --- メイン関数 ---
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    uint32_t width, height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = rcp(float32_t2(width, height));

    float32_t3 colorSum = (float32_t3) 0.0f;
    float32_t weightSum = 0.0f;
    
    // 標準偏差σ (スライドに合わせて 2.0f)
    float32_t sigma = 2.0f;
    
    int32_t k = gKernelSize;

    // 畳み込み演算
    for (int32_t x = -k; x <= k; ++x)
    {
        for (int32_t y = -k; y <= k; ++y)
        {
            // 現在の座標(x, y)における重みを求める
            float32_t weight = gauss((float32_t) x, (float32_t) y, sigma);
            
            // 重み付きで色を加算
            float32_t2 offset = float32_t2((float32_t) x, (float32_t) y) * uvStepSize;
            colorSum += gTexture.Sample(gSampler, input.texcoord + offset).rgb * weight;
            
            // 後で正規化するために重みの合計を貯める
            weightSum += weight;
        }
    }

    // 最後に重みの合計で割る（正規化）ことで明るさを維持
    output.color.rgb = colorSum * rcp(weightSum);
    output.color.a = 1.0f;

    return output;
}
#include "PostProcess.hlsli"

// --- リソース (Texture & Sampler) ---
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);


// --- メイン関数 ---
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 1. テクスチャサイズから1ピクセルあたりのUV移動量を計算
    uint32_t width, height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = rcp(float32_t2(width, height));

    // 2. 周辺ピクセルの色の合計を求める
    float32_t3 colorSum = (float32_t3) 0.0f;
    int32_t k = gKernelSize;

    // 動的な二重ループで周囲 (2k+1)x(2k+1) 範囲を走査
    for (int32_t x = -k; x <= k; ++x)
    {
        for (int32_t y = -k; y <= k; ++y)
        {
            // 現在のUV座標にピクセル単位のオフセットを加算
            float32_t2 offset = float32_t2(x, y) * uvStepSize;
            colorSum += gTexture.Sample(gSampler, input.texcoord + offset).rgb;
        }
    }

    // 3. 合計ピクセル数で割って平均化（ボックスフィルタ）
    float32_t numPixels = (float32_t) ((2 * k + 1) * (2 * k + 1));
    output.color.rgb = colorSum / numPixels;
    output.color.a = 1.0f;

    return output;
}
#include "PostProcess.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// --- 擬似乱数生成関数 ---
// UV座標と時間を使って 0.0 ～ 1.0 の値を返す
float32_t Random(float32_t2 uv)
{
    return frac(sin(dot(uv, float32_t2(12.9898, 78.233) + randomTime)) * 43758.5453);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 1. 元の色をサンプリング
    float32_t4 color = gTexture.Sample(gSampler, input.texcoord);

    // 2. ランダムなノイズを生成
    float32_t noise = Random(input.texcoord);

    // 3. ノイズを元の色に加算（または乗算）
    // randomIntensity で強さを調整
    color.rgb += noise * randomIntensity;

    output.color = color;
    return output;
}
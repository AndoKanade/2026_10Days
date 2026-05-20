#include "PostProcess.hlsli"

// --- リソース (Texture & Sampler) ---
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);


// --- メイン関数 ---
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // 1. テクスチャから元の色をサンプリング
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // 2. 中心からの距離に基づいたビネット係数の計算
    // UV(0.0~1.0) を中心(0.5, 0.5)からの相対座標に変換
    float32_t2 centeredUV = input.texcoord - 0.5f;
    float32_t distance = length(centeredUV);

    // 3. スケールと強度を適用
    // 距離が遠くなるほど小さい値になるよう反転し、指数で減衰を調整
    float32_t vignette = saturate(1.0f - distance * gVignetteScale);
    vignette = pow(vignette, gVignetteIntensity);

    // 4. 元の色に係数を掛けて出力
    output.color.rgb = textureColor.rgb * vignette;
    output.color.a = textureColor.a;
    
    return output;
}
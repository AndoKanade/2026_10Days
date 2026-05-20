#include "PostProcess.hlsli"

// --- リソース (Texture & Sampler) ---
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);


// --- メイン関数 ---
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // 1. テクスチャから元の色をそのままサンプリング
    // 何も加工せず出力することで、ポストプロセス無しの状態を作る
    output.color = gTexture.Sample(gSampler, input.texcoord);
    
    return output;
}
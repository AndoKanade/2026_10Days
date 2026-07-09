#include "Particle.hlsli"

// テクスチャとサンプラーの定義
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // 1. テクスチャの色を取得
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // 2. 最終的な色を決定 (テクスチャ色 × パーティクル色)
    output.color = textureColor * input.color;
    
    // 3. 透明部分の破棄 (Discard)
    if (output.color.a <= 0.0f)
    {
        discard;
    }
    
    return output;
}
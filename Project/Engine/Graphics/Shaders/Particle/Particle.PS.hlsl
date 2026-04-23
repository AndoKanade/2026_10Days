#include "Particle.hlsli"

// テクスチャとサンプラーのみ定義します
// (C++のルートシグネチャ設定と一致させます)
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
    // UV変換行列(uvTransform)も、個別のパーティクルでは通常使わないので単純に input.texcoord を使います
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // 2. 最終的な色を決定
    // テクスチャの色 × 頂点シェーダーから来たパーティクルの色(input.color)
    output.color = textureColor * input.color;
    
    // 3. 透明部分の破棄 (Discard)
    // アルファが0なら描画しない
    if (output.color.a == 0.0f)
    {
        discard;
    }
    
    return output;
}
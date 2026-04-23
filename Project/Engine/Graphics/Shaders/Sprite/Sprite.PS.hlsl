#include "Sprite.hlsli"

// ★ここに struct Material を書いてはいけません（hlsliにあるから）
// ★ DirectionalLight (b2) も書いてはいけません（C++から送ってないから）

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UV変換
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    
    // テクスチャの色を取得
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // ライト計算はせず、そのままの色を出力
    output.color = gMaterial.color * textureColor;
    
    // 透明部分の除外
    if (textureColor.a == 0.0f)
    {
        discard;
    }
    if (output.color.a == 0.0f)
    {
        discard;
    }
    
    return output;
}
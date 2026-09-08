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
    
    // マスク表示では元画像のRGBを無視し、透明度だけを形として指定色で描く。
    // Spriteでは使っていないenableLightingをマスク切り替えフラグとして利用する。
    float32_t4 sampledColor = textureColor;
    if (gMaterial.enableLighting != 0)
    {
        sampledColor.rgb = float32_t3(1.0f, 1.0f, 1.0f);
    }
    output.color = gMaterial.color * sampledColor;
    
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

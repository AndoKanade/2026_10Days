#include "Skybox.hlsli"

struct Material
{
    float32_t4 color;
};

ConstantBuffer<Material> gMaterial : register(b0);
TextureCube<float32_t4> gTexture : register(t0); // CubeMap用のテクスチャ
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    // CubeMapのサンプリング (正規化した方向ベクトルを使用)
    float32_t4 textureColor = gTexture.Sample(gSampler, normalize(input.texcoord));
    
    // マテリアルカラーと合成
    output.color = gMaterial.color * textureColor;
    
    return output;
}
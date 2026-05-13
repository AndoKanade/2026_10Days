#include "Particle.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
};

struct ParticleForGPU
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4 color;
    float32_t2 uvOffset;
};

StructuredBuffer<ParticleForGPU> gParticle : register(t0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

	// 行列による座標変換
    output.position = mul(input.position, gParticle[instanceId].WVP);

	// 元のUV座標に、インスタンスごとのオフセット値を足す
    output.texcoord = input.texcoord + gParticle[instanceId].uvOffset;

	// 法線の変換
    output.normal = normalize(mul(input.normal, (float32_t3x3) gParticle[instanceId].World));

	// 色の受け渡し
    output.color = gParticle[instanceId].color;

    return output;
}
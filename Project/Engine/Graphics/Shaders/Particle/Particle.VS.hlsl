#include "Particle.hlsli"

// --- データ構造体 ---

struct Particle
{
    float3 translate;
    float padding1;

    float3 scale;
    float lifeTime;

    float3 velocity;
    float currentTime;

    float4 color;

    float2 uvOffset;
    uint particleType;
    float padding2;
};

struct PerView
{
    float32_t4x4 viewProjection;
    float32_t4x4 billboardMatrix;
};

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

// --- リソースバインド ---

StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

// --- Vertex Shader メイン ---

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    // 1. パーティクルデータの取得
    Particle particle = gParticles[instanceId];

    // 2. ワールド行列の構築 (ビルボード行列ベース)
    float32_t4x4 worldMatrix = gPerView.billboardMatrix;
    
    worldMatrix[0] *= particle.scale.x;
    worldMatrix[1] *= particle.scale.y;
    worldMatrix[2] *= particle.scale.z;
    worldMatrix[3].xyz = particle.translate;

    // 3. 座標変換 (WVP)
    output.position = mul(input.position, mul(worldMatrix, gPerView.viewProjection));

    // 4. UV座標と色の出力
    output.texcoord = input.texcoord + particle.uvOffset;
    output.color = particle.color;

    return output;
}
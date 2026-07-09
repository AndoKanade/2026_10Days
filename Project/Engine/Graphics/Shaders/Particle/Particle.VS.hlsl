#include "Particle.hlsli"

// CPUから送るパーティクルの生データ
struct Particle
{
    float32_t3 translate; // 移動量
    float32_t3 scale; // スケール
    float32_t lifeTime; // 寿命
    float32_t3 velocity; // 速度
    float32_t currentTime; // 経過時間
    float4 color; // 色
    float32_t2 uvOffset;
};

// カメラ情報など（ビュー変換用）
struct PerView
{
    float32_t4x4 viewProjection;
    float32_t4x4 billboardMatrix;
};

// 修正：StructuredBufferの中身をスライドの設計に合わせる
StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    // インスタンスIDからパーティクルデータを取得
    Particle particle = gParticles[instanceId];

    // ビルボード行列をベースにワールド行列を構築
    float32_t4x4 worldMatrix = gPerView.billboardMatrix;
    
    // スケール適用
    worldMatrix[0] *= particle.scale.x;
    worldMatrix[1] *= particle.scale.y;
    worldMatrix[2] *= particle.scale.z;
    worldMatrix[3].xyz = particle.translate;

    // 座標変換 (WVP)
    output.position = mul(input.position, mul(worldMatrix, gPerView.viewProjection));

    // UVと色
    output.texcoord = input.texcoord;
    output.color = particle.color;
    
    output.texcoord = input.texcoord + particle.uvOffset;

    return output;
}
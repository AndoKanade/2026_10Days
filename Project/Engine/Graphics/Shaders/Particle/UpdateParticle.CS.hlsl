// --- データ構造体 (C++側とメモリレイアウトを一致) ---

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

// --- リソースバインド ---

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeCounter : register(u1);

// --- 定数定義 ---

static const float kDeltaTime = 1.0f / 60.0f;
static const uint32_t kMaxParticles = 1024;

// --- メイン処理 (パーティクル更新) ---

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;

    // 範囲外スレッドは終了
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    // 生存しているパーティクルのみ更新処理を行う
    if (gParticles[particleIndex].currentTime < gParticles[particleIndex].lifeTime)
    {
        // 経過時間を進める
        gParticles[particleIndex].currentTime += kDeltaTime;

        // 速度に基づいて位置を移動
        gParticles[particleIndex].translate += gParticles[particleIndex].velocity * kDeltaTime;
        
        // 寿命に応じた透明度の計算
        float alpha = 1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
        gParticles[particleIndex].color.a = saturate(alpha);
    }
}
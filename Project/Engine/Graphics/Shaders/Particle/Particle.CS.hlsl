// Particle.CS.hlsl

struct Particle
{
    float32_t3 translate;
    float32_t3 scale;
    float32_t lifeTime;
    float32_t3 velocity;
    float32_t currentTime;
    float4 color;
    float32_t2 uvOffset;
};

struct ParView
{
    float32_t4x4 viewProjection;
    float32_t4x4 billboardMatrix;
};

static const uint32_t kMaxParticles = 1024;
RWStructuredBuffer<Particle> gParticles : register(u0);

// 追加
static const float kDeltaTime = 1.0f / 60.0f;

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;
    
    if (particleIndex < kMaxParticles)
    {
        // 変更 寿命が残っている（生きている）パーティクルのみ処理を行う
        if (gParticles[particleIndex].lifeTime > gParticles[particleIndex].currentTime)
        {
            // 時間を進める
            gParticles[particleIndex].currentTime += kDeltaTime;

            // 速度を加算して移動させる
            gParticles[particleIndex].translate += gParticles[particleIndex].velocity * kDeltaTime;

            // 時間経過で透明にする
            float alpha = 1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
            gParticles[particleIndex].color.a = saturate(alpha);
        }
    }
}
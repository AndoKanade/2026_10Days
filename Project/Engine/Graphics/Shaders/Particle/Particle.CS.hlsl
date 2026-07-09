// パーティクルデータ構造
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

// ビュー行列用構造体
struct ParView
{
    float32_t4x4 viewProjection;
    float32_t4x4 billboardMatrix;
};

// 定数定義
static const uint32_t kMaxParticles = 1024;
static const float kDeltaTime = 1.0f / 60.0f;

// リソース定義
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeCounter : register(u1);

// 更新処理 (256スレッド/グループ)
[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;
    
    if (particleIndex < kMaxParticles)
    {
        // 生存中のパーティクルのみ更新
        if (gParticles[particleIndex].lifeTime > gParticles[particleIndex].currentTime)
        {
            
            // 時間の経過
            gParticles[particleIndex].currentTime += kDeltaTime;

            // 速度による移動
            gParticles[particleIndex].translate += gParticles[particleIndex].velocity * kDeltaTime;

            // 時間経過によるアルファ値（透明度）の更新
            float alpha = 1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
            gParticles[particleIndex].color.a = saturate(alpha);
        }
    }
    
    // カウンターのリセット（インデックス0のスレッドが担当）
    if (particleIndex == 0)
    {
        gFreeCounter[0] = 0;
    }
}
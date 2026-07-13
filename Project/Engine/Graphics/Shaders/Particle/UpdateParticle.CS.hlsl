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

// リソースバインド
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

// 定数定義
static const float kDeltaTime = 1.0f / 60.0f;
static const uint32_t kMaxParticles = 1024;

// メイン処理（パーティクルの更新と寿命による回収）
[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;

    // 範囲外スレッドは終了
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    // 生存中のパーティクルのみ更新処理を行う
    if (gParticles[particleIndex].currentTime < gParticles[particleIndex].lifeTime)
    {
        // 時間経過と位置の更新
        gParticles[particleIndex].currentTime += kDeltaTime;
        gParticles[particleIndex].translate += gParticles[particleIndex].velocity * kDeltaTime;

        // アルファ値（透明度）の計算
        float alpha = 1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
        gParticles[particleIndex].color.a = saturate(alpha);

        // 寿命を迎えたパーティクルの回収処理
        if (gParticles[particleIndex].color.a == 0.0f)
        {
            // スケールを0にして描画対象から除外する
            gParticles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);
            
            // FreeListにインデックスを戻す
            int32_t freeListIndex;
            InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
            
            // 安全な範囲であればリストにインデックスを格納
            if ((freeListIndex + 1) < kMaxParticles)
            {
                gFreeList[freeListIndex + 1] = particleIndex;
            }
            else
            {
                // 想定外の状況に対する安全策
                InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
            }
        }
    }
}
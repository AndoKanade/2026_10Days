// --- 乱数生成ヘルパー ---

float32_t rand3dToid(float32_t3 value)
{
    return frac(sin(dot(value, float32_t3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);
}

float32_t3 rand3dTo3d(float32_t3 value)
{
    return float32_t3(
        rand3dToid(value),
        rand3dToid(value + float32_t3(0.123f, 0.456f, 0.789f)),
        rand3dToid(value + float32_t3(0.321f, 0.654f, 0.987f))
    );
}

class RandomGenerator
{
    float32_t3 seed;

    float32_t3 Generate3d()
    {
        seed = rand3dTo3d(seed);
        return seed;
    }

    float32_t GenerateId()
    {
        float32_t result = rand3dToid(seed);
        seed.x = result;
        return result;
    }
};

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

struct EmitterSphere
{
    float3 translate;
    float radius;
    uint count;
    float frequency;
    float frequencyTime;
    uint emit;
    float4 color;
    float3 velocity;
    float lifeTime;
    uint particleType;
};

struct PerFrame
{
    float time;
    float deltaTime;
};

// --- リソースバインド ---

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<Particle> gParticles : register(u0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
RWStructuredBuffer<int32_t> gFreeCounter : register(u1);

static const uint32_t kMaxParticles = 1024;

// --- メイン処理 (パーティクル発生) ---

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit != 0)
    {
        RandomGenerator generator;
        // シード値の計算
        generator.seed = (DTid + gPerFrame.time + 1.0f) * gPerFrame.time;

        for (uint32_t countIndex = 0; countIndex < gEmitter.count; ++countIndex)
        {
            int32_t particleIndex;
            // カウンターを加算してインデックスを取得
            InterlockedAdd(gFreeCounter[0], 1, particleIndex);

            // 1. リングバッファ化 (最大数でループ)
            particleIndex = (particleIndex % kMaxParticles);

            // 2. パーティクル初期値の設定
            float3 randomOffset = (generator.Generate3d() - 0.5f) * 2.0f;
            
            gParticles[particleIndex].translate = gEmitter.translate + (randomOffset * gEmitter.radius);
            gParticles[particleIndex].scale = float3(1.0f, 1.0f, 1.0f);
            gParticles[particleIndex].color = gEmitter.color;
            gParticles[particleIndex].lifeTime = gEmitter.lifeTime;
            gParticles[particleIndex].currentTime = 0.0f;
            gParticles[particleIndex].velocity = gEmitter.velocity + (generator.Generate3d() - 0.5f) * 2.0f;
        }
    }
}
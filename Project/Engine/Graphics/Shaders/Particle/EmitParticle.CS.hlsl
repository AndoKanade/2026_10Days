// 3Dベクトルから1Dのランダム値を生成するハッシュ関数
float32_t rand3dToid(float32_t3 value)
{
    return frac(sin(dot(value, float32_t3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);
}

// 3Dベクトルから3Dのランダム値を生成するハッシュ関数
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

struct EmitterSphere
{
    float32_t3 translate;
    float32_t radius;
    uint32_t count;
    float32_t frequency;
    float32_t frequencyTime;
    uint32_t emit;
};

struct PerFrame
{
    float32_t time;
    float32_t deltaTime;
};

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<Particle> gParticles : register(u0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit != 0)
    {
        RandomGenerator generator;
        generator.seed = (DTid + gPerFrame.time) * gPerFrame.time;
        
        for (uint32_t countIndex = 0; countIndex < gEmitter.count; ++countIndex)
        {
            gParticles[countIndex].scale = generator.Generate3d();
            gParticles[countIndex].translate = generator.Generate3d();
            gParticles[countIndex].color.rgb = generator.Generate3d();
            gParticles[countIndex].color.a = 1.0f;
            gParticles[countIndex].lifeTime = 1.0f;
            gParticles[countIndex].currentTime = 0.0f;
            gParticles[countIndex].velocity = float32_t3(0.0f, 1.0f, 0.0f);
        }
    }
}
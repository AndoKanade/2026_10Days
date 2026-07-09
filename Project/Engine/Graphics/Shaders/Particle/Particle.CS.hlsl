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


[numthreads(1024, 1, 1)]
void main( uint32_t3 DTid : SV_DispatchThreadID )
{
    uint32_t particleIndex = DTid.x;
    
    if (particleIndex < kMaxParticles)
    {
        gParticles[particleIndex] = (Particle)0;
        gParticles[particleIndex].scale = float32_t3(0.5f, 0.5f, 0.5f);
        gParticles[particleIndex].color = float32_t4(1.0f, 1.0f, 1.0f, 1.0f);
        gParticles[particleIndex].uvOffset = float32_t2(0.0f, 0.0f);
    }
    
}
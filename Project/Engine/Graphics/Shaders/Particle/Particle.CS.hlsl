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

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

static const uint32_t kMaxParticles = 1024;
static const float kDeltaTime = 1.0f / 60.0f;

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;

    if (particleIndex < kMaxParticles)
    {
        if (gParticles[particleIndex].lifeTime > gParticles[particleIndex].currentTime)
        {
            gParticles[particleIndex].currentTime += kDeltaTime;

            if (gParticles[particleIndex].currentTime >= gParticles[particleIndex].lifeTime)
            {
                int32_t freeIndex;
                InterlockedAdd(gFreeListIndex[0], 1, freeIndex);
                gFreeList[freeIndex] = particleIndex;
            }

            gParticles[particleIndex].translate += gParticles[particleIndex].velocity * kDeltaTime;

            float alpha = 1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
            gParticles[particleIndex].color.a = saturate(alpha);
        }
    }
}
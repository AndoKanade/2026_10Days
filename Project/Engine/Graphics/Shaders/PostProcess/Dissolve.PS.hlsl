#include "PostProcess.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gMaskTexture : register(t1);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float32_t mask = gMaskTexture.Sample(gSampler, input.texcoord);

    if (mask <= dissolveThreshold)
    {
        discard;
    }

    float32_t edgeWeight = 1.0f - smoothstep(dissolveThreshold, dissolveThreshold + dissolveEdgeWidth, mask);

    output.color = gTexture.Sample(gSampler, input.texcoord);

    output.color.rgb += edgeWeight * dissolveEdgeColor;
    
    return output;
}
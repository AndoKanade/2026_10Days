struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL0; 
    float2 texcoord : TEXCOORD0;
};
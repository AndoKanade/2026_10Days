// --- 構造体定義 ---

struct Vertex
{
    float4 position;
    float2 texcoord;
    float3 normal;
    float4 weight;
    int4 index;
};

struct VertexInfluence
{
    float4 weight;
    int4 index;
};

struct MatrixPalette
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

struct SkinningInformation
{
    uint numVertices;
};

// --- バッファ宣言 ---

StructuredBuffer<MatrixPalette> gMatrixPalette : register(t0);
StructuredBuffer<Vertex> gInputVertices : register(t1);
StructuredBuffer<VertexInfluence> gInfluences : register(t2);
RWStructuredBuffer<Vertex> gOutputVertices : register(u0);
ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0);

// --- メイン処理 ---

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vertexIndex = DTid.x;

    if (vertexIndex < gSkinningInformation.numVertices)
    {
        Vertex input = gInputVertices[vertexIndex];
        VertexInfluence influence = gInfluences[vertexIndex];
        
        Vertex skinned;
        skinned.texcoord = input.texcoord;
        skinned.weight = influence.weight;
        skinned.index = influence.index;
        
        float4 position = float4(0.0f, 0.0f, 0.0f, 0.0f);
        float3 normal = float3(0.0f, 0.0f, 0.0f);
        
        // 4つのボーンウェイトを適用してスキニング計算
        for (int i = 0; i < 4; ++i)
        {
            float weight = influence.weight[i];
            int index = influence.index[i];
            
            // 頂点位置と法線のスキニング
            position += mul(float4(input.position.xyz, 1.0f), gMatrixPalette[index].skeletonSpaceMatrix) * weight;
            normal += mul(input.normal, (float3x3) gMatrixPalette[index].skeletonSpaceInverseTransposeMatrix) * weight;
        }

        skinned.position = position;
        skinned.normal = normalize(normal);
        
        gOutputVertices[vertexIndex] = skinned;
    }
}
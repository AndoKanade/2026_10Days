#include "Object3d.hlsli"

// ボーンごとの行列構造体
struct MatrixPalette
{
    row_major float4x4 skeletonSpaceMatrix;
    row_major float4x4 skeletonSpaceInverseTransposeMatrix;
};

// 頂点入力構造体
struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 weight : WEIGHT0;
    int32_t4 index : INDEX0;
};

// 光源データ構造体
struct LightData
{
    float3 direction;
    float intensity;
};

// 変換行列用定数バッファ構造体
struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

// スキニング後の頂点データ構造体
struct Skinned
{
    float32_t4 position;
    float32_t3 normal;
};

// バッファの登録
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);
StructuredBuffer<MatrixPalette> gMatrixPalette : register(t0);
ConstantBuffer<LightData> gLightData : register(b2);

// スキニング処理
Skinned Skinning(VertexShaderInput input)
{
    Skinned skinned;

    skinned.position = float4(0.0f, 0.0f, 0.0f, 0.0f);
    skinned.normal = float3(0.0f, 0.0f, 0.0f);

    // インデックスとウェイトをもとに頂点座標と法線を計算
    skinned.position += mul(input.position, gMatrixPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.x].skeletonSpaceInverseTransposeMatrix) * input.weight.x;

    skinned.position += mul(input.position, gMatrixPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.y].skeletonSpaceInverseTransposeMatrix) * input.weight.y;

    skinned.position += mul(input.position, gMatrixPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.z].skeletonSpaceInverseTransposeMatrix) * input.weight.z;

    skinned.position += mul(input.position, gMatrixPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.w].skeletonSpaceInverseTransposeMatrix) * input.weight.w;

    skinned.position = skinned.position;
    skinned.position.w = 1.0f;
    skinned.normal = normalize(skinned.normal);

    return skinned;
}

// 頂点シェーダーメイン処理
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // スキニングを実行
    Skinned skinned = Skinning(input);

    // 各種行列を乗算して出力データを構築
    output.worldPosition = mul(skinned.position, gTransformationMatrix.World).xyz;
    output.position = mul(skinned.position, gTransformationMatrix.WVP);
    output.normal = mul(skinned.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose);
    output.texcoord = input.texcoord;

    return output;
}
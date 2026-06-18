#include "Object3d.hlsli"

// ボーンごとの行列構造体
struct MatrixPalette
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

// 頂点入力構造体
struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 weight : WEIGHT0;
    int4 index : INDEX0;
};

// 光源データ
struct LightData
{
    float3 direction;
    float intensity;
};

// 変換行列用定数バッファ
struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

// バッファの登録
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);
StructuredBuffer<MatrixPalette> gMatrixPalette : register(t0); // 構造体配列としてバインド
ConstantBuffer<LightData> gLightData : register(b2);

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // スキニング後の位置と法線を初期化
    float4 skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);

    // ウェイトによる行列のブレンド計算
    for (int i = 0; i < 4; ++i)
    {
        if (input.weight[i] > 0.0f)
        {
            uint jointIdx = (uint) input.index[i];
            MatrixPalette bone = gMatrixPalette[jointIdx];

            // 1. 位置のスキニング計算
            skinnedPosition += mul(input.position, bone.skeletonSpaceMatrix) * input.weight[i];
            
            // 2. 法線のスキニング計算 (3x3行列で回転・スケールを適用)
            skinnedNormal += mul(input.normal, (float3x3) bone.skeletonSpaceInverseTransposeMatrix) * input.weight[i];
        }
    }
    
    // スキニング後の座標のwを戻し、法線を正規化
    skinnedPosition.w = 1.0f;
    skinnedNormal = normalize(skinnedNormal);

    // ワールド座標、WVP空間への変換、および法線の変換
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    output.normal = mul(skinnedNormal, (float3x3) gTransformationMatrix.WorldInverseTranspose);
    output.texcoord = input.texcoord;

    return output;
}
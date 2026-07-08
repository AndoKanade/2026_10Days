#include "Object3d.hlsli"

// 頂点シェーダー入力構造体
struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 weight : WEIGHT0; // CS側で処理済みのためVSでは未使用
    int32_t4 index : INDEX0; // CS側で処理済みのためVSでは未使用
};

// 変換行列用定数バッファ
struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

// 定数バッファの登録
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);

// 頂点シェーダーメイン処理
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // スキニングはCompute Shaderで完了しているため、そのまま座標変換を行う
    output.worldPosition = mul(input.position, gTransformationMatrix.World).xyz;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));
    output.texcoord = input.texcoord;

    return output;
}
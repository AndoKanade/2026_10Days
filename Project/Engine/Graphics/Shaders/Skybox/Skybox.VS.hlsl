#include "Skybox.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // WVP行列で座標変換
    output.position = mul(input.position, gTransformationMatrix.WVP).xyww;
    // モデルのローカル座標をサンプリング方向として使用
    output.texcoord = input.position.xyz;
    return output;
}
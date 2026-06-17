#include "Object3d.hlsli"

// 座標変換行列
struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
};

// 入力データ (WEIGHTとINDEXを追加)
struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 weight : WEIGHT0;
    int32_t4 index : INDEX0;
};

// 定数バッファと構造化バッファ
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);
StructuredBuffer<float32_t4x4> gBonePalette : register(t2);

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // スキニング計算用の変数を初期化
    float32_t4 skinnedPosition = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    float32_t3 skinnedNormal = float32_t3(0.0f, 0.0f, 0.0f);

    // 4つのボーンの影響を計算
    for (int32_t i = 0; i < 4; ++i)
    {
        if (input.weight[i] <= 0.0f)
        {
            continue;
        }

        // ボーン行列を取得
        float32_t4x4 boneMatrix = gBonePalette[input.index[i]];

        // 頂点と法線にボーン行列とウェイトを適用
        skinnedPosition += mul(input.position, boneMatrix) * input.weight[i];
        skinnedNormal += mul(input.normal, (float32_t3x3) boneMatrix) * input.weight[i];
    }

    skinnedPosition.w = 1.0f;
    skinnedNormal = normalize(skinnedNormal);

    // 変換された座標と法線を使用して通常の計算を行う
    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinnedNormal, (float32_t3x3) gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    
    return output;
}
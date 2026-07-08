#include "PostProcess.hlsli"

// --- リソース ---
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// --- メイン処理 ---
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

	// 1. 画面の分割数を 30 に調整（細かすぎず粗すぎない絶妙な帯の太さ）
    float32_t blockY = floor(input.texcoord.y * 30.0);

	// 2. ブロックごとの乱数を生成
    float32_t randomVal = frac(sin(blockY * 12.9898 + randomTime * 30.0) * 43758.5453);

	// 3. 引き裂き（シフト値）の計算
	// 確率を少し絞り、ズレ幅も適度な激しさに調整
    float32_t shift = 0.0;
    if (randomVal > 0.6)
    {
        shift = (randomVal - 0.8) * randomIntensity * 2.5;
    }

	// 4. 色収差（RGB分離）をマイルドに適用
	// ズレたブロックの輪郭が綺麗に赤・青ににじむように、サンプリングを少しだけずらします
    float32_t2 uvR = input.texcoord + float32_t2(shift * 1.2, 0.0);
    float32_t2 uvG = input.texcoord + float32_t2(shift, 0.0);
    float32_t2 uvB = input.texcoord + float32_t2(shift * 0.8, 0.0);

    float32_t r = gTexture.Sample(gSampler, uvR).r;
    float32_t g = gTexture.Sample(gSampler, uvG).g;
    float32_t b = gTexture.Sample(gSampler, uvB).b;
    float32_t a = gTexture.Sample(gSampler, uvG).a;

    output.color = float32_t4(r, g, b, a);
    return output;
}
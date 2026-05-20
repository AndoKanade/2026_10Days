#include "PostProcess.hlsli"

// --- 定数カーネル (Prewitt) ---
static const float32_t kPrewittHorizontalKernel[3][3] =
{
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
};

static const float32_t kPrewittVerticalKernel[3][3] =
{
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f },
};

// --- リソース ---
Texture2D<float32_t4> gTexture : register(t0); // 元画像
Texture2D<float32_t> gDepthTexture : register(t1); // 深度テクスチャ

SamplerState gSampler : register(s0); // Linearサンプラー
SamplerState gSamplerPoint : register(s1); // Pointサンプラー (追加)


// --- 輝度変換関数 ---
float32_t Luminance(float32_t3 v)
{
    return dot(v, float32_t3(0.2125f, 0.7154f, 0.0721f));
}

// --- メイン関数 ---
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    uint32_t width, height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = rcp(float32_t2(width, height));

    // 差分累計用 (x: 横方向, y: 縦方向)
    float32_t2 diffLuminance = float32_t2(0.0f, 0.0f);
    float32_t2 diffDepth = float32_t2(0.0f, 0.0f);

    // 3x3の畳み込み
    for (int32_t x = 0; x < 3; ++x)
    {
        for (int32_t y = 0; y < 3; ++y)
        {
            float32_t2 offset = float32_t2(x - 1, y - 1);
            float32_t2 texcoord = input.texcoord + offset * uvStepSize;

            // 1. 輝度の取得 (通常のサンプラー)
            float32_t luminance = Luminance(gTexture.Sample(gSampler, texcoord).rgb);
            
            // 2. 深度の取得 (補間しないポイントサンプラーを使用)
            float32_t depth = gDepthTexture.Sample(gSamplerPoint, texcoord);

            // 輝度の差分加算
            diffLuminance.x += luminance * kPrewittHorizontalKernel[x][y];
            diffLuminance.y += luminance * kPrewittVerticalKernel[x][y];

            // 深度の差分加算
            diffDepth.x += depth * kPrewittHorizontalKernel[x][y];
            diffDepth.y += depth * kPrewittVerticalKernel[x][y];
        }
    }

    // それぞれのエッジ強度を計算
    float32_t weightLuminance = length(diffLuminance);
    float32_t weightDepth = length(diffDepth);

    // 強調して合成 (maxでどちらか強い方を採用)
    float32_t weight = max(weightLuminance * 6.0f, weightDepth * 10.0f);
    weight = saturate(weight);

    // 元画像と合成 (1.0 - weight でエッジを黒くする)
    float32_t3 texColor = gTexture.Sample(gSampler, input.texcoord).rgb;
    output.color.rgb = (1.0f - weight) * texColor;
    output.color.a = 1.0f;

    return output;
}
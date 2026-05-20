#include "PostProcess.hlsli"

// --- 定数カーネル ---
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

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// --- 輝度変換関数 ---
float32_t Luminance(float32_t3 v)
{
    return dot(v, float32_t3(0.2125f, 0.7154f, 0.0721f));
}

// --- メイン関数 ---
PixelShaderOutput main(VertexShaderOutput input)
{
    uint32_t width, height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = rcp(float32_t2(width, height));

    // 1. 縦横の畳み込み結果を計算
    float32_t2 difference = float32_t2(0.0f, 0.0f);

    for (int32_t x = 0; x < 3; ++x)
    {
        for (int32_t y = 0; y < 3; ++y)
        {
            float32_t2 offset = float32_t2(x - 1, y - 1);
            float32_t2 texcoord = input.texcoord + offset * uvStepSize;

            float32_t3 fetchColor = gTexture.Sample(gSampler, texcoord).rgb;
            float32_t luminance = Luminance(fetchColor);

            difference.x += luminance * kPrewittHorizontalKernel[x][y];
            difference.y += luminance * kPrewittVerticalKernel[x][y];
        }
    }

    // 2. エッジ強度の計算と調整
    float32_t weight = length(difference);
    // スライドに基づき、差分を強調（6倍）して0~1に収める
    weight = saturate(weight * 6.0f);

    // 3. 元画像との合成
    PixelShaderOutput output;
    float32_t3 texColor = gTexture.Sample(gSampler, input.texcoord).rgb;
    
    // weightが大きい（エッジが強い）ほど、色が黒(0)に近づくように合成
    output.color.rgb = (1.0f - weight) * texColor;
    output.color.a = 1.0f;

    return output;
}
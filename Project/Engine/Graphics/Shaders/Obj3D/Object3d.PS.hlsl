#include"Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float32_t shininess;
    
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
    
};

struct Camera
{
    float32_t3 worldPosition;
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
ConstantBuffer<Camera> gCamera : register(b3);


struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 1. 共通計算 (UV変換とテクスチャサンプリング)
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // 2. ライティング計算
    if (gMaterial.enableLighting != 0)
    {
        // 法線の正規化
        float32_t3 normal = normalize(input.normal);

        // --- A. 拡散反射 (Diffuse) ---
        // 既存のハーフランバート計算
        float NdotL = dot(normal, -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        float32_t3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;

        // --- B. 鏡面反射 (Specular) ---
        // 視線ベクトル (カメラ位置 - ピクセル位置)
        float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        // 反射ベクトル (ライトの方向を法線で反射させる)
        // ※ reflect関数の第1引数は「入射ベクトル(光源の進行方向)」なので gDirectionalLight.direction を使います
        float32_t3 reflectLight = reflect(gDirectionalLight.direction, normal);

        // 鏡面反射の強さを計算 (R dot E)
        float RdotE = dot(reflectLight, toEye);
        float specularPow = pow(saturate(RdotE), gMaterial.shininess);

        // スペキュラ色の計算 (白く光らせるため float32_t3(1.0f, 1.0f, 1.0f) を掛けています)
        float32_t3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);

        // --- C. 合成 (Diffuse + Specular) ---
        output.color.rgb = diffuse + specular;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        // ライティング無効時
        output.color = gMaterial.color * textureColor;
    }

    // 3. アルファテスト (不要なピクセルを破棄)
    if (textureColor.a <= 0.5f)
    {
        discard;
    }
    if (textureColor.a == 0.0f)
    {
        discard;
    }
    if (output.color.a == 0.0f)
    {
        discard;
    }

    return output;
}
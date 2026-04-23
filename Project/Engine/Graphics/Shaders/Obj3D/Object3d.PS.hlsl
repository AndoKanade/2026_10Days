#include "Object3d.hlsli"

// --- 構造体定義 ---
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
    float32_t intensity;
};

struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t radius;
    float32_t decay;
    float32_t padding[2];
};

struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t3 direction;
    float32_t distance;
    float32_t cosAngle;
    float32_t decay;
    float32_t cosFalloffStart;
    float32_t padding[2];
};

struct Camera
{
    float32_t3 worldPosition;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// --- 定数バッファ・テクスチャ ---
ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
ConstantBuffer<Camera> gCamera : register(b3);
ConstantBuffer<PointLight> gPointLight : register(b4);
ConstantBuffer<SpotLight> gSpotLight : register(b5);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// --- メイン関数 ---
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV Transform適用
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // 最終的な出力カラーの初期化
    float32_t3 finalColor = float32_t3(0.0f, 0.0f, 0.0f);

    // ライティング計算
    if (gMaterial.enableLighting != 0)
    {
        float32_t3 normal = normalize(input.normal);
        float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        // --- 1. Directional Light (平行光源) ---
        float32_t3 dirFactor = normalize(-gDirectionalLight.direction);
        float32_t dirNdotL = dot(normal, dirFactor);
        float32_t dirCos = pow(dirNdotL * 0.5f + 0.5f, 2.0f); // Half-Lambert
        
        // Diffuse & Specular
        float32_t3 dirDiffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * dirCos * gDirectionalLight.intensity;
        float32_t3 dirHalfVector = normalize(dirFactor + toEye);
        float32_t dirNDotH = dot(normal, dirHalfVector);
        float32_t dirSpecularPow = pow(saturate(dirNDotH), gMaterial.shininess);
        float32_t3 dirSpecular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * dirSpecularPow;

        // --- 2. Point Light (点光源) ---
        float32_t3 pointVec = input.worldPosition - gPointLight.position;
        float32_t pDistance = length(pointVec);
        float32_t3 pFactor = normalize(-pointVec);
        float32_t pAttenuation = pow(saturate(1.0f - (pDistance / gPointLight.radius)), gPointLight.decay);
        
        float32_t pNdotL = dot(normal, pFactor);
        float32_t pCos = pow(pNdotL * 0.5f + 0.5f, 2.0f);
        
        // Diffuse & Specular
        float32_t3 pDiffuse = gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * pCos * gPointLight.intensity * pAttenuation;
        float32_t3 pHalfVector = normalize(pFactor + toEye);
        float32_t pNDotH = dot(normal, pHalfVector);
        float32_t pSpecularPow = pow(saturate(pNDotH), gMaterial.shininess);
        float32_t3 pSpecular = gPointLight.color.rgb * gPointLight.intensity * pSpecularPow * pAttenuation;

        // --- 3. Spot Light (スポットライト) ---
        float32_t3 spotVec = input.worldPosition - gSpotLight.position;
        float32_t sDistance = length(spotVec);
        float32_t3 spotDirP = normalize(spotVec);
        
        // 角度減衰 & 距離減衰
        float32_t sCosAngle = dot(spotDirP, normalize(gSpotLight.direction));
        float32_t sFalloff = saturate((sCosAngle - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));
        float32_t sAttenuation = pow(saturate(1.0f - (sDistance / gSpotLight.distance)), gSpotLight.decay);
        
        float32_t3 sFactor = -spotDirP;
        float32_t sNdotL = dot(normal, sFactor);
        float32_t sCos = pow(sNdotL * 0.5f + 0.5f, 2.0f);
        
        // Diffuse & Specular
        float32_t3 sDiffuse = gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb * sCos * gSpotLight.intensity * sAttenuation * sFalloff;
        float32_t3 sHalfVector = normalize(sFactor + toEye);
        float32_t sNDotH = dot(normal, sHalfVector);
        float32_t sSpecularPow = pow(saturate(sNDotH), gMaterial.shininess);
        float32_t3 sSpecular = gSpotLight.color.rgb * gSpotLight.intensity * sSpecularPow * sAttenuation * sFalloff;

        // 合計
        finalColor = dirDiffuse + dirSpecular + pDiffuse + pSpecular + sDiffuse + sSpecular;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        // ライティング無効時はマテリアル色×テクスチャ色のみ
        finalColor = gMaterial.color.rgb * textureColor.rgb;
        output.color.a = gMaterial.color.a * textureColor.a;
    }

    // アルファテスト
    if (textureColor.a <= 0.5f || output.color.a == 0.0f)
    {
        discard;
    }

    output.color.rgb = finalColor;
    return output;
}
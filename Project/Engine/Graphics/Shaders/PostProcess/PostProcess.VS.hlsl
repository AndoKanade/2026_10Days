#include "PostProcess.hlsli"

// 頂点数
static const uint32_t kNumVertex = 3;

// 全画面を覆う三角形の頂点座標（NDC空間）
static const float32_t4 kPositions[kNumVertex] =
{
    { -1.0f, 1.0f, 0.0f, 1.0f }, // 左上
    { 3.0f, 1.0f, 0.0f, 1.0f }, // 右上（画面外まで飛ばす）
    { -1.0f, -3.0f, 0.0f, 1.0f } // 左下（画面外まで飛ばす）
};

// 対応するUV座標
static const float32_t2 kTexcoords[kNumVertex] =
{
    { 0.0f, 0.0f }, // 左上
    { 2.0f, 0.0f }, // 右上
    { 0.0f, 2.0f } // 左下
};

// 頂点番号（SV_VertexID）を直接受け取る
VertexShaderOutput main(uint32_t vertexId : SV_VertexID)
{
    VertexShaderOutput output;
    output.position = kPositions[vertexId];
    output.texcoord = kTexcoords[vertexId];
    return output;
}
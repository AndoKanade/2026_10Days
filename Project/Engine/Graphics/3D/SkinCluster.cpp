#include "SkinCluster.h"
#include "Logger.h"
#include "Model.h"

void SkinCluster::Initialize(DXCommon* dxCommon,const Model::ModelData& modelData,const Skeleton& skeleton){
    OutputDebugStringA("--- Checking Skinning Data ---\n");

    paletteSize_ = static_cast<uint32_t>(skeleton.joints.size());
    inverseBindPoseMatrices_.resize(paletteSize_);

    // 1. インバースバインドポーズのコピー
    for(const auto& [jointName,weightData] : modelData.skinClusterData){
        auto it = skeleton.jointMap.find(jointName);
        if(it != skeleton.jointMap.end()){
            uint32_t index = it->second;
            inverseBindPoseMatrices_[index] = weightData.inverseBindPoseMatrix;
        }
    }

    // 2. MatrixPalette 構造体サイズでバッファを生成（256バイトアライメント）
    size_t paletteSizeInBytes = sizeof(MatrixPalette) * paletteSize_;
    size_t alignedSize = (paletteSizeInBytes + 255) & ~255;
    paletteResource = dxCommon->CreateBufferResource(alignedSize);

    // 頂点ごとのウェイト・インデックス影響情報を初期化
    std::vector<VertexInfluence> vertexInfluences(modelData.vertices.size(),{{0,0,0,0}, {0,0,0,0}});

    // 3. ボーンウェイトの適用
    for(const auto& [jointName,jointWeightData] : modelData.skinClusterData){
        auto it = skeleton.jointMap.find(jointName);
        if(it == skeleton.jointMap.end()) continue;
        uint32_t jointIndex = it->second;

        for(const auto& vertexWeight : jointWeightData.vertexWeights){
            uint32_t vertexIndex = vertexWeight.vertexIndex;
            VertexInfluence& influence = vertexInfluences[vertexIndex];

            // 影響度の低い枠を探して置き換える（4つまで）
            int bestIndex = -1;
            float minWeight = vertexWeight.weight;

            for(int j = 0; j < 4; ++j){
                if(influence.weight[j] < minWeight){
                    minWeight = influence.weight[j];
                    bestIndex = j;
                }
            }

            if(bestIndex != -1){
                influence.weight[bestIndex] = vertexWeight.weight;
                influence.index[bestIndex] = static_cast<int32_t>(jointIndex);
            }
        }
    }

    // 4. ウェイトの正規化
    for(auto& influence : vertexInfluences){
        float totalWeight = 0.0f;
        for(int j = 0; j < 4; ++j) totalWeight += influence.weight[j];
        if(totalWeight > 0.0f){
            for(int j = 0; j < 4; ++j) influence.weight[j] /= totalWeight;
        }
    }

    // 5. 頂点影響情報バッファの作成
    size_t influenceSizeInBytes = sizeof(VertexInfluence) * vertexInfluences.size();
    influenceResource = dxCommon->CreateBufferResource(influenceSizeInBytes);

    VertexInfluence* ptr = nullptr;
    influenceResource->Map(0,nullptr,reinterpret_cast<void**>(&ptr));
    std::memcpy(ptr,vertexInfluences.data(),influenceSizeInBytes);
    influenceResource->Unmap(0,nullptr);

    // 頂点バッファビューの生成
    influenceBufferView.BufferLocation = influenceResource->GetGPUVirtualAddress();
    influenceBufferView.SizeInBytes = UINT(influenceSizeInBytes);
    influenceBufferView.StrideInBytes = sizeof(VertexInfluence);
}

void SkinCluster::Update(const Skeleton& skeleton){
    if(!paletteResource) return;

    // パレットリソースの書き込み
    MatrixPalette* ptr = nullptr;
    paletteResource->Map(0,nullptr,reinterpret_cast<void**>(&ptr));

    for(uint32_t i = 0; i < paletteSize_; ++i){
        // 現在の骨の姿勢行列と初期姿勢の逆行列を乗算
        Matrix4x4 mat = Multiply(skeleton.joints[i].skeletonSpaceMatrix,inverseBindPoseMatrices_[i]);

        // HLSL向けに行列を転置して渡す
        ptr[i].skeletonSpaceMatrix = Transpose(mat);

        // 法線用の逆転置行列を計算して転置
        Matrix4x4 invMat = Inverse(mat);
        ptr[i].skeletonSpaceInverseTransposeMatrix = Transpose(invMat);
    }
    paletteResource->Unmap(0,nullptr);
}
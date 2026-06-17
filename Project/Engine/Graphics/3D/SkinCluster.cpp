#include "SkinCluster.h"

void SkinCluster::Initialize(DXCommon* dxCommon,const Model::ModelData& modelData,const Skeleton& skeleton){
    OutputDebugStringA("--- Checking Skinning Data ---\n");
    for(const auto& [name,data] : modelData.skinClusterData){
        char buf[128];
        sprintf_s(buf,"Joint: %s, WeightCount: %zd\n",name.c_str(),data.vertexWeights.size());
        OutputDebugStringA(buf);
    }

    paletteSize_ = static_cast<uint32_t>(skeleton.joints.size());
    inverseBindPoseMatrices_.resize(paletteSize_);
    for(uint32_t i = 0; i < paletteSize_; ++i){
        inverseBindPoseMatrices_[i] = skeleton.joints[i].inverseBindPoseMatrix;
    }

    size_t paletteSizeInBytes = sizeof(Matrix4x4) * paletteSize_;
    paletteResource = dxCommon->CreateBufferResource(paletteSizeInBytes);

    std::vector<VertexInfluence> vertexInfluences(modelData.vertices.size(),{});

    for(const auto& [jointName,jointWeightData] : modelData.skinClusterData){
        auto it = skeleton.jointMap.find(jointName);
        if(it == skeleton.jointMap.end()) continue;
        uint32_t jointIndex = it->second;

        for(const auto& vertexWeight : jointWeightData.vertexWeights){
            VertexInfluence& influence = vertexInfluences[vertexWeight.vertexIndex];
            // ★ここをチェック: j=0から3まで、weightが0.0なら代入するというロジックは合っています
            for(int j = 0; j < 4; ++j){
                if(influence.weight[j] == 0.0f){
                    influence.weight[j] = vertexWeight.weight;
                    influence.index[j] = jointIndex;
                    break;
                }
            }
        }
    }

    // ...以降の頂点影響バッファ作成処理はそのまま...
    size_t influenceSizeInBytes = sizeof(VertexInfluence) * vertexInfluences.size();
    influenceResource = dxCommon->CreateBufferResource(influenceSizeInBytes);
    VertexInfluence* ptr = nullptr;
    influenceResource->Map(0,nullptr,reinterpret_cast<void**>(&ptr));
    std::memcpy(ptr,vertexInfluences.data(),influenceSizeInBytes);
    influenceResource->Unmap(0,nullptr);
    influenceBufferView.BufferLocation = influenceResource->GetGPUVirtualAddress();
    influenceBufferView.SizeInBytes = UINT(influenceSizeInBytes);
    influenceBufferView.StrideInBytes = sizeof(VertexInfluence);
}

void SkinCluster::Update(const Skeleton& skeleton){
    if(!paletteResource) return; // リソースがないなら更新しない

    Matrix4x4* ptr = nullptr;
    // Map の結果をチェック
    if(FAILED(paletteResource->Map(0,nullptr,reinterpret_cast<void**>(&ptr)))){
        return;
    }

    uint32_t loopCount = (paletteSize_ < skeleton.joints.size())?paletteSize_:(uint32_t)skeleton.joints.size();

    for(uint32_t i = 0; i < loopCount; ++i){
        ptr[i] = Multiply(inverseBindPoseMatrices_[i],skeleton.joints[i].skeletonSpaceMatrix);
    }

    paletteResource->Unmap(0,nullptr);
}
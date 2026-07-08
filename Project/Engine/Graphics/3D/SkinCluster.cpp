#include "SkinCluster.h"
#include "SrvManager.h"
#include "Logger.h"
#include "Model.h"

void SkinCluster::Initialize(DXCommon* dxCommon,const Model::ModelData& modelData,const Skeleton& skeleton){
    OutputDebugStringA("--- Checking Skinning Data ---\n");

    paletteSize_ = static_cast<uint32_t>(skeleton.joints.size());
    inverseBindPoseMatrices_.resize(paletteSize_,MakeIdentity4x4());

    // インバースバインドポーズのコピー
    for(const auto& [jointName,weightData] : modelData.skinClusterData){
        auto it = skeleton.jointMap.find(jointName);
        if(it != skeleton.jointMap.end()){
            inverseBindPoseMatrices_[it->second] = weightData.inverseBindPoseMatrix;
        }
    }

    // 1. 各リソースの生成
    size_t paletteSizeInBytes = (sizeof(MatrixPalette) * paletteSize_ + 255) & ~255;
    paletteResource = dxCommon->CreateBufferResource(paletteSizeInBytes);

    // 頂点ウェイト情報の構築
    std::vector<VertexInfluence> vertexInfluences(modelData.vertices.size(),{{0, 0, 0, 0}, {0, 0, 0, 0}});
    for(const auto& [jointName,jointWeightData] : modelData.skinClusterData){
        auto it = skeleton.jointMap.find(jointName);
        if(it == skeleton.jointMap.end()) continue;
        for(const auto& vw : jointWeightData.vertexWeights){
            VertexInfluence& inf = vertexInfluences[vw.vertexIndex];
            for(int j = 0; j < 4; ++j){
                if(vw.weight > inf.weight[j]){
                    for(int k = 3; k > j; --k){ inf.weight[k] = inf.weight[k - 1]; inf.index[k] = inf.index[k - 1]; }
                    inf.weight[j] = vw.weight;
                    inf.index[j] = static_cast<int32_t>(it->second);
                    break;
                }
            }
        }
    }

    influenceResource = dxCommon->CreateBufferResource(sizeof(VertexInfluence) * vertexInfluences.size());
    VertexInfluence* infPtr = nullptr;
    influenceResource->Map(0,nullptr,reinterpret_cast<void**>(&infPtr));
    std::memcpy(infPtr,vertexInfluences.data(),sizeof(VertexInfluence) * vertexInfluences.size());
    influenceResource->Unmap(0,nullptr);

    // 入力頂点バッファ (t1用)
    inputVerticesResource = dxCommon->CreateBufferResource(sizeof(Model::VertexData) * modelData.vertices.size());
    Model::VertexData* vPtr = nullptr;
    inputVerticesResource->Map(0,nullptr,reinterpret_cast<void**>(&vPtr));
    std::memcpy(vPtr,modelData.vertices.data(),sizeof(Model::VertexData) * modelData.vertices.size());
    inputVerticesResource->Unmap(0,nullptr);

    // スキニング結果出力用UAVバッファ
    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeof(Model::VertexData) * modelData.vertices.size();
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.SampleDesc.Quality = 0;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    D3D12_HEAP_PROPERTIES heapProps = {.Type = D3D12_HEAP_TYPE_DEFAULT};
    dxCommon->GetDevice()->CreateCommittedResource(&heapProps,D3D12_HEAP_FLAG_NONE,&resDesc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&skinnedVertexBuffer));



    // SRV/UAVの確保 (連続したインデックスで作成)
    SrvManager* srvManager = SrvManager::GetInstance();
    uint32_t srvIndex = srvManager->Allocate();
    srvManager->Allocate(); // 2つ目
    srvManager->Allocate();
    srvManager->CreateSRVforStructuredBuffer(srvIndex,paletteResource.Get(),paletteSize_,sizeof(MatrixPalette));
    uint32_t structStride = sizeof(Model::VertexData);
    srvManager->CreateSRVforStructuredBuffer(srvIndex + 1,inputVerticesResource.Get(),(UINT)modelData.vertices.size(),structStride);
    srvManager->CreateSRVforStructuredBuffer(srvIndex + 2,influenceResource.Get(),(UINT)vertexInfluences.size(),sizeof(VertexInfluence));
    this->paletteSrvHandle = srvManager->GetGPUDescriptorHandle(srvIndex);

    uint32_t uavIndex = srvManager->Allocate();
    srvManager->CreateUAVForStructuredBuffer(uavIndex,skinnedVertexBuffer.Get(),(UINT)modelData.vertices.size(),sizeof(Model::VertexData));
    this->outputUavHandle = srvManager->GetGPUDescriptorHandle(uavIndex);

    skinningInfoResource = dxCommon->CreateBufferResource(sizeof(uint32_t));
    uint32_t* infoPtr = nullptr;
    skinningInfoResource->Map(0,nullptr,reinterpret_cast<void**>(&infoPtr));
    *infoPtr = static_cast<uint32_t>(modelData.vertices.size());
    skinningInfoResource->Unmap(0,nullptr);

    numVertices_ = static_cast<uint32_t>(modelData.vertices.size());
}

void SkinCluster::Update(const Skeleton& skeleton,ID3D12GraphicsCommandList* commandList,Obj3dCommon* obj3dCommon,D3D12_GPU_VIRTUAL_ADDRESS skinningInfoAddress){
    if(!paletteResource) return;

    // パレット更新
    MatrixPalette* ptr = nullptr;
    paletteResource->Map(0,nullptr,reinterpret_cast<void**>(&ptr));
    for(uint32_t i = 0; i < paletteSize_; ++i){
        Matrix4x4 mat = Multiply(inverseBindPoseMatrices_[i],skeleton.joints[i].skeletonSpaceMatrix);
        ptr[i].skeletonSpaceMatrix = mat;
        Matrix4x4 rot = mat;
        rot.m[3][0] = 0.0f; rot.m[3][1] = 0.0f; rot.m[3][2] = 0.0f;
        ptr[i].skeletonSpaceInverseTransposeMatrix = Transpose(Inverse(rot));
    }
    paletteResource->Unmap(0,nullptr);

    // デスクリプタヒープを必ずセット
    ID3D12DescriptorHeap* ppHeaps[] = {SrvManager::GetInstance()->GetDescriptorHeap()};
    commandList->SetDescriptorHeaps(_countof(ppHeaps),ppHeaps);

    commandList->SetComputeRootSignature(obj3dCommon->GetComputeRootSignature());
    commandList->SetPipelineState(obj3dCommon->GetComputePipelineState());

    // バインド (register t0, t1, t2 に順次対応)
    commandList->SetComputeRootConstantBufferView(0,skinningInfoAddress);
    commandList->SetComputeRootDescriptorTable(1,paletteSrvHandle);
    commandList->SetComputeRootDescriptorTable(2,outputUavHandle);

    uint32_t groupCountX = (numVertices_ + 1023) / 1024;
    commandList->Dispatch(groupCountX,1,1);
    // バリア
    
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = skinnedVertexBuffer.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1,&barrier);
}
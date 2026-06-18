#pragma once

#include "Model.h"
#include "Skeleton.h"
#include "DXCommon.h" // リソース作成のために必要
#include <d3d12.h>
#include <wrl.h>
#include <vector>

class SkinCluster{
public:
    struct VertexInfluence{
        float weight[4];
        int32_t index[4];
    };

    struct MatrixPalette{
        Matrix4x4 skeletonSpaceMatrix;
        Matrix4x4 skeletonSpaceInverseTransposeMatrix;
    };

    // 初期化：ModelData と Skeleton から必要なリソースを作る
    void Initialize(DXCommon* dxCommon,const Model::ModelData& modelData,const Skeleton& skeleton);

    // 更新：現在の Skeleton の姿勢をパレットに書き込む
    void Update(const Skeleton& skeleton);

    // ゲッター：描画時に使うもの
    D3D12_GPU_VIRTUAL_ADDRESS GetPaletteAddress() const{ return paletteResource->GetGPUVirtualAddress(); }
    ID3D12Resource* GetPaletteResource() const{ return paletteResource.Get(); }
    const D3D12_VERTEX_BUFFER_VIEW& GetInfluenceBufferView() const{ return influenceBufferView; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;
    D3D12_VERTEX_BUFFER_VIEW influenceBufferView{};

    // パレット行列の数（ボーン数）を保存しておく
    uint32_t paletteSize_ = 0;
    std::vector<Matrix4x4> inverseBindPoseMatrices_;
};
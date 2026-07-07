#pragma once

#include "Model.h"
#include "Skeleton.h"
#include "DXCommon.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <array>

// スキニングクラス
class SkinCluster{
public:
	// 定数定義
	static constexpr uint32_t kNumMaxInfluence = 4;

	// 構造体定義
	struct VertexInfluence{
		std::array<float,kNumMaxInfluence> weight;
		std::array<int32_t,kNumMaxInfluence> index;
	};

	struct MatrixPalette{
		Matrix4x4 skeletonSpaceMatrix;
		Matrix4x4 skeletonSpaceInverseTransposeMatrix;
	};

	// メンバ関数
	void Initialize(DXCommon* dxCommon,const Model::ModelData& modelData,const Skeleton& skeleton);
	void Update(const Skeleton& skeleton);

	// ゲッター
	D3D12_GPU_VIRTUAL_ADDRESS GetPaletteAddress() const{ return paletteResource->GetGPUVirtualAddress(); }
	ID3D12Resource* GetPaletteResource() const{ return paletteResource.Get(); }
	const D3D12_VERTEX_BUFFER_VIEW& GetInfluenceBufferView() const{ return influenceBufferView; }

private:
	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;
	D3D12_VERTEX_BUFFER_VIEW influenceBufferView{};

	// パレットデータ
	uint32_t paletteSize_ = 0;
	std::vector<Matrix4x4> inverseBindPoseMatrices_;
};
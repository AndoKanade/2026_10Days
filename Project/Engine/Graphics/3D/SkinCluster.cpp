#include "SkinCluster.h"
#include "Logger.h"
#include "Model.h"

void SkinCluster::Initialize(DXCommon* dxCommon,const Model::ModelData& modelData,const Skeleton& skeleton){
	OutputDebugStringA("--- Checking Skinning Data ---\n");

	paletteSize_ = static_cast<uint32_t>(skeleton.joints.size());
	inverseBindPoseMatrices_.resize(paletteSize_);

	for(uint32_t i = 0; i < paletteSize_; ++i){
		inverseBindPoseMatrices_[i] = MakeIdentity4x4();
	}

	// インバースバインドポーズのコピー
	for(const auto& [jointName,weightData] : modelData.skinClusterData){
		auto it = skeleton.jointMap.find(jointName);
		if(it != skeleton.jointMap.end()){
			uint32_t index = it->second;
			inverseBindPoseMatrices_[index] = weightData.inverseBindPoseMatrix;
		}
	}

	// MatrixPalette構造体サイズでバッファを生成（256バイトアライメント）
	size_t paletteSizeInBytes = sizeof(MatrixPalette) * paletteSize_;
	size_t alignedSize = (paletteSizeInBytes + 255) & ~255;
	paletteResource = dxCommon->CreateBufferResource(alignedSize);

	// 頂点ごとのウェイト・インデックス影響情報を初期化
	std::vector<VertexInfluence> vertexInfluences(modelData.vertices.size(),{{0, 0, 0, 0}, {0, 0, 0, 0}});

	// ボーンウェイトの適用
	for(const auto& [jointName,jointWeightData] : modelData.skinClusterData){
		auto it = skeleton.jointMap.find(jointName);
		if(it == skeleton.jointMap.end()) continue;
		uint32_t jointIndex = it->second;

		for(const auto& vertexWeight : jointWeightData.vertexWeights){
			uint32_t vertexIndex = vertexWeight.vertexIndex;
			VertexInfluence& influence = vertexInfluences[vertexIndex];

			// 新しいウェイトを重い順に挿入する
			for(int j = 0; j < 4; ++j){
				if(vertexWeight.weight > influence.weight[j]){
					// 今の値を一つ後ろへずらす
					for(int k = 3; k > j; --k){
						influence.weight[k] = influence.weight[k - 1];
						influence.index[k] = influence.index[k - 1];
					}
					// 新しい値を挿入
					influence.weight[j] = vertexWeight.weight;
					influence.index[j] = static_cast<int32_t>(jointIndex);
					break;
				}
			}
		}
	}

	// ウェイトの正規化
	for(auto& influence : vertexInfluences){
		float totalWeight = 0.0f;
		for(int j = 0; j < 4; ++j){
			totalWeight += influence.weight[j];
		}

		if(totalWeight > 0.0f){
			for(int j = 0; j < 4; ++j){
				influence.weight[j] /= totalWeight;
			}
		} else{
			influence.weight[0] = 1.0f;
			influence.weight[1] = 0.0f;
			influence.weight[2] = 0.0f;
			influence.weight[3] = 0.0f;
			influence.index[0] = 0;
			influence.index[1] = 0;
			influence.index[2] = 0;
			influence.index[3] = 0;
		}
	}

	// 頂点影響情報バッファの作成
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

	MatrixPalette* ptr = nullptr;
	paletteResource->Map(0,nullptr,reinterpret_cast<void**>(&ptr));

	for(uint32_t i = 0; i < paletteSize_; ++i){
		// スキニング行列（頂点移動用）
		Matrix4x4 mat = Multiply(inverseBindPoseMatrices_[i],skeleton.joints[i].skeletonSpaceMatrix);
		ptr[i].skeletonSpaceMatrix = mat;

		// 法線用行列（回転のみを抽出して逆転置）
		Matrix4x4 rotateMat = mat;
		rotateMat.m[3][0] = 0.0f;
		rotateMat.m[3][1] = 0.0f;
		rotateMat.m[3][2] = 0.0f;

		ptr[i].skeletonSpaceInverseTransposeMatrix = Transpose(Inverse(rotateMat));
	}
	paletteResource->Unmap(0,nullptr);
}
#pragma once

#include "ModelCommon.h"
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl.h>
#include "MyMath.h"
#include "Skeleton.h"
#include <assimp/scene.h>

struct aiNode;

class Model{
public:
	// 頂点データ
	struct VertexData{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// マテリアル読み込みデータ
	struct MaterialData{
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	// モデルデータ全体
	struct ModelData{
		std::vector<VertexData> vertices;
		MaterialData material;
		Node rootNode;
	};

	// マテリアル定数バッファ
	struct Material{
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
		float shininess;
		float environmentCoefficient;
	};

public:
	// 初期化
	void Initialize(ModelCommon* modelCommon,const std::string& directorypath,const std::string& filename);

	// 描画
	void Draw(uint32_t skyboxTextureIndex,D3D12_GPU_VIRTUAL_ADDRESS cameraAddress);

	// テクスチャのセット
	void SetTexture(const std::string& texturefilePath);

	// RootNodeの取得
	const Node& GetRootNode() const{ return modelData.rootNode; }

	// .mtlファイルの読み込み
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath,const std::string& filename);

	// .objファイルの読み込み
	static ModelData LoadModelFile(const std::string& directoryPath,const std::string& filename);

private:
	// 頂点バッファの作成
	void CreateVertexData();

	// マテリアルバッファの作成
	void CreateMaterialData();

	// Assimpのノードを読み込む
	static Node ReadNode(aiNode* node);

private:
	// 共通リソースへのポインタ
	ModelCommon* modelCommon_ = nullptr;

	// CPU側のモデルデータ
	ModelData modelData;

	// 頂点バッファ関連リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	VertexData* vertexData = nullptr;

	// マテリアルバッファ関連リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Material* materialData = nullptr;
};
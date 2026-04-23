#pragma once
#include "ModelCommon.h"

// 必要な標準ライブラリとDirectX関連のinclude
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl.h>
#include"MyMath.h"


class Model{
public: // --- 構造体定義 (データ構造) ---

	// 頂点データ (GPUに送る1頂点ごとの情報)
	struct VertexData{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// マテリアル読み込みデータ (.mtlファイルから読み取った情報の保持用)
	struct MaterialData{
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	// モデルデータ全体 (頂点リスト + マテリアル情報)
	struct ModelData{
		std::vector<VertexData> vertices;
		MaterialData material;
	};

	// マテリアル定数バッファ (GPUのConstantBufferに送るデータ)
	// ※構造体のアライメント(16バイト境界)に注意
	struct Material{
		Vector4 color;
		int32_t enableLighting; // 4バイト
		float padding[3];       // 4バイト * 3 = 12バイト (合計16バイトにするためのパディング)
		Matrix4x4 uvTransform;
		float shininess;
	};

public: // --- メンバ関数 ---

	// 初期化
	void Initialize(ModelCommon* modelCommon,const std::string& directorypath,const std::string& filename);

	// 描画
	void Draw();

	// --- 静的関数 (ファイル読み込みなど) ---

	// .mtlファイルの読み込み
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath,const std::string& filename);

	// .objファイルの読み込み
	static ModelData LoadModelFile(const std::string&directoryPath,const std::string& filename);


private: // --- メンバ変数 ---

	// 共通リソースへのポインタ
	ModelCommon* modelCommon_ = nullptr;

	// CPU側のモデルデータ保持用
	ModelData modelData;

	// --- DirectXリソース (頂点バッファ) ---
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	VertexData* vertexData = nullptr; // マップ用ポインタ

	// --- DirectXリソース (マテリアル定数バッファ) ---
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Material* materialData = nullptr; // マップ用ポインタ


private: // --- プライベート関数 ---

	// 頂点バッファの作成
	void CreateVertexData();

	// マテリアルバッファの作成
	void CreateMaterialData();
};
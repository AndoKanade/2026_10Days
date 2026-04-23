#include "Model.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cassert>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

void Model::Initialize(ModelCommon* modelCommon,const std::string& directorypath,const std::string& filename){
	this->modelCommon_ = modelCommon;

	// モデルデータの読み込み (.obj)
	// TODO: 将来的には引数でファイル名を受け取るように変更する
	modelData = LoadModelFile(directorypath,filename);

	// テクスチャの読み込み (.mtlから取得したパスを使用)
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);

	// テクスチャ番号(Index)を取得して保存
	modelData.material.textureIndex =
		TextureManager::GetInstance()->GetSrvIndex(modelData.material.textureFilePath);

	// バッファの生成 (頂点・マテリアル)
	CreateVertexData();
	CreateMaterialData();
}

void Model::Draw(){
	// コマンドリストを取得
	auto* commandList = modelCommon_->GetDxCommon()->commandList.Get();

	// 1. 頂点バッファ(VBV)をセット
	commandList->IASetVertexBuffers(0,1,&vertexBufferView);

	// 2. マテリアルCBufferをセット (RootParameter[0])
	commandList->SetGraphicsRootConstantBufferView(
		0,materialResource->GetGPUVirtualAddress());

	// 3. テクスチャSRVをセット (RootParameter[2])
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle =
		SrvManager::GetInstance()->GetGPUDescriptorHandle(modelData.material.textureIndex);
	commandList->SetGraphicsRootDescriptorTable(2,textureSrvHandle);

	// 4. 描画コマンド発行
	commandList->DrawInstanced(UINT(modelData.vertices.size()),1,0,0);
}

Model::MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath,const std::string& filename){
	MaterialData materialData;
	std::string line;
	std::ifstream file(directoryPath + "/" + filename);

	assert(file.is_open());

	while(std::getline(file,line)){
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// "map_Kd": テクスチャファイル名
		if(identifier == "map_Kd"){
			std::string textureFilename;
			s >> textureFilename;
			// パスを結合して保存
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

Model::ModelData Model::LoadModelFile(const std::string& directoryPath,const std::string& filename){
	ModelData modelData;
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;

	// 1. Sceneを構築（DirectX12向け設定）
	const aiScene* scene = importer.ReadFile(filePath.c_str(),
		aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate);

	assert(scene && scene->HasMeshes());

	// 2. Meshの解析
	for(uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex){
		aiMesh* mesh = scene->mMeshes[meshIndex];

		assert(mesh->HasNormals());
		assert(mesh->HasTextureCoords(0));

		// Face（面）の解析
		for(uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex){
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3); // 三角形のみ

			// Vertex（頂点）の解析
			for(uint32_t element = 0; element < face.mNumIndices; ++element){
				uint32_t vertexIndex = face.mIndices[element];

				aiVector3D& position = mesh->mVertices[vertexIndex];
				aiVector3D& normal = mesh->mNormals[vertexIndex];
				aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];

				VertexData vertex;
				vertex.position = {position.x, position.y, position.z, 1.0f};
				vertex.normal = {normal.x, normal.y, normal.z};
				vertex.texcoord = {texcoord.x, texcoord.y};

				// 右手系から左手系への変換（資料に基づきx軸を反転）
				vertex.position.x *= -1.0f;
				vertex.normal.x *= -1.0f;

				modelData.vertices.push_back(vertex);
			}
		}
	}

	// 3. Materialの解析
	for(uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex){
		aiMaterial* material = scene->mMaterials[materialIndex];
		if(material->GetTextureCount(aiTextureType_DIFFUSE) != 0){
			aiString textureFilePath;
			material->GetTexture(aiTextureType_DIFFUSE,0,&textureFilePath);
			modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();

			// 資料の「最後にロードされたものを使う」挙動を維持しつつ、
			// 1つ見つかったら抜ける場合は break; を入れてもOK
		}
	}

	return modelData;
}

void Model::CreateVertexData(){
	// リソース作成
	vertexResource = modelCommon_->GetDxCommon()->CreateBufferResource(
		sizeof(VertexData) * modelData.vertices.size());

	// VBVの設定
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// データの書き込み (Map -> memcpy -> Unmap)
	VertexData* vertexDataPtr = nullptr;
	vertexResource->Map(0,nullptr,reinterpret_cast<void**>(&vertexDataPtr));
	std::memcpy(vertexDataPtr,modelData.vertices.data(),sizeof(VertexData) * modelData.vertices.size());
	vertexResource->Unmap(0,nullptr);
}

void Model::CreateMaterialData(){
	// ■■■ 1. バッファサイズの計算 (256バイトアライメント) ■■■
	// これをしないと、GPUが「データが足りない！(範囲外アクセス)」とエラーを出します
	size_t sizeInBytes = (sizeof(Material) + 0xff) & ~0xff;

	// リソース作成 (計算した sizeInBytes を使う)
	materialResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeInBytes);

	// データを書き込むためのアドレスを取得
	materialResource->Map(0,nullptr,reinterpret_cast<void**>(&materialData));

	// 初期値設定
	materialData->color = Vector4(1.0f,1.0f,1.0f,1.0f);
	materialData->enableLighting = 1;
	materialData->uvTransform = MakeIdentity4x4();

	// ■■■ 2. shininess の初期化を追加 ■■■
	materialData->shininess = 50.0f; // 輝きの鋭さ (とりあえず50.0f)
}
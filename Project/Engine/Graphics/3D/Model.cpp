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

// 初期化
void Model::Initialize(ModelCommon* modelCommon,const std::string& directorypath,const std::string& filename){
	this->modelCommon_ = modelCommon;

	// モデルデータの読み込み
	modelData = LoadModelFile(directorypath,filename);

	// テクスチャの読み込みとIndex保持
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetSrvIndex(modelData.material.textureFilePath);

	// 各種リソース生成
	CreateVertexData();
	CreateMaterialData();
}

// 描画
void Model::Draw(uint32_t skyboxTextureIndex,D3D12_GPU_VIRTUAL_ADDRESS cameraAddress){
	auto* commandList = modelCommon_->GetDxCommon()->commandList.Get();

	// 1. 頂点バッファ(VBV)をセット
	commandList->IASetVertexBuffers(0,1,&vertexBufferView);

	// 2. テクスチャSRVをセット (register t0)
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(modelData.material.textureIndex);
	commandList->SetGraphicsRootDescriptorTable(2,textureSrvHandle);

	// 3. 環境マップ用SRVをセット (register t1)
	D3D12_GPU_DESCRIPTOR_HANDLE skyboxSrvHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(skyboxTextureIndex);
	commandList->SetGraphicsRootDescriptorTable(3,skyboxSrvHandle);

	// 4. カメラCBufferをセット (register b3)
	commandList->SetGraphicsRootConstantBufferView(5,cameraAddress);

	// ※ マテリアル(b0)と行列(b1)は Obj3D::Draw 側でセットするため、ここでは行わない

	// 5. 描画コマンド発行
	commandList->DrawInstanced(UINT(modelData.vertices.size()),1,0,0);
}

// .objファイルの読み込み (Assimp使用)
Model::ModelData Model::LoadModelFile(const std::string& directoryPath,const std::string& filename){
	ModelData modelData;
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;

	const aiScene* scene = importer.ReadFile(filePath.c_str(),aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate);
	assert(scene && scene->HasMeshes());

	for(uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex){
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals() && mesh->HasTextureCoords(0));

		for(uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex){
			aiFace& face = mesh->mFaces[faceIndex];
			for(uint32_t element = 0; element < face.mNumIndices; ++element){
				uint32_t vertexIndex = face.mIndices[element];
				aiVector3D& pos = mesh->mVertices[vertexIndex];
				aiVector3D& norm = mesh->mNormals[vertexIndex];
				aiVector3D& tex = mesh->mTextureCoords[0][vertexIndex];

				VertexData vertex;
				vertex.position = {-pos.x, pos.y, pos.z, 1.0f}; // 右手→左手変換
				vertex.normal = {-norm.x, norm.y, norm.z};      // 右手→左手変換
				vertex.texcoord = {tex.x, tex.y};
				modelData.vertices.push_back(vertex);
			}
		}
	}

	for(uint32_t i = 0; i < scene->mNumMaterials; ++i){
		aiMaterial* mat = scene->mMaterials[i];
		if(mat->GetTextureCount(aiTextureType_DIFFUSE) > 0){
			aiString path;
			mat->GetTexture(aiTextureType_DIFFUSE,0,&path);
			modelData.material.textureFilePath = directoryPath + "/" + path.C_Str();
		}
	}
	return modelData;
}

// 頂点バッファ生成
void Model::CreateVertexData(){
	vertexResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	VertexData* ptr = nullptr;
	vertexResource->Map(0,nullptr,reinterpret_cast<void**>(&ptr));
	std::memcpy(ptr,modelData.vertices.data(),sizeof(VertexData) * modelData.vertices.size());
	vertexResource->Unmap(0,nullptr);
}

// マテリアルバッファ生成
void Model::CreateMaterialData(){
	size_t sizeInBytes = (sizeof(Material) + 0xff) & ~0xff;
	materialResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeInBytes);
	materialResource->Map(0,nullptr,reinterpret_cast<void**>(&materialData));

	// 初期値設定
	materialData->color = {1.0f, 1.0f, 1.0f, 1.0f};
	materialData->enableLighting = 1;
	materialData->uvTransform = MakeIdentity4x4();
	materialData->shininess = 50.0f;
	materialData->environmentCoefficient = 0.0f; // ★環境マップ係数の初期化を追加
}
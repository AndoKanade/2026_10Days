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

// ====================================================================
// 初期化・リソース生成
// ====================================================================

void Model::Initialize(ModelCommon* modelCommon,const std::string& directorypath,const std::string& filename){
	this->modelCommon_ = modelCommon;

	modelData = LoadModelFile(directorypath,filename);

	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetSrvIndex(modelData.material.textureFilePath);

	CreateVertexData();
	CreateIndexData();
	CreateMaterialData();
}

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

void Model::CreateIndexData(){
	size_t sizeInBytes = sizeof(uint32_t) * modelData.indices.size();

	indexResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeInBytes);

	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = UINT(sizeInBytes);
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* ptr = nullptr;
	indexResource->Map(0,nullptr,reinterpret_cast<void**>(&ptr));
	std::memcpy(ptr,modelData.indices.data(),sizeInBytes);
	indexResource->Unmap(0,nullptr);
}

void Model::CreateMaterialData(){
	size_t sizeInBytes = (sizeof(Material) + 0xff) & ~0xff;
	materialResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeInBytes);
	materialResource->Map(0,nullptr,reinterpret_cast<void**>(&materialData));

	materialData->color = {1.0f, 1.0f, 1.0f, 1.0f};
	materialData->enableLighting = 1;
	materialData->uvTransform = MakeIdentity4x4();
	materialData->shininess = 50.0f;
	materialData->environmentCoefficient = 0.0f;
}

// ====================================================================
// ファイル読み込み処理 (Assimp)
// ====================================================================

Model::ModelData Model::LoadModelFile(const std::string& directoryPath,const std::string& filename){
	ModelData modelData;
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;

	const aiScene* scene = importer.ReadFile(filePath.c_str(),aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate);
	assert(scene && scene->HasMeshes());

	if(scene->mRootNode){
		modelData.rootNode = ReadNode(scene->mRootNode);
	}

	for(uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex){
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals() && mesh->HasTextureCoords(0));

		uint32_t vertexOffset = static_cast<uint32_t>(modelData.vertices.size());

		// 頂点データの抽出
		for(uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex){
			aiVector3D& pos = mesh->mVertices[vertexIndex];
			aiVector3D& norm = mesh->mNormals[vertexIndex];
			aiVector3D& tex = mesh->mTextureCoords[0][vertexIndex];

			VertexData vertex;
			vertex.position = {-pos.x, pos.y, pos.z, 1.0f}; // 右手→左手変換
			vertex.normal = {-norm.x, norm.y, norm.z};      // 右手→左手変換
			vertex.texcoord = {tex.x, tex.y};
			modelData.vertices.push_back(vertex);
		}

		// インデックスデータの抽出
		for(uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex){
			aiFace& face = mesh->mFaces[faceIndex];
			for(uint32_t element = 0; element < face.mNumIndices; ++element){
				modelData.indices.push_back(face.mIndices[element] + vertexOffset);
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

Node Model::ReadNode(aiNode* node){
	Node result;
	result.name = node->mName.C_Str();

	aiVector3D scale,translate;
	aiQuaternion rotate;
	node->mTransformation.Decompose(scale,rotate,translate);

	result.transform.scale = {scale.x, scale.y, scale.z};
	result.transform.rotate = {rotate.x, -rotate.y, -rotate.z, rotate.w};
	result.transform.translate = {-translate.x, translate.y, translate.z};

	Matrix4x4 matScale = MakeScaleMatrix(result.transform.scale);
	Matrix4x4 matRotate = MakeRotateMatrix(result.transform.rotate);
	Matrix4x4 matTranslate = MakeTranslateMatrix(result.transform.translate);

	result.localMatrix = Multiply(Multiply(matScale,matRotate),matTranslate);

	result.children.resize(node->mNumChildren);
	for(uint32_t i = 0; i < node->mNumChildren; ++i){
		result.children[i] = ReadNode(node->mChildren[i]);
	}

	return result;
}

// ====================================================================
// 描画・更新処理
// ====================================================================

void Model::Draw(uint32_t skyboxTextureIndex,D3D12_GPU_VIRTUAL_ADDRESS cameraAddress){
	auto* commandList = modelCommon_->GetDxCommon()->commandList.Get();

	// 頂点バッファをセット
	commandList->IASetVertexBuffers(0,1,&vertexBufferView);

	// 指摘箇所：ここでインデックスバッファをセットする
	commandList->IASetIndexBuffer(&indexBufferView);

	// マテリアルやカメラのセット
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(modelData.material.textureIndex);
	commandList->SetGraphicsRootDescriptorTable(2,textureSrvHandle);

	D3D12_GPU_DESCRIPTOR_HANDLE skyboxSrvHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(skyboxTextureIndex);
	commandList->SetGraphicsRootDescriptorTable(3,skyboxSrvHandle);

	commandList->SetGraphicsRootConstantBufferView(5,cameraAddress);

	// インデックス描画コマンド
	commandList->DrawIndexedInstanced(UINT(modelData.indices.size()),1,0,0,0);
}

void Model::SetTexture(const std::string& texturefilePath){
	TextureManager::GetInstance()->LoadTexture(texturefilePath);

	modelData.material.textureIndex = TextureManager::GetInstance()->GetSrvIndex(texturefilePath);
	modelData.material.textureFilePath = texturefilePath;
}
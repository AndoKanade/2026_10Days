#include "Model.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "SkinCluster.h"
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
    // 共通モデル管理クラスの保持
    this->modelCommon_ = modelCommon;

    // ファイルからモデルデータの読み込み
    modelData = LoadModelFile(directorypath,filename);

    // テクスチャのロードとインデックス取得
    TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
    modelData.material.textureIndex = TextureManager::GetInstance()->GetSrvIndex(modelData.material.textureFilePath);

    // 各種バッファデータの生成
    CreateVertexData();
    CreateIndexData();
    CreateMaterialData();
}

void Model::CreateVertexData(){
    // 頂点バッファリソースの生成
    vertexResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());

    // 頂点バッファビューの設定
    vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
    vertexBufferView.StrideInBytes = sizeof(VertexData);

    // データのコピー
    VertexData* ptr = nullptr;
    vertexResource->Map(0,nullptr,reinterpret_cast<void**>(&ptr));
    std::memcpy(ptr,modelData.vertices.data(),sizeof(VertexData) * modelData.vertices.size());
    vertexResource->Unmap(0,nullptr);
}

void Model::CreateIndexData(){
    // インデックスデータサイズ計算
    size_t sizeInBytes = sizeof(uint32_t) * modelData.indices.size();

    // インデックスバッファリソースの生成
    indexResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeInBytes);

    // インデックスバッファビューの設定
    indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
    indexBufferView.SizeInBytes = UINT(sizeInBytes);
    indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    // データのコピー
    uint32_t* ptr = nullptr;
    indexResource->Map(0,nullptr,reinterpret_cast<void**>(&ptr));
    std::memcpy(ptr,modelData.indices.data(),sizeInBytes);
    indexResource->Unmap(0,nullptr);
}

void Model::CreateMaterialData(){
    // 256バイトアライメントを考慮したマテリアルバッファ生成
    size_t sizeInBytes = (sizeof(Material) + 0xff) & ~0xff;
    materialResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeInBytes);
    materialResource->Map(0,nullptr,reinterpret_cast<void**>(&materialData));

    // マテリアルの初期設定
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

    // Assimpによる読み込みと三角面化
    const aiScene* scene = importer.ReadFile(filePath.c_str(),aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate);
    assert(scene && scene->HasMeshes());

    // ノードの読み込み
    if(scene->mRootNode){
        modelData.rootNode = ReadNode(scene->mRootNode);
    }

    // メッシュデータの解析
    for(uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex){
        aiMesh* mesh = scene->mMeshes[meshIndex];
        assert(mesh->HasNormals() && mesh->HasTextureCoords(0));

        uint32_t vertexOffset = static_cast<uint32_t>(modelData.vertices.size());

        // 1. 頂点データの抽出
        for(uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex){
            aiVector3D& pos = mesh->mVertices[vertexIndex];
            aiVector3D& norm = mesh->mNormals[vertexIndex];

            VertexData vertex;
            // 左手系へ変換: Xを反転
            vertex.position = {-pos.x, pos.y, pos.z, 1.0f};
            vertex.normal = {-norm.x, norm.y, norm.z};
            vertex.texcoord = {mesh->mTextureCoords[0][vertexIndex].x, mesh->mTextureCoords[0][vertexIndex].y};
            modelData.vertices.push_back(vertex);
        }

        // 2. インデックスデータの抽出
        for(uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex){
            aiFace& face = mesh->mFaces[faceIndex];
            for(uint32_t element = 0; element < face.mNumIndices; ++element){
                modelData.indices.push_back(face.mIndices[element] + vertexOffset);
            }
        }

        // 3. ボーンデータの抽出（ここを修正）
        for(uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex){
            aiBone* bone = mesh->mBones[boneIndex];
            std::string jointName = bone->mName.C_Str();
            Model::JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

            // AIの行列を一旦Matrix4x4にコピー
            aiMatrix4x4 aiMat = bone->mOffsetMatrix;

            // 左手系への変換行列を作成：X成分を反転させる
            // aiMatrix4x4 は [行][列] の順
            aiMatrix4x4 leftHandMat;
            leftHandMat.a1 = -aiMat.a1; leftHandMat.a2 = aiMat.a2; leftHandMat.a3 = aiMat.a3; leftHandMat.a4 = -aiMat.a4;
            leftHandMat.b1 = -aiMat.b1; leftHandMat.b2 = aiMat.b2; leftHandMat.b3 = aiMat.b3; leftHandMat.b4 = aiMat.b4;
            leftHandMat.c1 = -aiMat.c1; leftHandMat.c2 = aiMat.c2; leftHandMat.c3 = aiMat.c3; leftHandMat.c4 = aiMat.c4;
            leftHandMat.d1 = -aiMat.d1; leftHandMat.d2 = aiMat.d2; leftHandMat.d3 = aiMat.d3; leftHandMat.d4 = aiMat.d4;

            // 最終的に自作Matrix4x4へ
            Matrix4x4 offsetMatrix;
            for(uint32_t r = 0; r < 4; ++r){
                for(uint32_t c = 0; c < 4; ++c){
                    float val = bone->mOffsetMatrix[r][c];
                    if(c == 0) val *= -1.0f; // X軸成分の反転
                    offsetMatrix.m[r][c] = val;
                }
            }

            jointWeightData.inverseBindPoseMatrix = offsetMatrix;

            for(uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex){
                jointWeightData.vertexWeights.push_back({
                    bone->mWeights[weightIndex].mWeight,
                    bone->mWeights[weightIndex].mVertexId + vertexOffset
                    });
            }
        }
    }

    // マテリアル情報の抽出
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

    // ノードの変換行列から分解
    aiVector3D scale,translate;
    aiQuaternion rotate;
    node->mTransformation.Decompose(scale,rotate,translate);

    // 左手系への変換
    result.transform.scale = {scale.x, scale.y, scale.z};
    result.transform.rotate = {rotate.x, -rotate.y, -rotate.z, rotate.w};
    result.transform.translate = {-translate.x, translate.y, translate.z};

    // ローカル行列の生成
    Matrix4x4 matScale = MakeScaleMatrix(result.transform.scale);
    Matrix4x4 matRotate = MakeRotateMatrix(result.transform.rotate);
    Matrix4x4 matTranslate = MakeTranslateMatrix(result.transform.translate);

    result.localMatrix = Multiply(Multiply(matScale,matRotate),matTranslate);

    // 子ノードの再帰的読み込み
    result.children.resize(node->mNumChildren);
    for(uint32_t i = 0; i < node->mNumChildren; ++i){
        result.children[i] = ReadNode(node->mChildren[i]);
    }

    return result;
}

// ====================================================================
// 描画・更新処理
// ====================================================================

void Model::Draw(uint32_t skyboxSRVIndex,D3D12_GPU_VIRTUAL_ADDRESS cameraAddress,SkinCluster* skinCluster){
    auto* commandList = modelCommon_->GetDxCommon()->GetCommandList();

    // 頂点バッファの設定
    if(skinCluster){
        D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {vertexBufferView, skinCluster->GetInfluenceBufferView()};
        commandList->IASetVertexBuffers(0,2,vbvs);
    } else{
        commandList->IASetVertexBuffers(0,1,&vertexBufferView);
    }
    // インデックスバッファの設定
    commandList->IASetIndexBuffer(&indexBufferView);

    // 描画実行
    commandList->DrawIndexedInstanced(UINT(modelData.indices.size()),1,0,0,0);
}

void Model::SetTexture(const std::string& texturefilePath){
    TextureManager::GetInstance()->LoadTexture(texturefilePath);

    modelData.material.textureIndex = TextureManager::GetInstance()->GetSrvIndex(texturefilePath);
    modelData.material.textureFilePath = texturefilePath;
}
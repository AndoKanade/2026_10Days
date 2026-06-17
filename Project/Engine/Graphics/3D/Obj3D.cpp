#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Model.h"
#include "TextureManager.h"
#include "Sprite.h"
#include "MyMath.h"
#include <cassert>
#include "ModelManager.h"
#include "Camera.h"

void Obj3D::Initialize(Obj3dCommon* object3dCommon){
    this->object3dCommon = object3dCommon;
    this->camera = object3dCommon->GetDefaultCamera();

    materialResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(Model::Material));

    materialResource->Map(0,nullptr,reinterpret_cast<void**>(&materialData));
    if(materialData){
        materialData->color = {1.0f, 1.0f, 1.0f, 1.0f};
        materialData->enableLighting = 1;
        materialData->uvTransform = MakeIdentity4x4(); // 単位行列
        materialData->shininess = 20.0f;
        materialData->environmentCoefficient = 0.0f;
    }

    // トランスフォームの初期化
    transform = {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

    // リソース（バッファ）の生成
    CreateTransformationMatrixData();
}

void Obj3D::Update(){
    // 1. ローカル行列の計算 (S * R * T)
    Matrix4x4 localMatrix;

    if(isUseQuaternion_){
        // クォータニオンを使用する場合の正しい行列合成
        Matrix4x4 matScale = MakeScaleMatrix(transform.scale);
        Matrix4x4 matRotate = MakeRotateMatrix(quaternion_); // クォータニオン用の回転行列生成関数
        Matrix4x4 matTranslate = MakeTranslateMatrix(transform.translate);
        localMatrix = Multiply(Multiply(matScale,matRotate),matTranslate);
    } else{
        // 従来の Vector3 (オイラー角) を使用する場合
        localMatrix = MakeAffineMatrix(transform.scale,transform.rotate,transform.translate);
    }

    // 2. ワールド行列の計算
    Matrix4x4 worldMatrix = localMatrix;

    // 親がいれば親のワールド行列を乗算する
    std::shared_ptr<Obj3D> parentPtr = parent_.lock();
    if(parentPtr){
        worldMatrix = Multiply(localMatrix,parentPtr->GetWorldMatrix());
    }

    // 3. カメラ行列との合成 (WVP)
    Matrix4x4 worldViewProjectionMatrix;
    Camera* cameraPtr = this->camera?this->camera:object3dCommon->GetDefaultCamera();

    if(cameraPtr){
        const Matrix4x4& viewProjectionMatrix = cameraPtr->GetViewProjectionMatrix();
        worldViewProjectionMatrix = Multiply(worldMatrix,viewProjectionMatrix);
    } else{
        worldViewProjectionMatrix = worldMatrix;
    }

    // 4. GPU上のバッファに書き込み
    transformationMatrixData->WVP = worldViewProjectionMatrix;
    transformationMatrixData->World = worldMatrix;
    // 逆転置行列の計算と転送
    transformationMatrixData->WorldInverseTranspose = Transpose(Inverse(worldMatrix));

    // 追加: アニメーションとスキニングの更新
    if(isSkinning_){
        animationTime_ += 1.0f / 60.0f;
        animationTime_ = std::fmod(animationTime_,animation_.duration);

        skeleton_.ApplyAnimation(animation_,animationTime_);
        skeleton_.Update();

        skinCluster_.Update(skeleton_);
    }
}

void Obj3D::Draw(){
    auto* commandList = object3dCommon->GetDxCommon()->GetCommandList();

    // 1. マテリアル・トランスフォームのセット（これは共通）
    commandList->SetGraphicsRootConstantBufferView(0,materialResource->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1,transformationMatrixResource->GetGPUVirtualAddress());

    if(model){
        Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
        if(activeCamera == nullptr) return;

        uint32_t skyboxSRVIndex = TextureManager::GetInstance()->GetSrvIndex("resource/Skybox/rostock_laage_airport_4k.dds");

        if(isSkinning_){
            // 1. スキニング用パイプラインのセット
            commandList->SetPipelineState(object3dCommon->GetSkinningGraphicsPipelineState());
            commandList->SetGraphicsRootShaderResourceView(8,skinCluster_.GetPaletteResource()->GetGPUVirtualAddress());

            model->Draw(skyboxSRVIndex,activeCamera->GetGPUVirtualAddress(),&skinCluster_);
        } else{
            // ★通常モデル用パイプラインのセット
            commandList->SetPipelineState(object3dCommon->GetGraphicsPipelineState());
            model->Draw(skyboxSRVIndex,activeCamera->GetGPUVirtualAddress());
        }
    }
}

void Obj3D::SetModel(const std::string& filePath){
    model = ModelManager::GetInstance()->FindModel(filePath);
}

void Obj3D::SetParent(const std::weak_ptr<Obj3D>& parent){
    this->parent_ = parent;
}

void Obj3D::CreateTransformationMatrixData(){
    // 256バイトアライメントを考慮したリソース作成
    size_t sizeInBytes = (sizeof(TransformationMatrix) + 0xff) & ~0xff;
    transformationMatrixResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeInBytes);

    transformationMatrixResource->Map(0,nullptr,reinterpret_cast<void**>(&transformationMatrixData));

    // 初期化
    transformationMatrixData->WVP = MakeIdentity4x4();
    transformationMatrixData->World = MakeIdentity4x4();
    transformationMatrixData->WorldInverseTranspose = MakeIdentity4x4();
}

void Obj3D::CreateMaterialData(){
    materialResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(Model::Material));
    materialResource->Map(0,nullptr,reinterpret_cast<void**>(&materialData));
}

void Obj3D::LoadAnimation(const std::string& directoryPath,const std::string& filename){
    if(!model){
        return;
    }

    animation_ = LoadAnimationFile(directoryPath,filename);
    animationTime_ = 0.0f;

    skeleton_.Create(model->GetRootNode());
    skinCluster_.Initialize(object3dCommon->GetDxCommon(),model->GetModelData(),skeleton_);

    isSkinning_ = true;
}
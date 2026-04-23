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
    Matrix4x4 localMatrix = MakeAffineMatrix(transform.scale,transform.rotate,transform.translate);

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
}

void Obj3D::Draw(){
    auto* commandList = object3dCommon->GetDxCommon()->GetCommandList();

    commandList->SetGraphicsRootConstantBufferView(
        0,materialResource->GetGPUVirtualAddress());

    // 座標変換行列CBufferの設定 (b1)
    commandList->SetGraphicsRootConstantBufferView(
        1,transformationMatrixResource->GetGPUVirtualAddress());

    // モデルの描画
    if(model){
        // 1. アクティブなカメラを取得
        Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();

        if(activeCamera == nullptr){
            // カメラがないなら描画をスキップするか、デフォルトのアドレスを渡す
            return;
        }

        uint32_t skyboxIndex = 0;

        uint32_t skyboxSRVIndex = TextureManager::GetInstance()->GetSrvIndex("resource/Skybox/rostock_laage_airport_4k.dds");

        // 3. 引数を渡してDrawを呼ぶ
        model->Draw(skyboxSRVIndex,activeCamera->GetGPUVirtualAddress());
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

#include "Obj3D.h"
#include "Obj3dCommon.h"
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

    // 座標変換行列CBufferの設定 (b1)
    commandList->SetGraphicsRootConstantBufferView(
        1,transformationMatrixResource->GetGPUVirtualAddress());

    // モデルの描画
    if(model){
        model->Draw();
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
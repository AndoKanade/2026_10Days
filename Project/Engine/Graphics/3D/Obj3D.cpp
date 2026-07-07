#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Model.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "Logger.h"
#include "Sprite.h"
#include "MyMath.h"
#include <cassert>
#include "ModelManager.h"

// 初期化
void Obj3D::Initialize(Obj3dCommon* object3dCommon){
	// 共通のオブジェクト共通設定を保持
	this->object3dCommon = object3dCommon;
	// デフォルトカメラの取得
	this->camera = object3dCommon->GetDefaultCamera();

	// マテリアル用リソースの確保
	materialResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(Model::Material));

	// リソースのマップと初期値の設定
	materialResource->Map(0,nullptr,reinterpret_cast<void**>(&materialData));
	if(materialData){
		materialData->color = {1.0f, 1.0f, 1.0f, 1.0f};
		materialData->enableLighting = 1;
		materialData->uvTransform = MakeIdentity4x4();
		materialData->shininess = 20.0f;
		materialData->environmentCoefficient = 0.0f;
	}

	// トランスフォームの初期化
	transform = {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

	// 変換行列用リソースの生成
	CreateTransformationMatrixData();
}

// 更新処理
void Obj3D::Update(){
	// ローカル行列の計算 (S * R * T)
	Matrix4x4 localMatrix;

	// カメラが設定されていない場合はデフォルトを取得
	if(this->camera == nullptr){
		if(object3dCommon != nullptr && object3dCommon->GetDefaultCamera() != nullptr){
			this->camera = object3dCommon->GetDefaultCamera();
		}
	}

	// デバッグ用ログ出力
	if(this->camera == nullptr){
		Logger::Log("Camera is still NULL!!\n");
	}

	// クォータニオンとオイラー角での行列生成分岐
	if(isUseQuaternion_){
		Matrix4x4 matScale = MakeScaleMatrix(transform.scale);
		Matrix4x4 matRotate = MakeRotateMatrix(quaternion_);
		Matrix4x4 matTranslate = MakeTranslateMatrix(transform.translate);
		localMatrix = Multiply(Multiply(matScale,matRotate),matTranslate);
	} else{
		localMatrix = MakeAffineMatrix(transform.scale,transform.rotate,transform.translate);
	}

	// ワールド行列の計算
	Matrix4x4 worldMatrix = localMatrix;

	// 親ノードがある場合はワールド行列を合成
	std::shared_ptr<Obj3D> parentPtr = parent_.lock();
	if(parentPtr){
		worldMatrix = Multiply(localMatrix,parentPtr->GetWorldMatrix());
	}

	// カメラ行列との合成 (WVP行列)
	Matrix4x4 worldViewProjectionMatrix;
	Camera* cameraPtr = this->camera?this->camera:object3dCommon->GetDefaultCamera();

	if(cameraPtr){
		const Matrix4x4& viewProjectionMatrix = cameraPtr->GetViewProjectionMatrix();
		worldViewProjectionMatrix = Multiply(worldMatrix,viewProjectionMatrix);
	} else{
		worldViewProjectionMatrix = worldMatrix;
	}

	// GPUバッファへのデータ書き込み
	transformationMatrixData->WVP = worldViewProjectionMatrix;
	transformationMatrixData->World = worldMatrix;
	// 逆転置行列の計算と転送
	transformationMatrixData->WorldInverseTranspose = Transpose(Inverse(worldMatrix));

	// アニメーションとスキニングの更新
	if(isSkinning_){
		animationTime_ += 1.0f / 60.0f;
		animationTime_ = std::fmod(animationTime_,animation_.duration);
		skeleton_.ApplyAnimation(animation_,animationTime_);
		skeleton_.Update();

		// 1. コマンドリストの取得 (これが必要です)
		auto* commandList = object3dCommon->GetDxCommon()->GetCommandList();
		assert(commandList != nullptr);

		// 2. スキニング計算の実行 (引数を合わせる)
		// ※ SkinningInformationのGPUアドレスを渡す必要があるため、
		// 必要に応じてバッファを作成・取得してください
		skinCluster_.Update(skeleton_,commandList,object3dCommon,skinCluster_.GetSkinningInfoAddress());

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = skinCluster_.GetSkinnedVertexBuffer();
		commandList->ResourceBarrier(1,&barrier);
	}
}

// 描画処理
void Obj3D::Draw(){
	auto* commandList = object3dCommon->GetDxCommon()->GetCommandList();

	// 共通リソースの取得
	auto* lightRes = ModelManager::GetInstance()->GetModelCommon()->GetLightResource();
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	uint32_t skyboxSRVIndex = TextureManager::GetInstance()->GetSrvIndex("resource/Skybox/rostock_laage_airport_4k.dds");

	// ルートシグネチャとPSOの切り替え
	if(isSkinning_){
		commandList->SetGraphicsRootSignature(object3dCommon->GetSkinningRootSignature());
		commandList->SetPipelineState(object3dCommon->GetSkinningGraphicsPipelineState());
	} else{
		commandList->SetGraphicsRootSignature(object3dCommon->GetRootSignature());
		commandList->SetPipelineState(object3dCommon->GetGraphicsPipelineState());
	}

	// 共通のルートパラメータ設定
	commandList->SetGraphicsRootConstantBufferView(0,materialResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1,transformationMatrixResource->GetGPUVirtualAddress());

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(model->GetModelData().material.textureIndex);
	commandList->SetGraphicsRootDescriptorTable(2,textureSrvHandle);

	D3D12_GPU_DESCRIPTOR_HANDLE skyboxSrvHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(skyboxSRVIndex);
	commandList->SetGraphicsRootDescriptorTable(3,skyboxSrvHandle);

	commandList->SetGraphicsRootConstantBufferView(4,lightRes->GetGPUVirtualAddress());
	if(activeCamera) commandList->SetGraphicsRootConstantBufferView(5,activeCamera->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(6,object3dCommon->GetPointLightDataGPU());
	commandList->SetGraphicsRootConstantBufferView(7,object3dCommon->GetSpotLightDataGPU());

	// スキニング特有のパラメータ設定と描画の呼び出し
	if(isSkinning_){
		commandList->SetGraphicsRootShaderResourceView(8,skinCluster_.GetPaletteAddress());
		model->Draw(skyboxSRVIndex,activeCamera->GetGPUVirtualAddress(),&skinCluster_);
	} else{
		model->Draw(skyboxSRVIndex,activeCamera->GetGPUVirtualAddress());
	}

}

// モデルのセット
void Obj3D::SetModel(const std::string& filePath){
	model = ModelManager::GetInstance()->FindModel(filePath);
}

// 親ノードのセット
void Obj3D::SetParent(const std::weak_ptr<Obj3D>& parent){
	this->parent_ = parent;
}

// 変換行列データの生成
void Obj3D::CreateTransformationMatrixData(){
	// 256バイトアライメントの計算
	size_t sizeInBytes = (sizeof(TransformationMatrix) + 0xff) & ~0xff;
	transformationMatrixResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeInBytes);

	transformationMatrixResource->Map(0,nullptr,reinterpret_cast<void**>(&transformationMatrixData));

	// 行列の初期化
	transformationMatrixData->WVP = MakeIdentity4x4();
	transformationMatrixData->World = MakeIdentity4x4();
	transformationMatrixData->WorldInverseTranspose = MakeIdentity4x4();
}

// マテリアルデータの生成
void Obj3D::CreateMaterialData(){
	materialResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(Model::Material));
	materialResource->Map(0,nullptr,reinterpret_cast<void**>(&materialData));
}

// アニメーションの読み込み
void Obj3D::LoadAnimation(const std::string& directoryPath,const std::string& filename){
	if(!model){
		return;
	}

	animation_ = LoadAnimationFile(directoryPath,filename);
	animationTime_ = 0.0f;

	// スケルトンとスキンクラスターの初期化
	skeleton_.Create(model->GetRootNode());
	skinCluster_.Initialize(object3dCommon->GetDxCommon(),model->GetModelData(),skeleton_);

	isSkinning_ = true;
}
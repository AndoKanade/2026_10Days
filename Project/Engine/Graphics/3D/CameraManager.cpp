#include "CameraManager.h"
#include <DXCommon.h>

/// <summary>
/// シングルトンインスタンスの取得
/// Meyers Singleton パターンを採用。
/// 初回呼び出し時に static 変数が初期化され、プログラム終了時に自動でデストラクタが呼ばれます。
/// </summary>
CameraManager* CameraManager::GetInstance(){
	static CameraManager instance;
	return &instance;
}

/// <summary>
/// 初期化
/// </summary>
void CameraManager::Initialize(){
	cameras_.clear();
	activeCamera_ = nullptr;

}

/// <summary>
/// 終了処理
/// </summary>
void CameraManager::Finalize(){
	// コンテナをクリアすることで、unique_ptr が管理する Camera インスタンスも破棄される
	cameras_.clear();
	activeCamera_ = nullptr;
}

/// <summary>
/// 更新処理
/// </summary>
void CameraManager::Update(){
	// アクティブなカメラがあれば更新処理を回す
	if(activeCamera_){
		activeCamera_->Update();
	}
}

/// <summary>
/// カメラ作成
/// </summary>
// CameraManager.cpp

void CameraManager::CreateCamera(const std::string& name,ID3D12Device* device){
	if(cameras_.contains(name)){
		return;
	}

	std::unique_ptr<Camera> newCamera = std::make_unique<Camera>();

	// 引数で受け取ったデバイスを使って初期化
	newCamera->Initialize(device);

	cameras_.emplace(name,std::move(newCamera));

	if(activeCamera_ == nullptr){
		activeCamera_ = cameras_[name].get();
	}
}

/// <summary>
/// アクティブカメラの切り替え
/// </summary>
void CameraManager::SetActiveCamera(const std::string& name){
	// 指定された名前のカメラが存在するか確認
	if(cameras_.contains(name)){
		// マップからポインタを取得してアクティブに設定
		activeCamera_ = cameras_.at(name).get();
	}
}

/// <summary>
/// 現在のアクティブカメラ取得
/// </summary>
Camera* CameraManager::GetActiveCamera() const{
	return activeCamera_;
}

/// <summary>
/// 名前指定でカメラ取得
/// </summary>
Camera* CameraManager::GetCamera(const std::string& name) const{
	if(cameras_.contains(name)){
		return cameras_.at(name).get();
	}
	return nullptr;
}
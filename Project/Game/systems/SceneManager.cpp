#include "SceneManager.h"
#include <cassert>

// -------------------------------------------------
// シングルトン管理
// -------------------------------------------------

SceneManager* SceneManager::GetInstance(){
	// 静的ローカル変数としてインスタンスを保持 (Meyers Singleton)
	// 初回呼び出し時に生成され、プログラム終了時に自動的に破棄されます。
	static SceneManager instance;
	return &instance;
}

void SceneManager::Finalize(){
	// 現在アクティブなシーンがあれば終了処理を実行
	if(scene_){
		scene_->Finalize();
	}
	// unique_ptr をリセットしてメモリを解放
	scene_.reset();
}

// -------------------------------------------------
// メインループ処理
// -------------------------------------------------

void SceneManager::Update(){
	// --- シーン切り替えリクエストの処理 ---
	// 次のシーン名が設定されている場合、切り替え処理を行う
	if(!nextSceneName_.empty()){

		// 1. 現在のシーンの終了処理
		if(scene_){
			scene_->Finalize();
		}

		// 2. 新しいシーンの生成
		// unique_ptr::reset() を使用して、ファクトリーが生成したインスタンスの所有権を受け取る
		if(sceneFactory_){
			scene_ = sceneFactory_->CreateScene(nextSceneName_);
		}

		// 3. 新しいシーンの初期化と依存関係の注入
		if(scene_){
			// シーンマネージャ自身の参照をセット (シーン遷移リクエスト用)
			scene_->SetSceneManager(this);

			// 共通リソース (モデル、入力、スプライト等) が揃っているか確認して渡す
			if(object3dCommon_ && input_ && spriteCommon_){
				scene_->Initialize(object3dCommon_,input_,spriteCommon_);
			}
		}

		// リクエストをクリア (切り替え完了)
		nextSceneName_.clear();
	}

	// --- 現在のシーンの更新 ---
	if(scene_){
		scene_->Update();
	}
}

void SceneManager::Draw(){
	// 現在のシーンがあれば描画処理を実行
	if(scene_){
		scene_->Draw();
	}
}

// -------------------------------------------------
// シーン制御
// -------------------------------------------------

void SceneManager::ChangeScene(const std::string& sceneName){
	// ファクトリーがセットされていない状態で切り替えはできないためアサート
	assert(sceneFactory_);

	// 次のシーン名を保存
	// ※実際の切り替え処理は、次フレームの Update() の冒頭で行われます
	nextSceneName_ = sceneName;
}
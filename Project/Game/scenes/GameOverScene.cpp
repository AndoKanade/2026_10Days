#include "GameOverScene.h"

#include "Input.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "WinAPI.h"

namespace{
	// 背景に使うテクスチャ（専用画像がないため既存テクスチャを赤で着色して流用）
	const std::string kBackgroundTexture = "resource/uvChecker.png";
	const Vector4 kOverColor = {1.0f, 0.3f, 0.3f, 1.0f};
}

GameOverScene::GameOverScene() = default;
GameOverScene::~GameOverScene() = default;

// 初期化処理
void GameOverScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// --- 背景スプライトの生成 ---
	TextureManager::GetInstance()->LoadTexture(kBackgroundTexture);

	background_ = std::make_unique<Sprite>();
	background_->Initialize(spriteCommon_,kBackgroundTexture);
	background_->SetPosition({0.0f, 0.0f});
	background_->SetSize({float(WinAPI::kClientWidth), float(WinAPI::kClientHeight)});
	background_->SetColor(kOverColor);
}

void GameOverScene::Finalize(){
	// unique_ptrにより自動解放されるため処理なし
}

// 更新処理
void GameOverScene::Update(){
	if(background_){
		background_->Update();
	}

	// スペースキーでタイトル画面へ遷移
	if(input_->TriggerKey(DIK_SPACE)){
		SceneManager::GetInstance()->ChangeScene("TITLE");
	}
}

// 描画処理
void GameOverScene::Draw(){
	if(spriteCommon_ && background_){
		spriteCommon_->Draw();
		background_->Draw();
	}
}

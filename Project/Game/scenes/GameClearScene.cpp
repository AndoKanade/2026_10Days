#include "GameClearScene.h"

#include "Input.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "WinAPI.h"

namespace{
	// 背景に使うテクスチャ（専用画像がないため既存テクスチャを緑で着色して流用）
	const std::string kBackgroundTexture = "resource/uvChecker.png";
	const Vector4 kClearColor = {0.4f, 1.0f, 0.4f, 1.0f};
}

GameClearScene::GameClearScene() = default;
GameClearScene::~GameClearScene() = default;

// 初期化処理
void GameClearScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// --- 背景スプライトの生成 ---
	TextureManager::GetInstance()->LoadTexture(kBackgroundTexture);

	background_ = std::make_unique<Sprite>();
	background_->Initialize(spriteCommon_,kBackgroundTexture);
	background_->SetPosition({0.0f, 0.0f});
	background_->SetSize({float(WinAPI::kClientWidth), float(WinAPI::kClientHeight)});
	background_->SetColor(kClearColor);
}

void GameClearScene::Finalize(){
	// unique_ptrにより自動解放されるため処理なし
}

// 更新処理
void GameClearScene::Update(){
	if(background_){
		background_->Update();
	}

	// スペースキーでゲームオーバー画面へ遷移
	if(input_->TriggerKey(DIK_SPACE)){
		SceneManager::GetInstance()->ChangeScene("GAMEOVER");
	}
}

// 描画処理
void GameClearScene::Draw(){
	if(spriteCommon_ && background_){
		spriteCommon_->Draw();
		background_->Draw();
	}
}

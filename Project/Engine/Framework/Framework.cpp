#include "Framework.h"
#include "Logger.h"
#include "SceneManager.h" 
#include "SrvManager.h"
#include "ImGuiManager.h"
#include "SoundManager.h"
#include "CameraManager.h"
#include "ParticleManager.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "TimeManager.h"
#include "SpriteCommon.h"
#include "Obj3DCommon.h"
#include "AbstractSceneFactory.h"

void Framework::Initialize(){
	// 1. 基盤の初期化（ハードウェア/OSとのやり取り）
	// クラッシュ時のダンプファイル出力設定
	SetUnhandledExceptionFilter(Logger::ExportDump);

	// WinAPIの初期化
	winApi_ = std::make_unique<WinAPI>();
	winApi_->Initialize(L"Andou_Kanade_就職作品",1280,720);

	// DirectXの初期化
	dxCommon_ = std::make_unique<DXCommon>();
	dxCommon_->Initialize(winApi_.get());

	// 入力システムの初期化
	input_ = std::make_unique<Input>();
	input_->Initialize(winApi_.get());

	// 2. ゲーム固有システムの初期化
	InitializeGameSystems();

	// 3. 描画共通設定の初期化
	spriteCommon_ = std::make_unique<SpriteCommon>();
	spriteCommon_->Initialize(dxCommon_.get());

	object3dCommon_ = std::make_unique<Obj3dCommon>();
	object3dCommon_->Initialize(dxCommon_.get());
}

void Framework::Update(){
	// --- ウィンドウメッセージ処理 ---
	// ウィンドウが閉じられたら終了リクエストを送る
	if(winApi_->ProcessMessage()){
		endRequest_ = true;
	}

	// --- 入力情報の更新 ---
	input_->Update();

	// --- ImGui 受付開始 ---
	// 注意: シーン更新前に Begin() を呼び出す必要がある
#ifdef _DEBUG
	ImGuiManager::GetInstance()->Begin();
#endif

	// --- シーン更新処理 ---
	SceneManager::GetInstance()->Update();

	// --- ImGui 受付終了 ---
#ifdef _DEBUG
	ImGuiManager::GetInstance()->End();
#endif

	// --- カメラの更新 ---
	CameraManager::GetInstance()->Update();
}

void Framework::Finalize(){
	// --- マネージャーの終了処理 ---
	// 依存関係を考慮して順次解放
	SceneManager::GetInstance()->Finalize();

#ifdef _DEBUG
	ImGuiManager::GetInstance()->Finalize();
#endif

	SoundManager::GetInstance()->Finalize();
	ParticleManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();
	TextureManager::GetInstance()->Finalize();
	CameraManager::GetInstance()->Finalize();
	SrvManager::GetInstance()->Finalize();

	// --- 基盤システムの終了 ---
	// unique_ptrにより各システムは自動的に解放される
}
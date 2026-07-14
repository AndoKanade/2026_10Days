#include "Framework.h"
#include "Logger.h"
#include "SceneManager.h" // シーンごとの更新・描画を管理

void Framework::Initialize(){
	// --- デバッグ設定 ---
	// 未処理例外発生時にダンプファイルを出力する
	SetUnhandledExceptionFilter(Logger::ExportDump);

	// --- 1. 基盤システムの初期化 ---
	// スマートポインタ(unique_ptr)で生成し、所有権をFrameworkが持つ
	winApi_ = std::make_unique<WinAPI>();
	winApi_->Initialize(L"Andou_Kanade_就職作品",1280,720);

	dxCommon_ = std::make_unique<DXCommon>();
	dxCommon_->Initialize(winApi_.get()); // 生ポインタが必要な場合は .get() で渡す

	input_ = std::make_unique<Input>();
	input_->Initialize(winApi_.get());

	// --- 2. 各種マネージャーの初期化 (シングルトン) ---
	// SRV (Shader Resource View) 管理
	SrvManager::GetInstance()->Initialize(dxCommon_.get());

#ifdef _DEBUG
	// ImGuiの初期化 (デバッグUI用)
	ImGuiManager::GetInstance()->Initialize(winApi_.get(),dxCommon_.get());
#endif

	// サウンド、テクスチャ、モデル、カメラ、パーティクルの管理初期化
	SoundManager::GetInstance()->Initialize();
	TextureManager::GetInstance()->Initialize(dxCommon_.get(),SrvManager::GetInstance());
	ModelManager::GetInstance()->Initialize(dxCommon_.get());
	CameraManager::GetInstance()->Initialize();
	ParticleManager::GetInstance()->Initialize(dxCommon_.get(),SrvManager::GetInstance());
	TimeManager::GetInstance()->Initialize();

	// --- 3. 描画共通リソースの初期化 ---
	// スプライト共通設定
	spriteCommon_ = std::make_unique<SpriteCommon>();
	spriteCommon_->Initialize(dxCommon_.get());

	// 3Dオブジェクト共通設定
	object3dCommon_ = std::make_unique<Obj3dCommon>();
	object3dCommon_->Initialize(dxCommon_.get());
}

void Framework::Update(){
	// --- ウィンドウメッセージ処理 ---
	// 「×」ボタンが押されたら終了フラグを立てる
	if(winApi_->ProcessMessage()){
		endRequest_ = true;
	}

	// --- 入力情報の更新 ---
	input_->Update();

	// --- ImGui 受付開始 ---
	// 注意: SceneManager::Update 内で ImGui を使用する場合があるため、
	// 必ずシーン更新の前に Begin() を呼ぶ必要がある
#ifdef _DEBUG
	ImGuiManager::GetInstance()->Begin();
#endif

	// --- シーン更新処理 ---
	// 現在のシーンのロジックを実行
	SceneManager::GetInstance()->Update();

	// --- ImGui 受付終了 ---
#ifdef _DEBUG
	ImGuiManager::GetInstance()->End();
#endif

	// --- カメラの更新 ---
	CameraManager::GetInstance()->Update();
}

void Framework::Finalize(){
	// --- シングルトンマネージャーの終了処理 ---
	// 依存関係やリソース解放順序を考慮して終了させる
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
	// winApi_, dxCommon_, input_, spriteCommon_ 等は
	// unique_ptr で管理されているため、デストラクタで自動的に解放されます。
	// 明示的な delete は不要です。
}
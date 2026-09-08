#include "TitleScene.h"

// エンジン/システム関連
#include "Input.h"
#include "ModelManager.h"
#include "SceneManager.h"
#include "Obj3D.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "Obj3dCommon.h"  
#include "DXCommon.h"     
#include "CameraManager.h" 
#include "Model.h"
#include "Application.h"
#include "SoundManager.h"
#include "Skybox.h"       // 追加：背景の天球
#include "SkyboxCommon.h" // 追加：天球の共通設定
#include <cmath>

// ImGui (マクロ定義がある場合のみ)
#ifdef USE_IMGUI
#include "imguiManager.h"
#endif

// 定数定義 (ファイルパスやパラメータ)
namespace{
	// 削除：仮置きだったFenceのモデルとuvCheckerのスプライト用の定数

	// 環境マップ（映り込み）に使うスカイボックスのテクスチャ。
	// 盤面のブロックの描画で参照するため、タイトルでも読み込んでおく必要がある。
	const std::string kSkyboxTexture = "resource/Skybox/rostock_laage_airport_4k.dds";

	// 追加：デモプレイの落下ブロックに使うモデル。
	// 読み込み自体は Board::Initialize() が済ませているため、ここではパスだけ持つ。
	const std::string kBlockModel = "blockBevel/blockBevel.obj";

	// 追加：デモプレイの操作間隔（フレーム）。
	// 回転・左右移動をこの間隔で1回ずつ行い、人が操作しているように見せる。
	constexpr int32_t kDemoActionIntervalFrames = 8;

	// 追加：デモプレイで1個のブロックにつき行う左右移動の最大回数。
	// 実際の回数はこの範囲から抽選する（負なら左、正なら右）。
	constexpr int32_t kDemoMaxHorizontalMoves = 5;

	// 追加：デモプレイの自動落下の間隔（フレーム）。
	// ゲーム中より速くして、タイトルでも展開が止まって見えないようにする。
	constexpr int32_t kDemoFallIntervalFrames = 20;

	// 追加：タイトル画面で流すBGMのパス
	const std::string kBgmPath = "resource/music/bgm/New_Breath.mp3";

	// 追加：タイトルBGMの初期音量
	constexpr float kBgmVolume = 0.5f;
}

// コンストラクタ
TitleScene::TitleScene() = default;

// デストラクタ
TitleScene::~TitleScene() = default;

// 初期化処理
void TitleScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	// メンバ変数の保持
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// --- リソースのロード ---
	TextureManager::GetInstance()->LoadTexture(kSkyboxTexture);

	auto* dxCommon = object3dCommon_->GetDxCommon();
	CameraManager::GetInstance()->CreateCamera("TitleCamera",dxCommon->GetDevice());

	// 2. 作ったカメラをアクティブにする
	CameraManager::GetInstance()->SetActiveCamera("TitleCamera");

	// 3. 必要なら座標を調整
	// 変更：背景の盤面がゲーム中とまったく同じ大きさ・位置で映るよう、
	// カメラの引き距離もゲームシーンと同じ値にする。
	auto* camera = CameraManager::GetInstance()->GetActiveCamera();
	camera->SetTranslate({0.0f, 0.0f, PuzzleConfig::kCameraDistanceZ});

	// 追加：Obj3Dは生成時に既定のカメラを取り込むため、盤面を作る前に差し替えておく
	object3dCommon_->SetDefaultCamera(camera);

	// 追加：ゲーム中と同じゲーミングな背景の天球を用意する。
	// 天球の共通設定（ルートシグネチャとPSO）はエンジン側で作られないため、シーンごとに作る。
	skyboxCommon_ = std::make_unique<SkyboxCommon>();
	skyboxCommon_->Initialize(object3dCommon_->GetDxCommon());
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(skyboxCommon_.get(),PuzzleConfig::kSkyboxBackgroundTexture);
	skyboxRotationY_ = 0.0f;
	skyboxPulseFrame_ = 0;
	// 1フレーム目の描画に間に合うよう、行列をここで一度作っておく
	skybox_->Update(*camera);

	// 追加：背景として描画する盤面を初期化する（壁だけの空の盤面）
	board_.Initialize(object3dCommon_);

	// 追加：デモプレイ用の乱数エンジンをシードし、最初のブロックを出現させる
	std::random_device seedGenerator;
	randomEngine_.seed(seedGenerator());
	SpawnDemoBlock();

	// 追加：落下中ブロックの描画オブジェクトを、ブロックのマス数だけ用意する。
	// 色は種類ごとに変わるため SyncDemoBlockObjs() で毎フレーム設定する。
	const size_t cellCount = fallingBlock_.GetOccupiedCells().size();
	for(size_t i = 0; i < cellCount; ++i){
		auto obj = std::make_unique<Obj3D>();
		obj->Initialize(object3dCommon_);
		obj->SetModel(kBlockModel);
		obj->SetScale({PuzzleConfig::kCellModelScale, PuzzleConfig::kCellModelScale, PuzzleConfig::kCellModelScale});

		if(Model::Material* material = obj->GetMaterial()){
			material->enableLighting = 1;
			material->shininess = PuzzleConfig::kBlockShininess;
			material->environmentCoefficient = PuzzleConfig::kBlockEnvironmentCoefficient;
		}

		fallingObjs_.push_back(std::move(obj));
	}
	SyncDemoBlockObjs();

	// 追加：タイトルBGMをロードしてループ再生する
	SoundManager::GetInstance()->SoundLoadFile(kBgmPath);
	SoundManager::GetInstance()->PlayAudio(kBgmPath,kBgmVolume,true);
}

// 終了処理
void TitleScene::Finalize(){
	// 追加：シーンを抜けるときにタイトルBGMを止める
	SoundManager::GetInstance()->StopAudio(kBgmPath);

	// そのほかは unique_ptr により自動解放されるため処理なし
}

// 更新処理
void TitleScene::Update(){

	// 1. ImGuiの設定更新
#ifdef USE_IMGUI
	// 削除：仮置きだったスプライトの色調整UI

	// 追加：音量調整UI
	SoundManager::GetInstance()->ShowVolumeGui();
#endif

	if(input_->TriggerKey(DIK_NUMPAD1) || input_->TriggerKey(DIK_1)){
		Application::GetInstance()->SetCurrentPPType(PostProcess::Type::PostProcess); // Default
	} else if(input_->TriggerKey(DIK_NUMPAD2) || input_->TriggerKey(DIK_2)){
		Application::GetInstance()->SetCurrentPPType(PostProcess::Type::BoxFilter);
	} else if(input_->TriggerKey(DIK_NUMPAD3) || input_->TriggerKey(DIK_3)){
		Application::GetInstance()->SetCurrentPPType(PostProcess::Type::Grayscale);
	} else if(input_->TriggerKey(DIK_NUMPAD4) || input_->TriggerKey(DIK_4)){
		Application::GetInstance()->SetCurrentPPType(PostProcess::Type::Vignette);
	} else if(input_->TriggerKey(DIK_NUMPAD5) || input_->TriggerKey(DIK_5)){
		Application::GetInstance()->SetCurrentPPType(PostProcess::Type::GaussianBlur);
	} else if(input_->TriggerKey(DIK_NUMPAD6) || input_->TriggerKey(DIK_6)){
		Application::GetInstance()->SetCurrentPPType(PostProcess::Type::LuminanceOutline);
	} else if(input_->TriggerKey(DIK_NUMPAD7) || input_->TriggerKey(DIK_7)){
		Application::GetInstance()->SetCurrentPPType(PostProcess::Type::DepthOutline);
	} else if(input_->TriggerKey(DIK_NUMPAD8) || input_->TriggerKey(DIK_8)){
		Application::GetInstance()->SetCurrentPPType(PostProcess::Type::RadialBlur);
	} else if(input_->TriggerKey(DIK_NUMPAD9) || input_->TriggerKey(DIK_9)){
		Application::GetInstance()->SetCurrentPPType(PostProcess::Type::Dissolve);
		// アニメーションのトリガーを引く
		Application::GetInstance()->StartDissolveAnimation();
	} else if(input_->TriggerKey(DIK_NUMPAD0) || input_->TriggerKey(DIK_0)){
		Application::GetInstance()->SetCurrentPPType(PostProcess::Type::Random);
	}else if(input_->TriggerKey(DIK_RETURN)){
		// 弱点を突いた瞬間をシミュレートしてグリッチを発動
		Application::GetInstance()->TriggerGlitch();
	}

	// 追加：背景の天球を進める（ゲーム中と同じ動かし方）。
	// ゆっくり回して虹色の帯を横へ流し、あわせて明るさをわずかに脈打たせる。
	if(skybox_){
		skyboxRotationY_ += PuzzleConfig::kSkyboxRotationPerFrame;
		if(skyboxRotationY_ >= 2.0f * PuzzleConfig::kPi){
			skyboxRotationY_ -= 2.0f * PuzzleConfig::kPi;
		}
		skybox_->SetRotationY(skyboxRotationY_);

		++skyboxPulseFrame_;
		if(skyboxPulseFrame_ >= PuzzleConfig::kSkyboxPulseCycleFrames){
			skyboxPulseFrame_ = 0;
		}
		const float phase = 2.0f * PuzzleConfig::kPi *
			static_cast<float>(skyboxPulseFrame_) / static_cast<float>(PuzzleConfig::kSkyboxPulseCycleFrames);
		const float brightness = 1.0f + PuzzleConfig::kSkyboxPulseAmplitude * std::sin(phase);
		skybox_->SetColor({brightness,brightness,brightness,1.0f});

		skybox_->Update(*CameraManager::GetInstance()->GetActiveCamera());
	}

	// 2. 背景のデモプレイの更新
	UpdateDemoPlay();

	// 3. 背景の盤面の更新（カメラに追従させるため行列を毎フレーム更新する）
	board_.Update();
	for(auto& obj : fallingObjs_){
		obj->Update();
	}

	// 4. シーン遷移 (スペースキー)
	if(input_->TriggerKey(DIK_SPACE)){
		SceneManager::GetInstance()->ChangeScene("GAME");
	}
}

// 描画処理
void TitleScene::Draw(){
	// 追加：背景の天球を最初に描く。
	// 天球は深度を書き込まないので、このあとに描く3Dオブジェクトがそのまま手前に重なる。
	if(skybox_){
		skybox_->Draw();
	}

	// 背景としてゲーム中と同じ盤面とデモプレイのブロックを描画する。
	// タイトル名などのUIは、このあとにスプライトで重ねて描画する。
	board_.Draw();
	for(auto& obj : fallingObjs_){
		obj->Draw();
	}
}

// 追加：背景のデモプレイを1フレーム進める。
void TitleScene::UpdateDemoPlay(){
	// 消去演出中はゲーム中と同じく操作も落下も止める（盤面の更新は呼び出し元で行う）
	if(board_.IsBusy()){
		return;
	}

	// 消去の結果はタイトルでは使わないため、溜め込まないよう毎フレーム捨てる
	board_.TakeClearResults();

	// 一定間隔で、出現時に決めておいた回転・左右移動を1つずつ実行する
	if(demoActionTimer_ > 0){
		--demoActionTimer_;
	} else if(demoPendingRotations_ > 0){
		fallingBlock_.Rotate(board_);
		--demoPendingRotations_;
		demoActionTimer_ = kDemoActionIntervalFrames;
	} else if(demoPendingMoves_ < 0){
		fallingBlock_.MoveLeft(board_);
		++demoPendingMoves_;
		demoActionTimer_ = kDemoActionIntervalFrames;
	} else if(demoPendingMoves_ > 0){
		fallingBlock_.MoveRight(board_);
		--demoPendingMoves_;
		demoActionTimer_ = kDemoActionIntervalFrames;
	} else{
		// 操作し終えたら、あとは加速して落とす（下キーを押しっぱなしにしたのと同じ扱い）
		fallingBlock_.SetSoftDrop(true);
	}

	// 時間経過を進め、盤面に固定されたら次のブロックを出す
	if(fallingBlock_.Update(board_,kDemoFallIntervalFrames)){
		// 天井より上にはみ出したまま固定された、または出現位置が塞がっていたら
		// ゲーム中はゲームオーバーになる状態。デモでは最初からやり直す
		const bool lockedAboveCeiling = fallingBlock_.IsLockedAboveCeiling();
		if(lockedAboveCeiling || !SpawnDemoBlock()){
			ResetDemoPlay();
		}
	}

	// 描画オブジェクトを現在の占有マスに合わせて動かす
	SyncDemoBlockObjs();
}

// 追加：デモ用のブロックを1個出現させ、そのブロックで行う操作内容を決める。
bool TitleScene::SpawnDemoBlock(){
	// 種類は全種類から等確率で選ぶ（背景の演出なので偏りは問題にならない）
	std::uniform_int_distribution<int32_t> typeDist(0,BlockShape::kTypeCount - 1);
	const BlockShape::Type type = static_cast<BlockShape::Type>(typeDist(randomEngine_));

	const bool spawned = fallingBlock_.Spawn(board_,type,nextBlockId_);
	++nextBlockId_;

	// 前のブロックの加速状態を持ち越さないよう戻す
	fallingBlock_.SetSoftDrop(false);

	// このブロックで行う回転回数と左右移動の回数を決める。
	// 壁や既存ブロックと重なる操作は FallingBlock 側が拒否するため、
	// ここでは置ける場所かどうかを気にせず抽選してよい。
	std::uniform_int_distribution<int32_t> rotationDist(0,BlockShape::kRotationCount - 1);
	std::uniform_int_distribution<int32_t> moveDist(-kDemoMaxHorizontalMoves,kDemoMaxHorizontalMoves);
	demoPendingRotations_ = rotationDist(randomEngine_);
	demoPendingMoves_ = moveDist(randomEngine_);
	demoActionTimer_ = kDemoActionIntervalFrames;

	return spawned;
}

// 追加：落下中ブロックの描画オブジェクトを、現在の占有マスと種類の色に合わせる。
void TitleScene::SyncDemoBlockObjs(){
	const Vector4 blockColor = PuzzleConfig::ApplyLitGain(PuzzleConfig::GetBlockColor(fallingBlock_.GetType()));

	const std::vector<GridPos> cells = fallingBlock_.GetOccupiedCells();
	for(size_t i = 0; i < fallingObjs_.size() && i < cells.size(); ++i){
		fallingObjs_[i]->SetTranslate(board_.GridToWorld(cells[i].x,cells[i].y));

		if(Model::Material* material = fallingObjs_[i]->GetMaterial()){
			material->color = blockColor;
		}
	}
}

// 追加：天井まで積み上がったときに、盤面を空に戻してデモを最初からやり直す。
void TitleScene::ResetDemoPlay(){
	board_.Reset();
	nextBlockId_ = 0;
	SpawnDemoBlock();
	SyncDemoBlockObjs();
}
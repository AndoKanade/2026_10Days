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

// 追加：タイトルロゴの浮遊アニメーションで sin を使うため
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

	// 追加：タイトルロゴのモデル（resource/ui/title/title.obj）
	const std::string kTitleModel = "ui/title/title.obj";

	// 追加：タイトルロゴの拡大率。
	// 元の形状は横幅1.6・縦幅0.46程度と小さいため、盤面（横幅10）に対して
	// 見出しとして目立つ大きさになるよう拡大する。
	constexpr float kTitleScale = 7.5f;

	// 追加：タイトルロゴの基準位置。
	// 盤面の上部に重なるよう盤面中心よりやや上（Y）に置き、
	// 手前（カメラ側、Zがより負の方向）にずらして落下ブロックより前面に表示する。
	const Vector3 kTitlePosition = {0.0f, 5.0f, -3.0f};

	// 追加：円周率×2。sin の位相計算（フレーム数→ラジアン）に使う。
	constexpr float kTitleFloatTwoPi = 3.14159265f * 2.0f;

	// 追加：上下にふわふわ浮くときの振れ幅（ワールド単位）
	constexpr float kTitleFloatAmplitude = 0.25f;

	// 追加：上下運動1往復にかけるフレーム数（150 = 約2.5秒。ゆっくり浮かせる）
	constexpr int32_t kTitleFloatPeriodFrames = 150;

	// 追加：左右にわずかに傾ける揺れの最大角度（ラジアン）
	// 度数法3度ぶん。傾けすぎるとロゴが読みにくくなるため控えめにする。
	constexpr float kTitleSwayAmplitudeRadians = 3.0f * (3.14159265f / 180.0f);

	// 追加：傾き1往復にかけるフレーム数。
	// 上下運動と同じ周期にすると単調に見えるため、あえてずらして自然な揺れにする。
	constexpr int32_t kTitleSwayPeriodFrames = 190;

	// --- 追加：難易度の選択UI ---

	// 見出しと各項目のラベル画像。ポーズ画面のラベルと同じ作り方で書き出してある。
	const std::string kDifficultyLabelTexture = "resource/ui/title/difficulty.png";
	const std::string kEasyTexture = "resource/ui/title/easy.png";
	const std::string kNormalTexture = "resource/ui/title/normal.png";
	const std::string kHardTexture = "resource/ui/title/hard.png";

	// 見出しと項目を並べる位置。盤面は画面の中央を占めるため、左の空きへ縦に並べる。
	constexpr Vector2 kDifficultyLabelPos = {56.0f,268.0f};
	constexpr float kDifficultyItemPosX = 76.0f;
	constexpr float kDifficultyItemTopPosY = 336.0f;
	constexpr float kDifficultyItemLineHeight = 56.0f;

	// 選択中の項目の色と、選択していない項目の色（ポーズ画面と同じ塗り分け）
	constexpr Vector4 kDifficultySelectedColor = {1.0f,1.0f,1.0f,1.0f};
	constexpr Vector4 kDifficultyUnselectedColor = {0.45f,0.45f,0.50f,1.0f};
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

	// 追加：盤面の上に重ねて表示するタイトルロゴを読み込む。
	// 位置・傾きは毎フレーム Update() 内でふわふわ動かすため、ここでは初期状態だけ設定する。
	ModelManager::GetInstance()->LoadModel(kTitleModel);
	titleObj_ = std::make_unique<Obj3D>();
	titleObj_->Initialize(object3dCommon_);
	titleObj_->SetModel(kTitleModel);
	titleObj_->SetScale({kTitleScale, kTitleScale, kTitleScale});
	titleObj_->SetTranslate(kTitlePosition);
	if(Model::Material* material = titleObj_->GetMaterial()){
		// ロゴなので陰影は付けず、常に同じ明るさで見えるようにする
		material->enableLighting = 0;
	}

	// 追加：難易度の選択UIを作る。
	// 前回選んだ値を SceneManager が保持しているため、その項目を選んだ状態で始める。
	TextureManager::GetInstance()->LoadTexture(kDifficultyLabelTexture);
	TextureManager::GetInstance()->LoadTexture(kEasyTexture);
	TextureManager::GetInstance()->LoadTexture(kNormalTexture);
	TextureManager::GetInstance()->LoadTexture(kHardTexture);

	// 画像の実寸のまま置く
	auto createLabel = [&](const std::string& texture,const Vector2& position){
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_,texture);
		sprite->SetPosition(position);
		return sprite;
	};

	difficultyLabel_ = createLabel(kDifficultyLabelTexture,kDifficultyLabelPos);

	// 並びは Difficulty の Easy / Normal / Hard に対応させる
	difficultySprites_.clear();
	const std::string difficultyTextures[] = {kEasyTexture, kNormalTexture, kHardTexture};
	for(int32_t i = 0; i < static_cast<int32_t>(Difficulty::Count); ++i){
		const float posY = kDifficultyItemTopPosY + kDifficultyItemLineHeight * static_cast<float>(i);
		difficultySprites_.push_back(createLabel(difficultyTextures[i],{kDifficultyItemPosX,posY}));
	}

	difficultyIndex_ = static_cast<int32_t>(SceneManager::GetInstance()->GetDifficulty());
	board_.SetDifficulty(SceneManager::GetInstance()->GetDifficulty());
	UpdateDifficultyUi();

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

	// 追加：タイトルロゴをふわふわ浮遊させる。
	// 上下移動と傾きで別々の周期の sin を使い、単純な往復には見えないようにする。
	++titleFloatTimer_;
	const float floatPhase = kTitleFloatTwoPi *
		static_cast<float>(titleFloatTimer_) / static_cast<float>(kTitleFloatPeriodFrames);
	const float swayPhase = kTitleFloatTwoPi *
		static_cast<float>(titleFloatTimer_) / static_cast<float>(kTitleSwayPeriodFrames);

	Vector3 titleTranslate = kTitlePosition;
	titleTranslate.y += std::sin(floatPhase) * kTitleFloatAmplitude;
	titleObj_->SetTranslate(titleTranslate);
	titleObj_->SetRotate({0.0f, 0.0f, std::sin(swayPhase) * kTitleSwayAmplitudeRadians});
	titleObj_->Update();

	// 追加：難易度の選択。上下（矢印キーまたはW/S）で移動し、端まで行ったら反対側へ回り込む。
	// 背景のデモプレイにも同じ難易度を反映して、消え方の違いをその場で見せる。
	{
		const int32_t itemCount = static_cast<int32_t>(Difficulty::Count);
		bool changed = false;

		if(input_->TriggerKey(DIK_UP) || input_->TriggerKey(DIK_W)){
			difficultyIndex_ = (difficultyIndex_ - 1 + itemCount) % itemCount;
			changed = true;
		}
		if(input_->TriggerKey(DIK_DOWN) || input_->TriggerKey(DIK_S)){
			difficultyIndex_ = (difficultyIndex_ + 1) % itemCount;
			changed = true;
		}

		if(changed){
			const Difficulty selected = static_cast<Difficulty>(difficultyIndex_);
			SceneManager::GetInstance()->SetDifficulty(selected);
			board_.SetDifficulty(selected);
		}
		UpdateDifficultyUi();
	}

	// 4. シーン遷移 (スペースキー)
	if(input_->TriggerKey(DIK_SPACE)){
		// 選択中の難易度を確定させてからゲームへ移る
		SceneManager::GetInstance()->SetDifficulty(static_cast<Difficulty>(difficultyIndex_));
		SceneManager::GetInstance()->ChangeScene("GAME");
	}
}

// 追加：選択状態に合わせて項目の色を塗り分け、行列を更新する。
void TitleScene::UpdateDifficultyUi(){
	if(difficultyLabel_){
		difficultyLabel_->Update();
	}

	// 選択中の項目だけ明るくして、いまどれを選んでいるか分かるようにする
	for(int32_t i = 0; i < static_cast<int32_t>(difficultySprites_.size()); ++i){
		difficultySprites_[i]->SetColor(i == difficultyIndex_
			? kDifficultySelectedColor
			: kDifficultyUnselectedColor);
		difficultySprites_[i]->Update();
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

	// 追加：タイトルロゴを最後に描画し、背景の盤面より手前に重ねて見せる
	titleObj_->Draw();

	// 追加：難易度の選択UIを、3D描画の後に通常UIとして重ねる
	if(spriteCommon_){
		spriteCommon_->Draw();
		if(difficultyLabel_){
			difficultyLabel_->Draw();
		}
		for(const auto& sprite : difficultySprites_){
			sprite->Draw();
		}
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
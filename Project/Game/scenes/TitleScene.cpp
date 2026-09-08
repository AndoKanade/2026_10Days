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
#include "SoundConfig.h"  // 追加：汎用SEのパスと再生窓口
#include "Skybox.h"       // 追加：背景の天球
#include "SkyboxCommon.h" // 追加：天球の共通設定
#include <cmath>
#include <filesystem>     // 追加：オプションのラベル画像が用意済みかを調べるため

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

	// 追加：カーソルがオプションへ移っている間、選んである難易度を示す色。
	// 白（カーソル位置）と灰（未選択）の中間にして、どれを選んであるかを見失わせない。
	constexpr Vector4 kDifficultyChosenColor = {0.75f,0.78f,0.85f,1.0f};

	// 追加：難易度の下に並べる「OPTION」のメニュー項目。
	// 難易度の並びの続きに置きつつ、別の項目だと分かるよう1行ぶんの余白を空ける。
	constexpr float kOptionMenuGapY = 32.0f;
	constexpr Vector2 kOptionMenuPlaceholderSize = {148.0f,40.0f};

	// --- 追加：オプション（音量設定）のUI ---

	// 見出しと各項目のラベル画像。難易度のラベルと同じ作り方で書き出す。
	const std::string kOptionHeaderTexture = "resource/ui/title/option.png";
	const std::string kOptionMasterTexture = "resource/ui/title/master.png";
	const std::string kOptionBgmTexture = "resource/ui/title/bgm.png";
	const std::string kOptionSeTexture = "resource/ui/title/se.png";

	// 暗幕と音量バーに使う白一色の画像（色はコード側で着ける）
	const std::string kOptionSolidTexture = "resource/ui/title/white.png";

	// 暗幕の大きさ（画面全体）と色（黒の半透明。後ろのタイトルがうっすら見える濃さ）
	constexpr Vector2 kOptionOverlaySize = {1280.0f,720.0f};
	constexpr Vector4 kOptionOverlayColor = {0.0f,0.0f,0.0f,0.72f};

	// 見出しの表示位置。
	// 変更：見出し画像 option.png は実寸 148x40 で用意したため、
	// 画面幅1280の中央に来るよう左端を 640 - 148/2 に置く。
	constexpr Vector2 kOptionHeaderPos = {566.0f,120.0f};

	// 項目のラベルを並べ始めるX座標・Y座標と、項目どうしの間隔
	constexpr float kOptionLabelPosX = 340.0f;
	constexpr float kOptionItemTopPosY = 280.0f;
	constexpr float kOptionItemLineHeight = 96.0f;

	// 音量バーの位置と大きさ。ラベルの右へ並べる。
	// ラベル画像の高さ40pxに対してバーの高さは28pxなので、
	// 縦の中心が揃うよう (40 - 28) / 2 = 6px だけ下げる。
	constexpr float kOptionBarPosX = 560.0f;
	constexpr float kOptionBarOffsetY = 6.0f;
	constexpr Vector2 kOptionBarSize = {400.0f,28.0f};

	// ラベル画像がまだ用意できていないときに置く仮ラベルの大きさ。
	// 変更：画像は用意済みのため通常は使われない。値は実際の画像の大きさに合わせてある。
	constexpr Vector2 kOptionHeaderPlaceholderSize = {148.0f,40.0f};
	constexpr Vector2 kOptionLabelPlaceholderSize = {160.0f,40.0f};

	// 音量バーの下地の色と、中身の色（選択中／非選択）
	constexpr Vector4 kOptionBarBackColor = {0.12f,0.14f,0.20f,0.9f};
	constexpr Vector4 kOptionBarSelectedColor = {0.15f,0.90f,1.0f,1.0f};
	constexpr Vector4 kOptionBarUnselectedColor = {0.30f,0.45f,0.55f,1.0f};

	// 選択中の項目の色と、選択していない項目の色（難易度と同じ塗り分け）
	constexpr Vector4 kOptionSelectedColor = {1.0f,1.0f,1.0f,1.0f};
	constexpr Vector4 kOptionUnselectedColor = {0.45f,0.45f,0.50f,1.0f};

	// 左右キー1回で動かす音量の量。0.0〜1.0を10段階で調整する。
	constexpr float kOptionVolumeStep = 0.1f;

	// 音量の下限と上限
	constexpr float kOptionVolumeMin = 0.0f;
	constexpr float kOptionVolumeMax = 1.0f;
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

	// 追加：カーソルは前回選んだ難易度の位置から始める
	menuIndex_ = difficultyIndex_;
	UpdateDifficultyUi();

	// 追加：オプション（音量設定）のUIを作る。
	// 開くまでは描画しないため、ここでは位置と大きさだけ決めておく。
	TextureManager::GetInstance()->LoadTexture(kOptionSolidTexture);

	// ラベル画像がまだ無い状態でも起動できるよう、
	// 画像があればそのまま実寸で置き、無ければ白一色の仮ラベルを同じ大きさで置く。
	// 画像を用意した時点で自動的に差し替わるため、ここを直す必要はない。
	auto createOptionLabel = [&](const std::string& texture,const Vector2& position,const Vector2& placeholderSize){
		if(std::filesystem::exists(texture)){
			TextureManager::GetInstance()->LoadTexture(texture);
			return createLabel(texture,position);
		}

		auto placeholder = createLabel(kOptionSolidTexture,position);
		placeholder->SetSize(placeholderSize);
		return placeholder;
	};

	// 追加：難易度の項目の下に「OPTION」のメニュー項目を置く。
	// 見出しと同じ option.png を使い、画像が無い間は仮ラベルで代用する。
	{
		const float optionMenuPosY = kDifficultyItemTopPosY
			+ kDifficultyItemLineHeight * static_cast<float>(static_cast<int32_t>(Difficulty::Count))
			+ kOptionMenuGapY;
		optionMenuItem_ = createOptionLabel(kOptionHeaderTexture,{kDifficultyItemPosX,optionMenuPosY},kOptionMenuPlaceholderSize);
	}

	optionOverlay_ = createLabel(kOptionSolidTexture,{0.0f,0.0f});
	optionOverlay_->SetSize(kOptionOverlaySize);
	optionOverlay_->SetColor(kOptionOverlayColor);

	optionHeader_ = createOptionLabel(kOptionHeaderTexture,kOptionHeaderPos,kOptionHeaderPlaceholderSize);

	// 並びは OptionItem の Master / Bgm / Se に対応させる
	optionLabels_.clear();
	optionBarBacks_.clear();
	optionBarFills_.clear();
	const std::string optionTextures[] = {kOptionMasterTexture, kOptionBgmTexture, kOptionSeTexture};
	for(int32_t i = 0; i < static_cast<int32_t>(OptionItem::Count); ++i){
		const float posY = kOptionItemTopPosY + kOptionItemLineHeight * static_cast<float>(i);
		optionLabels_.push_back(createOptionLabel(optionTextures[i],{kOptionLabelPosX,posY},kOptionLabelPlaceholderSize));

		// バーの下地は常に最大長。中身だけ音量に応じて縮める。
		auto back = createLabel(kOptionSolidTexture,{kOptionBarPosX,posY + kOptionBarOffsetY});
		back->SetSize(kOptionBarSize);
		back->SetColor(kOptionBarBackColor);
		optionBarBacks_.push_back(std::move(back));

		auto fill = createLabel(kOptionSolidTexture,{kOptionBarPosX,posY + kOptionBarOffsetY});
		fill->SetSize(kOptionBarSize);
		fill->SetColor(kOptionBarUnselectedColor);
		optionBarFills_.push_back(std::move(fill));
	}

	isOptionOpen_ = false;
	optionIndex_ = 0;
	UpdateOptionUi();

	// 追加：タイトルBGMをロードしてループ再生する
	SoundManager::GetInstance()->SoundLoadFile(kBgmPath,SoundCategory::BGM);
	SoundManager::GetInstance()->PlayAudio(kBgmPath,kBgmVolume,true);

	// 追加：全シーンで使う汎用SEをここでロードしておく
	SoundConfig::LoadCommonSe();
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

	// 追加：オプション（音量設定）の開閉と操作。
	// 開いている間はタイトル側の操作を止めるため、先に処理する。
	UpdateOptionInput();

	// 変更：難易度の3項目に「OPTION」を加えた縦メニューの選択。
	// 上下（矢印キーまたはW/S）で移動し、端まで行ったら反対側へ回り込む。
	// 難易度の項目に合わせたときは、その難易度を背景のデモプレイにも反映して
	// 消え方の違いをその場で見せる。
	// 変更：オプションを開いている間は受け付けない。
	if(!isOptionOpen_){
		// 難易度の項目数 + オプションの1項目
		const int32_t itemCount = static_cast<int32_t>(Difficulty::Count) + 1;
		bool moved = false;

		if(input_->TriggerKey(DIK_UP) || input_->TriggerKey(DIK_W)){
			menuIndex_ = (menuIndex_ - 1 + itemCount) % itemCount;
			moved = true;
		}
		if(input_->TriggerKey(DIK_DOWN) || input_->TriggerKey(DIK_S)){
			menuIndex_ = (menuIndex_ + 1) % itemCount;
			moved = true;
		}

		if(moved){
			// 難易度の項目に合わせている間だけ、選択中の難易度を差し替える。
			// オプションへ移っただけでは難易度は変わらない。
			if(!IsOptionMenuFocused()){
				difficultyIndex_ = menuIndex_;

				const Difficulty selected = static_cast<Difficulty>(difficultyIndex_);
				SceneManager::GetInstance()->SetDifficulty(selected);
				board_.SetDifficulty(selected);
			}

			// 追加：カーソル移動SE
			SoundConfig::PlayCursorMove();
		}
	}
	UpdateDifficultyUi();

	// 4. 決定 (スペースキー)
	// 変更：カーソルの位置によって、オプションを開くかゲームを始めるかを分ける。
	// オプションを開いている間はどちらも行わない。
	if(!isOptionOpen_ && input_->TriggerKey(DIK_SPACE)){
		// 追加：決定SE
		SoundConfig::PlayDecide();

		if(IsOptionMenuFocused()){
			// オプションを開く。開いた直後の1フレーム目から正しく描けるよう、
			// ここで音量設定のUIも更新しておく。
			isOptionOpen_ = true;
			optionIndex_ = 0;
			UpdateOptionUi();
		} else{
			// 選択中の難易度を確定させてからゲームへ移る
			SceneManager::GetInstance()->SetDifficulty(static_cast<Difficulty>(difficultyIndex_));
			SceneManager::GetInstance()->ChangeScene("GAME");
		}
	}
}

// 追加：カーソルがオプションの項目を指しているかを返す。
bool TitleScene::IsOptionMenuFocused() const{
	// 難易度の項目の次（末尾）がオプションの項目
	return menuIndex_ == static_cast<int32_t>(Difficulty::Count);
}

// 追加：オプションの開閉と、音量の増減操作を処理する。
void TitleScene::UpdateOptionInput(){
	// 変更：オプションはタイトルのメニューから開くようにしたため、
	// ここでは開いている間の操作（項目移動・音量調整・閉じる）だけを扱う。
	if(isOptionOpen_){
		const int32_t itemCount = static_cast<int32_t>(OptionItem::Count);

		// 上下で調整する項目を移動する（端まで行ったら反対側へ回り込む）
		if(input_->TriggerKey(DIK_UP) || input_->TriggerKey(DIK_W)){
			optionIndex_ = (optionIndex_ - 1 + itemCount) % itemCount;
			SoundConfig::PlayCursorMove();
		}
		if(input_->TriggerKey(DIK_DOWN) || input_->TriggerKey(DIK_S)){
			optionIndex_ = (optionIndex_ + 1) % itemCount;
			SoundConfig::PlayCursorMove();
		}

		// 左右で音量を増減する。上下限を超えないよう丸める。
		float volume = GetOptionVolume(optionIndex_);
		bool changed = false;

		if(input_->TriggerKey(DIK_LEFT) || input_->TriggerKey(DIK_A)){
			volume -= kOptionVolumeStep;
			changed = true;
		}
		if(input_->TriggerKey(DIK_RIGHT) || input_->TriggerKey(DIK_D)){
			volume += kOptionVolumeStep;
			changed = true;
		}

		if(changed){
			if(volume < kOptionVolumeMin){ volume = kOptionVolumeMin; }
			if(volume > kOptionVolumeMax){ volume = kOptionVolumeMax; }
			SetOptionVolume(optionIndex_,volume);

			// 調整した音量がその場で分かるよう、SEを鳴らして確認できるようにする
			SoundConfig::PlayCursorMove();
		}

		// 変更：ESCとTABのどちらでも閉じられるようにする。
		// 閉じるときに設定を保存し、次回起動へ引き継ぐ。
		if(input_->TriggerKey(DIK_ESCAPE) || input_->TriggerKey(DIK_TAB)){
			isOptionOpen_ = false;
			SoundManager::GetInstance()->SaveVolumeSettings();
			SoundConfig::PlayCancel();
		}
	}

	UpdateOptionUi();
}

// 追加：選択状態と現在の音量に合わせて、色とバーの長さを更新する。
void TitleScene::UpdateOptionUi(){
	// 閉じている間は描画しないため、行列の更新も不要
	if(!isOptionOpen_){
		return;
	}

	if(optionOverlay_){
		optionOverlay_->Update();
	}
	if(optionHeader_){
		optionHeader_->Update();
	}

	for(int32_t i = 0; i < static_cast<int32_t>(optionLabels_.size()); ++i){
		const bool selected = (i == optionIndex_);

		// 選択中の項目だけ明るくして、いまどれを調整しているか分かるようにする
		optionLabels_[i]->SetColor(selected ? kOptionSelectedColor : kOptionUnselectedColor);
		optionLabels_[i]->Update();

		optionBarBacks_[i]->Update();

		// バーの中身は音量の割合ぶんだけ横に伸ばす
		const float volume = GetOptionVolume(i);
		optionBarFills_[i]->SetSize({kOptionBarSize.x * volume, kOptionBarSize.y});
		optionBarFills_[i]->SetColor(selected ? kOptionBarSelectedColor : kOptionBarUnselectedColor);
		optionBarFills_[i]->Update();
	}
}

// 追加：指定した項目の音量を取得する。
float TitleScene::GetOptionVolume(int32_t index) const{
	SoundManager* soundManager = SoundManager::GetInstance();

	switch(static_cast<OptionItem>(index)){
	case OptionItem::Master:
		return soundManager->GetMasterVolume();
	case OptionItem::Bgm:
		return soundManager->GetCategoryVolume(SoundCategory::BGM);
	case OptionItem::Se:
		return soundManager->GetCategoryVolume(SoundCategory::SE);
	default:
		return kOptionVolumeMax;
	}
}

// 追加：指定した項目の音量を設定する。
void TitleScene::SetOptionVolume(int32_t index,float volume){
	SoundManager* soundManager = SoundManager::GetInstance();

	switch(static_cast<OptionItem>(index)){
	case OptionItem::Master:
		soundManager->SetMasterVolume(volume);
		break;
	case OptionItem::Bgm:
		soundManager->SetCategoryVolume(SoundCategory::BGM,volume);
		break;
	case OptionItem::Se:
		soundManager->SetCategoryVolume(SoundCategory::SE,volume);
		break;
	default:
		break;
	}
}

// 追加：選択状態に合わせて項目の色を塗り分け、行列を更新する。
void TitleScene::UpdateDifficultyUi(){
	if(difficultyLabel_){
		difficultyLabel_->Update();
	}

	// 変更：カーソル位置の項目を白く、カーソルが離れていても選んである難易度は
	// 中間色にして、いまどれを指していて、どれを選んであるかを分けて見せる。
	for(int32_t i = 0; i < static_cast<int32_t>(difficultySprites_.size()); ++i){
		Vector4 color = kDifficultyUnselectedColor;
		if(i == menuIndex_){
			color = kDifficultySelectedColor;
		} else if(i == difficultyIndex_){
			color = kDifficultyChosenColor;
		}

		difficultySprites_[i]->SetColor(color);
		difficultySprites_[i]->Update();
	}

	// 追加：オプションの項目も、カーソルが指しているときだけ明るくする
	if(optionMenuItem_){
		optionMenuItem_->SetColor(IsOptionMenuFocused()
			? kDifficultySelectedColor
			: kDifficultyUnselectedColor);
		optionMenuItem_->Update();
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

		// 追加：難易度の下に並べたオプションの項目
		if(optionMenuItem_){
			optionMenuItem_->Draw();
		}

		// 追加：オプションを開いている間だけ、暗幕と音量設定を最前面に重ねる
		if(isOptionOpen_){
			if(optionOverlay_){
				optionOverlay_->Draw();
			}
			if(optionHeader_){
				optionHeader_->Draw();
			}
			for(size_t i = 0; i < optionLabels_.size(); ++i){
				optionLabels_[i]->Draw();
				optionBarBacks_[i]->Draw();
				optionBarFills_[i]->Draw();
			}
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
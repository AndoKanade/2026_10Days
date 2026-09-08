#include "GameScene.h"
#include "CameraManager.h"
#include "ImGuiManager.h"
#include "ModelManager.h"
#include "SoundManager.h"
#include "Input.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "Application.h"
#include "Logger.h"
#include "LevelManager.h"
#include "SceneManager.h"
#include "WinAPI.h"
#include "Skybox.h"       // 追加：背景の天球
#include "SkyboxCommon.h" // 追加：天球の共通設定
#include <algorithm>
#include <cmath>

namespace{
	const std::string kGaugeBackgroundTexture = "resource/ui/specialGauge/red.png";
	const std::string kGaugeChargeTexture = "resource/ui/specialGauge/orange.png";
	const std::string kGaugeReadyTexture = "resource/ui/specialGauge/parple.png";
	const std::string kGaugeRainbowTexture = "resource/character/white.png";
	constexpr Vector2 kGaugePosition = {20.0f,20.0f};
	constexpr Vector2 kGaugeMaxSize = {360.0f,28.0f};
	const std::string kScoreNumberTexture = "resource/ui/score/numbers.png";

	// --- 追加：ポーズ画面 ---

	// 画面全体を暗くする暗幕に使う画像（白一色。色は下の定数で着ける）
	const std::string kPauseOverlayTexture = "resource/character/white.png";

	// ポーズ画面の見出しと各項目のラベル画像
	const std::string kPauseHeaderTexture = "resource/ui/pause/paused.png";
	const std::string kPauseRestartTexture = "resource/ui/pause/restart.png";
	const std::string kPauseTitleTexture = "resource/ui/pause/title.png";
	const std::string kPauseTutorialTexture = "resource/ui/pause/tutorial.png";

	// 追加：チュートリアルの内容を1枚にまとめた画像。
	// 画面と同じ 1280x720 で作ってあるので、画面全体へ引き伸ばさずそのまま貼る。
	const std::string kPauseTutorialSheetTexture = "resource/ui/pause/tutorialSheet.png";

	// 暗幕の色（黒の半透明。後ろのゲーム画面がうっすら見える濃さにする）
	constexpr Vector4 kPauseOverlayColor = {0.0f,0.0f,0.0f,0.65f};

	// 見出しを表示するY座標
	constexpr float kPauseHeaderPosY = 160.0f;

	// メニュー項目を並べ始めるY座標と、項目どうしの間隔
	constexpr float kPauseMenuTopPosY = 300.0f;
	constexpr float kPauseMenuLineHeight = 70.0f;

	// 選択中の項目の色と、選択していない項目の色
	constexpr Vector4 kPauseSelectedColor = {1.0f,1.0f,1.0f,1.0f};
	constexpr Vector4 kPauseUnselectedColor = {0.45f,0.45f,0.50f,1.0f};
	constexpr int32_t kScoreDigitCount = 8;
	constexpr Vector2 kScorePosition = {20.0f,60.0f};
	constexpr Vector2 kScoreDigitCellSize = {8.0f,12.0f};
	// 線形補間で隣の数字を拾わないよう、セル境界ではなく端のピクセル中心を参照する。
	constexpr Vector2 kScoreDigitSampleInset = {0.5f,0.5f};
	constexpr Vector2 kScoreDigitSampleSize = {7.0f,11.0f};
	constexpr Vector2 kScoreDigitDrawSize = {32.0f,48.0f};
	constexpr int32_t kRainbowFramesPerColor = 12;
	constexpr Vector4 kRainbowColors[] = {
		{1.0f,0.15f,0.15f,1.0f}, {1.0f,0.55f,0.10f,1.0f},
		{1.0f,0.95f,0.10f,1.0f}, {0.15f,1.0f,0.25f,1.0f},
		{0.10f,0.85f,1.0f,1.0f}, {0.20f,0.30f,1.0f,1.0f},
		{0.75f,0.20f,1.0f,1.0f}
	};
	Vector4 LerpColor(const Vector4& a,const Vector4& b,float t){
		return {a.x + (b.x-a.x)*t,a.y + (b.y-a.y)*t,
			a.z + (b.z-a.z)*t,a.w + (b.w-a.w)*t};
	}
	// 追加：ゲーム画面のBGMの初期音量
	constexpr float kBgmVolume = 0.5f;

	const std::string kLevelJsonFile = "level.json"; // レベル配置情報のJSONファイル名

	// 変更：落下ブロックの描画に使うモデル。盤面のマスと同じ面取りキューブを使う。
	const std::string kBlockModel = "blockBevel/blockBevel.obj";

	// 削除：落下中ブロック・ゴースト・プレビューの色は
	// PuzzleConfig::GetBlockColor() でブロックの種類ごとに引くようにした

	// 追加：ゴーストの拡大率（落下中ブロックのマス拡大率に対する倍率）
	constexpr float kGhostScaleRate = 0.7f;

	// ネクスト・ホールドのプレビューの拡大率（落下中ブロックのマス拡大率に対する倍率）
	constexpr float kPreviewScaleRate = 0.55f;

	// 追加：NEXT/HOLDの見出しラベルに使うモデル（Blenderで作成したテキスト形状のメッシュ）
	const std::string kNextLabelModel = "character/next.obj";
	const std::string kHoldLabelModel = "character/hold.obj";

	// 追加：ラベルの拡大率。文字メッシュは1マス幅程度の大きさなので、少し拡大して見やすくする。
	constexpr float kLabelScale = 1.3f;

	// 変更：ラベル下端の、プレビュー基準マス（row 0）からのワールド座標での高さ。
	// マス単位（整数行）だと1マス刻みでしか調整できず余白が広くなりすぎるため、
	// 「プレビューの形が最大で張り出す1マス上（row -1）のマス上端」+「わずかな余白」
	// をワールド単位で計算する。
	constexpr float kLabelClearanceMargin = 0.15f; // 追加の余白
	constexpr float kLabelWorldYOffset =
		1.0f * PuzzleConfig::kCellWorldSize +                      // row -1 の中心まで
		PuzzleConfig::kCellModelScale * kPreviewScaleRate +        // そのマスの半径ぶん（上端まで）
		kLabelClearanceMargin;

	// 追加：ネクストの枠と枠の間に、行の詰め幅（kNextPreviewRowSpan）だけでは
	// 足りない追加の余白（ワールド単位）。これが無いと、隣の枠との隙間が
	// 形の内部の隙間と同じ幅になり、3個の形が1本に繋がって見えてしまう。
	// 枠が進むごとに積み重なるので、n番目の枠はこの値のn倍だけ余分に下がる。
	constexpr float kNextPreviewExtraGapPerSlot = 0.6f;

	// 追加：ラベルの色（陰影なしの白っぽい色で光らせ、視認性を確保する）
	const Vector4 kLabelColor = {0.9f, 0.9f, 0.95f, 1.0f};

	// ブロックの種類名（ImGui表示用）
	const char* BlockTypeName(BlockShape::Type type){
		switch(type){
		case BlockShape::Type::T: return "T";
		case BlockShape::Type::L: return "L";
		case BlockShape::Type::I: return "I";
		case BlockShape::Type::J: return "J";
		case BlockShape::Type::S: return "S";
		case BlockShape::Type::Z: return "Z";
		}
		return "?";
	}

	// 変更：背景の天球の調整値は PuzzleConfig へ移した（タイトルと共通で使うため）

	// スペシャル選択カーソルの見た目
	constexpr float kSpecialCursorScale = 0.50f;
	const Vector4 kSpecialCursorValidColor = {0.25f, 1.0f, 0.45f, 0.75f};
	const Vector4 kSpecialCursorInvalidColor = {1.0f, 0.25f, 0.25f, 0.75f};
}

	// ImGuiに1個ぶんのブロックの形を、回転0の状態で簡易表示する。
	void ShowShapeGuiRows(BlockShape::Type type){
#ifdef USE_IMGUI
		const std::vector<GridPos>& shape = BlockShape::GetCells(type,0);
		if(shape.empty()){
			ImGui::Text("(no shape data)");
			return;
		}

		// 形が収まる範囲（外接矩形）を求める
		int32_t minX = shape.front().x;
		int32_t maxX = shape.front().x;
		int32_t minY = shape.front().y;
		int32_t maxY = shape.front().y;
		for(const GridPos& cell : shape){
			if(cell.x < minX){ minX = cell.x; }
			if(cell.x > maxX){ maxX = cell.x; }
			if(cell.y < minY){ minY = cell.y; }
			if(cell.y > maxY){ maxY = cell.y; }
		}

		// 範囲内を1行ずつ文字列にして表示する（マスあり = [] 、マスなし = 空白）
		for(int32_t y = minY; y <= maxY; ++y){
			std::string row;
			for(int32_t x = minX; x <= maxX; ++x){
				bool filled = false;
				for(const GridPos& cell : shape){
					if(cell.x == x && cell.y == y){
						filled = true;
						break;
					}
				}
				row += filled ? "[]" : "  ";
			}
			ImGui::Text("%s",row.c_str());
		}
#endif
	}


GameScene::GameScene() = default;
GameScene::~GameScene() = default;

// --- 初期化 ---
void GameScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;
	score_.Reset();
	SceneManager::GetInstance()->SetFinalScore(0);
	SceneManager::GetInstance()->SetFinalClearedCells(0);
	activePlayFrames_ = 0;
	debugManualFallSpeed_ = false;
	debugFallIntervalFrames_ = PuzzleConfig::kFallIntervalFrames;
	suppressSpecialClearCharge_ = false;

	// カメラの生成・設定
	CameraManager::GetInstance()->CreateCamera("default",object3dCommon_->GetDxCommon()->GetDevice());
	auto* defaultCamera = CameraManager::GetInstance()->GetCamera("default");
	defaultCamera->SetTranslate({0.0f, 0.0f, PuzzleConfig::kCameraDistanceZ});
	CameraManager::GetInstance()->SetActiveCamera("default");
	object3dCommon_->SetDefaultCamera(CameraManager::GetInstance()->GetActiveCamera());

	// 追加：ゲーミングな背景の天球を用意する。
	// 天球の共通設定（ルートシグネチャとPSO）はエンジン側で作られないため、シーンごとに作る。
	skyboxCommon_ = std::make_unique<SkyboxCommon>();
	skyboxCommon_->Initialize(object3dCommon_->GetDxCommon());
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(skyboxCommon_.get(),PuzzleConfig::kSkyboxBackgroundTexture);
	skyboxRotationY_ = 0.0f;
	skyboxPulseFrame_ = 0;
	// 1フレーム目の描画に間に合うよう、行列をここで一度作っておく
	skybox_->Update(*CameraManager::GetInstance()->GetActiveCamera());

	SoundManager::GetInstance()->SoundLoadFile(kBgmPath_);

	// 追加：ゲームBGMをループ再生する
	SoundManager::GetInstance()->PlayAudio(kBgmPath_,kBgmVolume,true);

	// 追加：パズルの盤面を初期化する（盤面は3Dオブジェクトで描画する）
	board_.Initialize(object3dCommon_);

	// 追加：次ブロック抽選用の乱数エンジンをシードする
	std::random_device seedGenerator;
	randomEngine_.seed(seedGenerator());

	// 追加：ネクストキューを満たしてから、最初の落下ブロックを出現させる
	FillNextQueue();
	if(!SpawnNextBlock()){
		isGameOver_ = true;
	}

	// 追加：落下中ブロックとゴースト（着地予測）の描画オブジェクトを、ブロックのマス数だけ用意する
	const size_t cellCount = fallingBlock_.GetOccupiedCells().size();
	for(size_t i = 0; i < cellCount; ++i){
		// 落下中ブロック本体
		auto obj = std::make_unique<Obj3D>();
		obj->Initialize(object3dCommon_);
		obj->SetModel(kBlockModel);
		obj->SetScale({PuzzleConfig::kCellModelScale, PuzzleConfig::kCellModelScale, PuzzleConfig::kCellModelScale});

		// 変更：面取り面が光を拾うようライティングを有効にし、光沢を乗せる。
		// 色はブロックの種類ごとに変わるため SyncFallingObjs() で毎フレーム設定する。
		if(Model::Material* material = obj->GetMaterial()){
			material->enableLighting = 1;
			material->shininess = PuzzleConfig::kBlockShininess;
			material->environmentCoefficient = PuzzleConfig::kBlockEnvironmentCoefficient;
		}
		fallingObjs_.push_back(std::move(obj));

		// ゴースト（着地予測）。本体より少し小さくして影のように見せる
		auto ghost = std::make_unique<Obj3D>();
		ghost->Initialize(object3dCommon_);
		ghost->SetModel(kBlockModel);
		ghost->SetScale({PuzzleConfig::kCellModelScale * kGhostScaleRate, PuzzleConfig::kCellModelScale * kGhostScaleRate, PuzzleConfig::kCellModelScale * kGhostScaleRate});

		// ゴーストは着地位置を示すための影なので、陰影を切って平らな暗い色にする。
		// 色はブロックの種類ごとに変わるため SyncFallingObjs() で毎フレーム設定する。
		if(Model::Material* material = ghost->GetMaterial()){
			material->enableLighting = 0;
		}
		ghostObjs_.push_back(std::move(ghost));
	}
	SyncFallingObjs();

	// 追加：ネクスト・ホールドのプレビューを初期状態で構築する
	RebuildPreviewObjs();

	// 追加：NEXT/HOLDの見出しラベルを生成する（位置はSyncPreviewLabelsで設定する）
	ModelManager::GetInstance()->LoadModel(kNextLabelModel);
	ModelManager::GetInstance()->LoadModel(kHoldLabelModel);

	nextLabelObj_ = std::make_unique<Obj3D>();
	nextLabelObj_->Initialize(object3dCommon_);
	nextLabelObj_->SetModel(kNextLabelModel);
	nextLabelObj_->SetScale({kLabelScale, kLabelScale, kLabelScale});
	if(Model::Material* material = nextLabelObj_->GetMaterial()){
		material->color = kLabelColor;
		material->enableLighting = 0; // 2D的な見た目にするため陰影を切る
	}

	holdLabelObj_ = std::make_unique<Obj3D>();
	holdLabelObj_->Initialize(object3dCommon_);
	holdLabelObj_->SetModel(kHoldLabelModel);
	holdLabelObj_->SetScale({kLabelScale, kLabelScale, kLabelScale});
	if(Model::Material* material = holdLabelObj_->GetMaterial()){
		material->color = kLabelColor;
		material->enableLighting = 0;
	}

	SyncPreviewLabels();

	// スペシャルの対象選択カーソルを用意する
	specialCursorObj_ = std::make_unique<Obj3D>();
	specialCursorObj_->Initialize(object3dCommon_);
	specialCursorObj_->SetModel(kBlockModel);
	specialCursorObj_->SetScale({kSpecialCursorScale,kSpecialCursorScale,kSpecialCursorScale});
	if(Model::Material* material = specialCursorObj_->GetMaterial()){
		material->color = kSpecialCursorValidColor;
		material->enableLighting = 0;
	}

	// 通常UIのスペシャルゲージを作る。白画像は発動中の七色着色専用。
	TextureManager::GetInstance()->LoadTexture(kGaugeBackgroundTexture);
	TextureManager::GetInstance()->LoadTexture(kGaugeChargeTexture);
	TextureManager::GetInstance()->LoadTexture(kGaugeReadyTexture);
	TextureManager::GetInstance()->LoadTexture(kGaugeRainbowTexture);
	auto createGaugeSprite = [&](const std::string& texture){
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_,texture);
		sprite->SetPosition(kGaugePosition);
		sprite->SetSize(kGaugeMaxSize);
		return sprite;
	};
	specialGaugeBackgroundSprite_ = createGaugeSprite(kGaugeBackgroundTexture);
	specialGaugeChargeSprite_ = createGaugeSprite(kGaugeChargeTexture);
	specialGaugeReadySprite_ = createGaugeSprite(kGaugeReadyTexture);
	specialGaugeActiveSprite_ = createGaugeSprite(kGaugeRainbowTexture);
	specialGaugeRainbowFrame_ = 0;
	UpdateSpecialGaugeUi();

	// 80x12の数字画像を8x12ずつ切り出し、8桁のスコアとして並べる。
	TextureManager::GetInstance()->LoadTexture(kScoreNumberTexture);
	scoreDigitSprites_.clear();
	for(int32_t i = 0; i < kScoreDigitCount; ++i){
		auto digit = std::make_unique<Sprite>();
		digit->Initialize(spriteCommon_,kScoreNumberTexture);
		digit->SetPosition({
			kScorePosition.x + kScoreDigitDrawSize.x * static_cast<float>(i),
			kScorePosition.y
		});
		digit->SetSize(kScoreDigitDrawSize);
		digit->SetTextureSize(kScoreDigitSampleSize);
		scoreDigitSprites_.push_back(std::move(digit));
	}
	UpdateScoreUi();

	// 追加：ポーズ画面のスプライトを用意する
	InitializePauseUi();

	// LevelManagerを初期化し、レベル配置オブジェクトを構築
	levelManager_.LoadJSON(kLevelJsonFile);
	RebuildLevelObjects();
}

// 追加：落下中ブロックとゴーストの描画オブジェクトの位置を、現在の占有マス／着地予測マスに合わせる
void GameScene::SyncFallingObjs(){
	// 追加：いま落ちているブロックの種類に対応する色。
	// ホールドやスポーンで種類が入れ替わるため、位置と一緒に毎フレーム反映する。
	const Vector4 baseColor = PuzzleConfig::GetBlockColor(fallingBlock_.GetType());

	// 本体はライティングありなので減衰ぶんを補正する。
	// ゴーストはライティングを切っているので補正せず、そのまま暗くする。
	const Vector4 blockColor = PuzzleConfig::ApplyLitGain(baseColor);
	const Vector4 ghostColor = PuzzleConfig::MakeDimColor(baseColor);

	// 落下中ブロック本体
	const std::vector<GridPos> cells = fallingBlock_.GetOccupiedCells();
	for(size_t i = 0; i < fallingObjs_.size() && i < cells.size(); ++i){
		fallingObjs_[i]->SetTranslate(board_.GridToWorld(cells[i].x,cells[i].y));

		if(Model::Material* material = fallingObjs_[i]->GetMaterial()){
			material->color = blockColor;
		}
	}

	// ゴースト（いま真下に落とした場合の着地位置）
	const std::vector<GridPos> ghostCells = fallingBlock_.GetLandingCells(board_);
	for(size_t i = 0; i < ghostObjs_.size() && i < ghostCells.size(); ++i){
		ghostObjs_[i]->SetTranslate(board_.GridToWorld(ghostCells[i].x,ghostCells[i].y));

		if(Model::Material* material = ghostObjs_[i]->GetMaterial()){
			material->color = ghostColor;
		}
	}
}

// 変更：ネクストキューの先頭のブロックを出現させ、キューの末尾に新しい種類を補充する。
bool GameScene::SpawnNextBlock(){
	// キューの先頭（いちばん次に来る種類）を実際に出現させる
	const BlockShape::Type typeToSpawn = nextQueue_.front();
	const bool spawned = fallingBlock_.Spawn(board_,typeToSpawn,nextBlockId_);

	// 次に出現するブロックへ振る元ブロックIDを1つ進める
	++nextBlockId_;

	// キューを1つ進め、末尾に新しい抽選結果を補充する（表示は常に規定個数を保つ）
	nextQueue_.erase(nextQueue_.begin());
	nextQueue_.push_back(PickNextBlockType());

	// 新しいブロックが出現したので、このブロックに対するホールドを再び使えるようにする
	canHold_ = true;

	// ネクストの中身が変わったのでプレビューを作り直す
	RebuildPreviewObjs();

	return spawned;
}

// 追加：ネクストキューを規定個数ぶん抽選して満たす。
void GameScene::FillNextQueue(){
	nextQueue_.clear();

	// 追加：前回の残りを持ち越さないよう、袋も空にしてから詰め直す
	blockBag_.clear();
	for(int32_t i = 0; i < PuzzleConfig::kNextQueueSize; ++i){
		nextQueue_.push_back(PickNextBlockType());
	}
}

// 追加：次に落ちてくるブロックの種類をひとつ抽選して返す。
BlockShape::Type GameScene::PickNextBlockType(){
	// 変更：毎回等確率で抽選すると同じ種類が続けて出る偏りが起きるため、
	// 全種類を1個ずつ入れた袋から1個ずつ取り出す方式にした。
	// 袋を使い切るまで同じ種類は2回出ず、空になったら詰め直す。
	if(blockBag_.empty()){
		RefillBlockBag();
	}

	// シャッフル済みなので、末尾から順に取り出すだけでよい
	const BlockShape::Type type = blockBag_.back();
	blockBag_.pop_back();
	return type;
}

// 追加：ブロックの袋に全種類を1個ずつ詰め直し、取り出す順番をシャッフルする。
void GameScene::RefillBlockBag(){
	blockBag_.clear();
	blockBag_.reserve(BlockShape::kTypeCount);

	// 種類が増えても直すのは BlockShape::kTypeCount だけで済むよう、
	// Type の並び順をそのまま袋の中身として使う
	for(int32_t i = 0; i < BlockShape::kTypeCount; ++i){
		blockBag_.push_back(static_cast<BlockShape::Type>(i));
	}

	std::shuffle(blockBag_.begin(),blockBag_.end(),randomEngine_);
}

// 追加：ホールド操作。今のブロックをホールドへ預け、代わりにホールド済みの
// ブロック（未ホールドならネクスト先頭のブロック）を出現させる。
bool GameScene::SwapHold(){
	// 1個のブロックにつきホールドは1回まで。既に使っていたら何もしない。
	if(!canHold_){
		return true;
	}

	// 今出現しているブロックの種類を控えておく（ホールドへ入れる分）
	const BlockShape::Type currentType = fallingBlock_.GetType();

	// 代わりに出現させる種類。既にホールド中ならその種類、まだなら
	// ネクストキューの先頭を使う（先頭を使った場合はキューを補充する）。
	BlockShape::Type typeToSpawn;
	if(hasHeldBlock_){
		typeToSpawn = holdType_;
	} else{
		typeToSpawn = nextQueue_.front();
		nextQueue_.erase(nextQueue_.begin());
		nextQueue_.push_back(PickNextBlockType());
	}

	holdType_ = currentType;
	hasHeldBlock_ = true;

	const bool spawned = fallingBlock_.Spawn(board_,typeToSpawn,nextBlockId_);
	++nextBlockId_;

	// 固定されるまで再びホールドは使えない
	canHold_ = false;

	// ネクスト・ホールドの中身が変わったのでプレビューを作り直す
	RebuildPreviewObjs();

	return spawned;
}

// 追加：ネクストキュー・ホールドのプレビュー用3Dオブジェクトを、現在の中身に合わせて作り直す。
void GameScene::RebuildPreviewObjs(){
	nextPreviewObjs_.clear();
	holdPreviewObjs_.clear();

	// ネクストキュー：右の壁の外側に、上から順に縦に並べる
	for(size_t i = 0; i < nextQueue_.size(); ++i){
		const int32_t anchorX = board_.GetWidth() + PuzzleConfig::kPreviewMarginCols;
		const int32_t anchorY = static_cast<int32_t>(i) * PuzzleConfig::kNextPreviewRowSpan;

		// 変更：プレビューもブロックの種類の色で表示する（何が来るか色でも分かるようにする）
		std::vector<std::unique_ptr<Obj3D>> slotObjs;
		const Vector4 nextColor = PuzzleConfig::ApplyLitGain(PuzzleConfig::GetBlockColor(nextQueue_[i]));
		// 枠ごとに追加の余白を積み重ね、隣の枠との境目が分かるようにする
		const float extraGapY = kNextPreviewExtraGapPerSlot * static_cast<float>(i);
		BuildPreviewShape(slotObjs,nextQueue_[i],anchorX,anchorY,nextColor,extraGapY);
		nextPreviewObjs_.push_back(std::move(slotObjs));
	}

	// ホールド：左の壁の外側に1枠だけ並べる（未ホールドの間は表示しない）
	if(hasHeldBlock_){
		const int32_t anchorX = -(1 + PuzzleConfig::kPreviewMarginCols + PuzzleConfig::kPreviewShapeMaxExtent);
		const int32_t anchorY = 0;
		// 変更：ホールドもブロックの種類の色で表示する。
		// このブロックで既にホールドを使い切っている間は、その色を暗くして再使用不可を示す。
		const Vector4 litColor = PuzzleConfig::ApplyLitGain(PuzzleConfig::GetBlockColor(holdType_));
		const Vector4 holdColor = canHold_ ? litColor : PuzzleConfig::MakeDimColor(litColor);
		BuildPreviewShape(holdPreviewObjs_,holdType_,anchorX,anchorY,holdColor);
	}

	// プレビューの配置（盤面幅）に合わせてラベルの位置も更新する
	SyncPreviewLabels();
}

// 追加：NEXT/HOLDラベルの位置を、現在の盤面幅に合わせたプレビューの配置に合わせて更新する。
void GameScene::SyncPreviewLabels(){
	if(!nextLabelObj_ || !holdLabelObj_){
		return;
	}

	// ネクストキュー先頭枠の基準マスの真上にラベルを置く
	const int32_t nextAnchorX = board_.GetWidth() + PuzzleConfig::kPreviewMarginCols;
	Vector3 nextLabelPos = board_.GridToWorld(nextAnchorX,0);
	nextLabelPos.y += kLabelWorldYOffset;
	nextLabelObj_->SetTranslate(nextLabelPos);

	// ホールド枠の基準マスの真上にラベルを置く
	const int32_t holdAnchorX = -(1 + PuzzleConfig::kPreviewMarginCols + PuzzleConfig::kPreviewShapeMaxExtent);
	Vector3 holdLabelPos = board_.GridToWorld(holdAnchorX,0);
	holdLabelPos.y += kLabelWorldYOffset;
	holdLabelObj_->SetTranslate(holdLabelPos);
}

// 追加：1個ぶんのブロックのプレビューを、指定した配列に構築する。
// extraYOffsetWorld は、マス目盛りでは表せない微調整用のワールド単位の下方向オフセット
// （下げる量。ネクストの枠を1個ずつさらに間隔を空けるのに使う）。
void GameScene::BuildPreviewShape(std::vector<std::unique_ptr<Obj3D>>& objs,BlockShape::Type type,int32_t anchorX,int32_t anchorY,const Vector4& color,float extraYOffsetWorld){
	// 回転0の形をそのままプレビューとして使う
	const std::vector<GridPos>& shape = BlockShape::GetCells(type,0);

	for(const GridPos& relative : shape){
		auto obj = std::make_unique<Obj3D>();
		obj->Initialize(object3dCommon_);
		obj->SetModel(kBlockModel);
		obj->SetScale({
			PuzzleConfig::kCellModelScale * kPreviewScaleRate,
			PuzzleConfig::kCellModelScale * kPreviewScaleRate,
			PuzzleConfig::kCellModelScale * kPreviewScaleRate
		});
		Vector3 pos = board_.GridToWorld(anchorX + relative.x,anchorY + relative.y);
		pos.y -= extraYOffsetWorld;
		obj->SetTranslate(pos);

		// 変更：盤面のブロックと同じ質感にそろえる
		if(Model::Material* material = obj->GetMaterial()){
			material->color = color;
			material->enableLighting = 1;
			material->shininess = PuzzleConfig::kBlockShininess;
			material->environmentCoefficient = PuzzleConfig::kBlockEnvironmentCoefficient;
		}

		objs.push_back(std::move(obj));
	}
}

// 変更：ImGui にネクストキュー・ホールドを表示する。
void GameScene::ShowNextBlockGui() const{
#ifdef USE_IMGUI
	if(!ImGui::CollapsingHeader("Next / Hold")){
		return;
	}

	// ネクストキューを先頭から順に表示する
	ImGui::Text("Next Queue");
	for(size_t i = 0; i < nextQueue_.size(); ++i){
		ImGui::Text("[%zu] Type: %s",i,BlockTypeName(nextQueue_[i]));
		ShowShapeGuiRows(nextQueue_[i]);
	}

	ImGui::Separator();

	// ホールドの中身とホールド可否を表示する
	ImGui::Text("Hold (%s)",canHold_ ? "usable" : "used");
	if(hasHeldBlock_){
		ImGui::Text("Type: %s",BlockTypeName(holdType_));
		ShowShapeGuiRows(holdType_);
	} else{
		ImGui::Text("(empty)");
	}
#endif
}

// レベル配置データからオブジェクトを再構築する処理
// Initialize()での初回構築と、ホットリロード時の再構築で呼ばれる
void GameScene::RebuildLevelObjects(){
	// 度数法からラジアンへの変換に使用する定数
	constexpr float kDegToRad = 3.14159265f / 180.0f;

	// 既存の配置オブジェクトを破棄してから作り直す
	levelObjects_.clear();

	for(const auto& objData : levelManager_.GetObjects()){
		if(objData.type == "MESH"){
			// 新しいObj3Dインスタンスを生成・初期化
			auto newObj = std::make_shared<Obj3D>();
			newObj->Initialize(object3dCommon_);

			// ファイル名(モデル名)が指定されていればモデルをセット
			if(!objData.fileName.empty()){
				ModelManager::GetInstance()->LoadModel(objData.fileName);
				newObj->SetModel(objData.fileName);
			}

			// トランスフォームの適用
			newObj->SetTranslate(objData.translation);
			newObj->SetScale(objData.scaling);

			// JSONに保存されている回転角(度数法)をラジアンに変換
			float radX = objData.rotation.x * kDegToRad;
			float radY = objData.rotation.y * kDegToRad;
			float radZ = objData.rotation.z * kDegToRad;

			// オイラー角からクォータニオンに変換してセット
			newObj->SetQuaternion(MakeQuaternionFromEuler(radX,radY,radZ));

			// TODO: コライダー(Obj3D::SetCollider)が実装され次第、
			// objData.colliderType を見てここで初期化する

			// 管理用配列に追加
			levelObjects_.push_back(newObj);
		}
	}
}

void GameScene::Finalize(){
	// 追加：シーンを抜けるときにゲームBGMを止める
	SoundManager::GetInstance()->StopAudio(kBgmPath_);
}

// --- 更新処理 ---
void GameScene::Update() {
	// 追加：TABでポーズ画面を開閉する。
	// チュートリアルを表示中はメニューへ戻る一段階として使う。
	// ゲームオーバー時はリザルトへ遷移済みのため受け付けない。
	if(!isGameOver_ && input_->TriggerKey(DIK_TAB)){
		if(isPaused_ && pauseMode_ == PauseMode::Tutorial){
			pauseMode_ = PauseMode::Menu;
		} else{
			isPaused_ = !isPaused_;
			if(isPaused_){
				// 開くたびに先頭の項目から選び直す
				pauseMode_ = PauseMode::Menu;
				pauseMenuIndex_ = 0;
			}
		}
		UpdatePauseUi();
	}

	// 追加：ポーズ中はゲームの処理を一切進めず、メニューの操作だけを受け付ける
	if(isPaused_){
		UpdatePauseMenu();
		return;
	}

	// 追加：背景の天球を進める。
	// ゆっくり回して虹色の帯を横へ流し、あわせて明るさをわずかに脈打たせる。
	// ポーズ中はここまで来ないので、背景も一緒に止まる。
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

	// レベル配置JSONのホットリロード確認
	// ファイルが更新されていた場合、自動で再読み込みしてlevelObjects_を作り直す
	if (levelManager_.CheckAndReload()) {
		RebuildLevelObjects();
	}

	// 追加：盤面の更新
	board_.Update();
	for (const Board::ClearResult& result : board_.TakeClearResults()) {
		if(!suppressSpecialClearCharge_){
			specialGauge_.AddFromClear(result.cellCount, result.chainCount);
		}
		score_.AddFromClear(result.cellCount,result.chainCount);
	}
	// スペシャルから始まった消去と、その落下連鎖がすべて終わってから通常チャージへ戻す。
	if(suppressSpecialClearCharge_ && !board_.IsBusy()){
		suppressSpecialClearCharge_ = false;
	}

	// スペシャル発動後は、対象選択中もゲージを減少させる
	if (!isGameOver_) {
		specialGauge_.Update();
	}
	if (specialSelector_.IsSelecting() && !specialGauge_.IsActivationActive()) {
		// 制限時間内に決定できなかったため、選択を終了する
		specialSelector_.Cancel();
	}

	// Qキーでスペシャルの対象選択を開始する
	if (!isGameOver_ && !board_.IsBusy() && !specialSelector_.IsSelecting() &&
		specialGauge_.CanActivate() && input_->TriggerKey(DIK_Q)) {
		if (specialSelector_.Begin(board_)) {
			specialGauge_.StartActivation();
		}
	}

	// 対象選択中は落下処理を止め、カーソル操作だけを受け付ける
	if (specialSelector_.IsSelecting()) {
		fallingBlock_.SetSoftDrop(false);

		if (input_->TriggerKey(DIK_LEFT)) {
			specialSelector_.Move(-1, 0, board_);
		}
		if (input_->TriggerKey(DIK_RIGHT)) {
			specialSelector_.Move(1, 0, board_);
		}
		if (input_->TriggerKey(DIK_UP)) {
			specialSelector_.Move(0, -1, board_);
		}
		if (input_->TriggerKey(DIK_DOWN)) {
			specialSelector_.Move(0, 1, board_);
		}
		if (input_->TriggerKey(DIK_RETURN)) {
			ConfirmSpecialTarget();
		}
		SyncSpecialCursor();
	} else if (!isGameOver_) {
			// 追加：消去演出中（Board::IsBusy）はブロックの操作・落下・出現を止める
			if (!board_.IsBusy()) {
				// 手動調整中は自動加速の時計を止める。
				if(!debugManualFallSpeed_){ ++activePlayFrames_; }
				// 左右移動・回転はトリガー（押した瞬間）で1回ずつ
				if (input_->TriggerKey(DIK_A)) {
					fallingBlock_.MoveLeft(board_);
				}
				if (input_->TriggerKey(DIK_D)) {
					fallingBlock_.MoveRight(board_);
				}
				if (input_->TriggerKey(DIK_W)) {
					fallingBlock_.Rotate(board_);
				}
				// 追加：ホールド操作（1個のブロックにつき1回まで）
				if (input_->TriggerKey(DIK_C) && canHold_) {
					if (!SwapHold()) {
						// 差し替えたブロックの出現位置が塞がっていた＝ゲームオーバー
						isGameOver_ = true;
						SceneManager::GetInstance()->SetFinalScore(score_.GetTotal());
						SceneManager::GetInstance()->SetFinalClearedCells(score_.GetTotalCells());
						SceneManager::GetInstance()->ChangeScene("GAMEOVER");
						return;
					}
				}
				// 下キーは押しっぱなしで加速落下
				fallingBlock_.SetSoftDrop(input_->PushKey(DIK_S));

				// 時間経過を進め、盤面に固定されたら次のブロックを出す
				const bool blockLocked = input_->TriggerKey(DIK_RETURN)
					? fallingBlock_.HardDrop(board_)
					: fallingBlock_.Update(board_,GetCurrentFallInterval());

				if (blockLocked) {
					// 天井より上にはみ出したまま固定された ＝ 積み上がりすぎでゲームオーバー
					const bool lockedAboveCeiling = fallingBlock_.IsLockedAboveCeiling();

					// 次のブロック（ネクスト）を出現させる。出現位置が塞がっていても同様にゲームオーバー
					const bool spawned = SpawnNextBlock();

					if (lockedAboveCeiling || !spawned) {
						isGameOver_ = true;
						SceneManager::GetInstance()->SetFinalScore(score_.GetTotal());
						SceneManager::GetInstance()->SetFinalClearedCells(score_.GetTotalCells());
						SceneManager::GetInstance()->ChangeScene("GAMEOVER");
						return;
					}
				}

				// 描画オブジェクトを現在の占有マス／着地予測マスに合わせて動かす
				SyncFallingObjs();
			}

			for (auto& obj : fallingObjs_) {
				obj->Update();
			}
			for (auto& obj : ghostObjs_) {
				obj->Update();
			}
			// 追加：ネクスト・ホールドのプレビューも毎フレーム行列を更新する
			for (auto& slot : nextPreviewObjs_) {
				for (auto& obj : slot) {
					obj->Update();
				}
			}
			for (auto& obj : holdPreviewObjs_) {
				obj->Update();
			}
			// 追加：NEXT/HOLDラベルも毎フレーム行列を更新する
			if(nextLabelObj_){
				nextLabelObj_->Update();
			}
			if(holdLabelObj_){
				holdLabelObj_->Update();
			}
		}

		for (auto& obj : levelObjects_) {
			obj->Update();
		}

		// --- デバッグUIの表示 ---
#ifdef USE_IMGUI
		if (Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()) {
			// メインデバッグウィンドウ
			ImGui::Begin("GameScene Debug");
			ImGui::Text("Score: %lld",static_cast<long long>(score_.GetTotal()));
			ImGui::Text("Total cleared cells: %lld",static_cast<long long>(score_.GetTotalCells()));
			ImGui::Text("Last gain: +%lld",static_cast<long long>(score_.GetLastGain()));
			ImGui::Text("Cells: %d (x%.1f) / Combo: %d (x%.1f)",
				score_.GetLastCells(),ScoreSystem::CellMultiplierTenths(score_.GetLastCells()) / 10.0,
				score_.GetLastChain(),ScoreSystem::ChainMultiplierTenths(score_.GetLastChain()) / 10.0);

			if(ImGui::CollapsingHeader("Fall Speed")){
				if(ImGui::Checkbox("Manual fall speed",&debugManualFallSpeed_) && debugManualFallSpeed_){
					debugFallIntervalFrames_ = PuzzleConfig::GetFallIntervalFrames(activePlayFrames_);
				}
				if(debugManualFallSpeed_){
					ImGui::SliderInt("Frames per cell",&debugFallIntervalFrames_,1,180);
					// キーボードで範囲外の値を入力しても安全な範囲へ戻す。
					if(debugFallIntervalFrames_ < 1){ debugFallIntervalFrames_ = 1; }
					if(debugFallIntervalFrames_ > 180){ debugFallIntervalFrames_ = 180; }
					ImGui::Text("Lower = faster. 60 frames = 1 second.");
					ImGui::Text("Auto speed timer is paused while manual mode is on.");
				}
				if(ImGui::Button("Reset to initial auto speed")){
					debugManualFallSpeed_ = false;
					debugFallIntervalFrames_ = PuzzleConfig::kFallIntervalFrames;
					activePlayFrames_ = 0;
				}
				ImGui::Text("Active time: %.1f sec",static_cast<double>(activePlayFrames_) / PuzzleConfig::kFrameRate);
				ImGui::Text("Fall interval: %.3f sec / cell",
					GetCurrentFallInterval() / PuzzleConfig::kFrameRate);
			}

			// 追加：次に落ちてくるブロック（ネクスト）の表示
			ShowNextBlockGui();

			// カメラ設定
			if (ImGui::CollapsingHeader("Camera Settings")) {
				Vector3 camPos = activeCamera->GetTranslate();
				if (ImGui::DragFloat3("Camera Pos", &camPos.x, 0.1f)) {
					activeCamera->SetTranslate(camPos);
				}

				Vector3 camRot = activeCamera->GetRotate();
				if (ImGui::DragFloat3("Camera Rotate", &camRot.x, 0.01f)) {
					activeCamera->SetRotate(camRot);
				}
			}

			// ライティング設定
			if (ImGui::CollapsingHeader("Lighting")) {
				if (PointLight* pData = object3dCommon_->GetPointLightData()) {
					ImGui::Text("Point Light");
					ImGui::ColorEdit4("Point Color", &pData->color.x);
					ImGui::DragFloat3("Point Pos", &pData->position.x, 0.1f);
					ImGui::DragFloat("Point Intensity", &pData->intensity, 0.1f, 0.0f, 100.0f);
				}
				if (SpotLight* sData = object3dCommon_->GetSpotLightData()) {
					ImGui::Text("Spot Light");
					ImGui::ColorEdit4("Spot Color", &sData->color.x);
				}
			}

			// レベル配置(JSON)の描画切り替え・手動ホットリロード
			if (ImGui::CollapsingHeader("Level Objects")) {
				// 読み込んだ配置オブジェクトを描画するかどうかの切り替え
				ImGui::Checkbox("Draw Level Objects", &isLevelObjectsVisible_);

				// ボタン押下でJSONファイルを強制的に再読み込みする
				if (ImGui::Button("Reload Level JSON")) {
					levelManager_.LoadJSON(kLevelJsonFile);
					RebuildLevelObjects();
				}
			}

			// スペシャルゲージの加算・消費を単独で確認する
			if (ImGui::CollapsingHeader("Special Gauge")) {
				ImGui::Text("Gauge: %d / %d", specialGauge_.GetValue(), specialGauge_.GetMaxValue());
				ImGui::ProgressBar(specialGauge_.GetRatio(), ImVec2(-1.0f, 0.0f));
				const char* specialStatus = specialGauge_.IsActivationActive() ? "ACTIVE" :
					(specialGauge_.CanActivate() ? "READY" : "CHARGING");
				ImGui::Text("Status: %s", specialStatus);
				ImGui::Text("Active Limit: %.1f seconds",
					static_cast<float>(PuzzleConfig::kSpecialReadyDurationFrames) / PuzzleConfig::kFrameRate);

				ImGui::InputInt("Cleared Cells", &debugClearedCellCount_);
				ImGui::InputInt("Chain Count", &debugChainCount_);

				if (ImGui::Button("Apply Clear Result")) {
					specialGauge_.AddFromClear(debugClearedCellCount_, debugChainCount_);
				}

				if (ImGui::Button("Fill Gauge")) {
					specialGauge_.Fill();
				}
				ImGui::SameLine();
				if (ImGui::Button("Complete Use (Debug)")) {
					if (specialGauge_.Consume()) {
						specialSelector_.Cancel();
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Reset Gauge")) {
					specialGauge_.Reset();
					specialSelector_.Cancel();
				}

				if (!specialSelector_.IsSelecting()) {
					if (ImGui::Button("Start Selection")) {
						if (!isGameOver_ && !board_.IsBusy() && specialGauge_.CanActivate()) {
							if (specialSelector_.Begin(board_)) {
								specialGauge_.StartActivation();
							}
						}
					}
				} else {
					const GridPos target = specialSelector_.GetTarget();
					ImGui::Text("Target: (%d, %d)", target.x, target.y);
					ImGui::Text("Target Cell: %s", specialSelector_.CanConfirm(board_) ? "VALID" : "INVALID");
					ImGui::Text("Arrow Keys: Move / Enter: Confirm");
					if (ImGui::Button("Confirm Target")) {
						ConfirmSpecialTarget();
					}
					if (ImGui::Button("Force Cancel (Debug)")) {
						specialSelector_.Cancel();
						specialGauge_.Reset();
					}
				}
			}

			ModelManager::GetInstance()->UpdateLightGui();
			ImGui::End();

			// 追加：音量調整UI
			SoundManager::GetInstance()->ShowVolumeGui();

			Application::GetInstance()->ShowPostProcessUI();
		}
#endif
	UpdateSpecialGaugeUi();
	UpdateScoreUi();
}

// --- 描画処理 ---
void GameScene::Draw(){
	// 追加：背景の天球を最初に描く。
	// 天球は深度を書き込まないので、このあとに描く3Dオブジェクトがそのまま手前に重なる。
	if(skybox_){
		skybox_->Draw();
	}

	object3dCommon_->Draw();

	// 配置オブジェクトの描画
	if(isLevelObjectsVisible_){
		for(const auto& obj : levelObjects_){
			obj->Draw();
		}
	}

	// 追加：パズルの盤面（壁とマス）を描画する
	board_.Draw();

	// 追加：着地予測（ゴースト）→ 落下中のブロックの順に描画する
	if(!isGameOver_){
		for(const auto& obj : ghostObjs_){
			obj->Draw();
		}
		for(const auto& obj : fallingObjs_){
			obj->Draw();
		}
	}

	// ネクスト・ホールドはスペシャル使用時だけでなく毎フレーム描画する。
	for(const auto& slot : nextPreviewObjs_){
		for(const auto& obj : slot){
			obj->Draw();
		}
	}
	for(const auto& obj : holdPreviewObjs_){
		obj->Draw();
	}
	// 追加：NEXT/HOLDラベルを描画する
	if(nextLabelObj_){
		nextLabelObj_->Draw();
	}
	if(holdLabelObj_){
		holdLabelObj_->Draw();
	}

	// スペシャルの対象選択カーソルを最後に重ねて描画する
	if(specialSelector_.IsSelecting() && specialCursorObj_){
		specialCursorObj_->Draw();
	}

	// 3D描画の後に通常UIとして重ねる。Releaseでも表示される。
	if(spriteCommon_ && specialGaugeBackgroundSprite_){
		spriteCommon_->Draw();
		specialGaugeBackgroundSprite_->Draw();
		if(specialGauge_.IsActivationActive()){
			specialGaugeActiveSprite_->Draw();
		}else if(specialGauge_.CanActivate()){
			specialGaugeReadySprite_->Draw();
		}else if(specialGauge_.GetValue() > 0){
			specialGaugeChargeSprite_->Draw();
		}
		for(const auto& digit : scoreDigitSprites_){
			digit->Draw();
		}
	}

	// 追加：ポーズ画面はすべての描画の手前に重ねる
	DrawPause();
}

// 追加：ポーズ画面のスプライトを生成して初期配置する。
void GameScene::InitializePauseUi(){
	TextureManager::GetInstance()->LoadTexture(kPauseOverlayTexture);
	TextureManager::GetInstance()->LoadTexture(kPauseHeaderTexture);
	TextureManager::GetInstance()->LoadTexture(kPauseRestartTexture);
	TextureManager::GetInstance()->LoadTexture(kPauseTitleTexture);
	TextureManager::GetInstance()->LoadTexture(kPauseTutorialTexture);
	TextureManager::GetInstance()->LoadTexture(kPauseTutorialSheetTexture); // 追加

	// 画面全体を覆う暗幕
	pauseOverlaySprite_ = std::make_unique<Sprite>();
	pauseOverlaySprite_->Initialize(spriteCommon_,kPauseOverlayTexture);
	pauseOverlaySprite_->SetPosition({0.0f,0.0f});
	pauseOverlaySprite_->SetSize({
		static_cast<float>(WinAPI::kClientWidth),
		static_cast<float>(WinAPI::kClientHeight)
	});
	pauseOverlaySprite_->SetColor(kPauseOverlayColor);

	// 画像の実寸のまま、画面の中央にそろえて置く
	auto createCenteredSprite = [&](const std::string& texture,float posY){
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_,texture);
		const float posX = (static_cast<float>(WinAPI::kClientWidth) - sprite->GetSize().x) * 0.5f;
		sprite->SetPosition({posX,posY});
		return sprite;
	};

	pauseHeaderSprite_ = createCenteredSprite(kPauseHeaderTexture,kPauseHeaderPosY);

	// メニュー項目。並びは PauseMenuItem の順に対応させる
	pauseMenuSprites_.clear();
	const std::string menuTextures[] = {
		kPauseRestartTexture,
		kPauseTitleTexture,
		kPauseTutorialTexture,
	};
	for(int32_t i = 0; i < static_cast<int32_t>(PauseMenuItem::Count); ++i){
		const float posY = kPauseMenuTopPosY + kPauseMenuLineHeight * static_cast<float>(i);
		pauseMenuSprites_.push_back(createCenteredSprite(menuTextures[i],posY));
	}

	// 変更：チュートリアル画面は見出しではなく、内容をまとめた1枚絵をそのまま表示する。
	// 画像は画面と同じ大きさで作ってあるため、左上に置いて画面いっぱいに広げる。
	pauseTutorialSheetSprite_ = std::make_unique<Sprite>();
	pauseTutorialSheetSprite_->Initialize(spriteCommon_,kPauseTutorialSheetTexture);
	pauseTutorialSheetSprite_->SetPosition({0.0f,0.0f});
	pauseTutorialSheetSprite_->SetSize({
		static_cast<float>(WinAPI::kClientWidth),
		static_cast<float>(WinAPI::kClientHeight)
	});

	UpdatePauseUi();
}

// 追加：ポーズ中の入力を受け付け、UIを更新する。
void GameScene::UpdatePauseMenu(){
	if(pauseMode_ == PauseMode::Menu){
		const int32_t itemCount = static_cast<int32_t>(PauseMenuItem::Count);

		// 上下で選択を移動する（端まで行ったら反対側へ回り込む）
		if(input_->TriggerKey(DIK_UP) || input_->TriggerKey(DIK_W)){
			pauseMenuIndex_ = (pauseMenuIndex_ - 1 + itemCount) % itemCount;
		}
		if(input_->TriggerKey(DIK_DOWN) || input_->TriggerKey(DIK_S)){
			pauseMenuIndex_ = (pauseMenuIndex_ + 1) % itemCount;
		}

		// 決定
		if(input_->TriggerKey(DIK_RETURN) || input_->TriggerKey(DIK_SPACE)){
			ConfirmPauseMenuItem();
		}
	} else{
		// チュートリアル表示中は、決定でメニューへ戻る（TABでも戻れる）
		if(input_->TriggerKey(DIK_RETURN) || input_->TriggerKey(DIK_SPACE)){
			pauseMode_ = PauseMode::Menu;
		}
	}

	UpdatePauseUi();
}

// 追加：ポーズ画面のスプライトを、いまの選択状態に合わせて更新する。
void GameScene::UpdatePauseUi(){
	if(pauseOverlaySprite_){
		pauseOverlaySprite_->Update();
	}
	if(pauseHeaderSprite_){
		pauseHeaderSprite_->Update();
	}
	// 変更：チュートリアルの1枚絵も毎フレーム行列を更新する
	if(pauseTutorialSheetSprite_){
		pauseTutorialSheetSprite_->Update();
	}

	// 選択中の項目だけ明るくして、いまどれを選んでいるか分かるようにする
	for(int32_t i = 0; i < static_cast<int32_t>(pauseMenuSprites_.size()); ++i){
		pauseMenuSprites_[i]->SetColor(i == pauseMenuIndex_ ? kPauseSelectedColor : kPauseUnselectedColor);
		pauseMenuSprites_[i]->Update();
	}
}

// 追加：選択中の項目を決定したときの処理。
void GameScene::ConfirmPauseMenuItem(){
	switch(static_cast<PauseMenuItem>(pauseMenuIndex_)){
	case PauseMenuItem::Restart:
		// シーンを作り直すことで、盤面・スコア・ゲージをまとめて初期状態に戻す
		SceneManager::GetInstance()->ChangeScene("GAME");
		break;

	case PauseMenuItem::Title:
		SceneManager::GetInstance()->ChangeScene("TITLE");
		break;

	case PauseMenuItem::Tutorial:
		pauseMode_ = PauseMode::Tutorial;
		break;

	default:
		break;
	}
}

// 追加：ポーズ画面を描画する。
void GameScene::DrawPause(){
	if(!isPaused_ || !spriteCommon_){
		return;
	}

	spriteCommon_->Draw();

	if(pauseOverlaySprite_){
		pauseOverlaySprite_->Draw();
	}

	if(pauseMode_ == PauseMode::Menu){
		if(pauseHeaderSprite_){
			pauseHeaderSprite_->Draw();
		}
		for(const auto& sprite : pauseMenuSprites_){
			sprite->Draw();
		}
	} else{
		// 変更：チュートリアル画面。内容をまとめた1枚絵を暗幕の上に重ねる
		if(pauseTutorialSheetSprite_){
			pauseTutorialSheetSprite_->Draw();
		}
	}
}

void GameScene::UpdateScoreUi(){
	if(scoreDigitSprites_.empty()){ return; }

	// 表示は8桁。範囲を超えた場合は99999999で止める。
	int64_t displayScore = score_.GetTotal();
	if(displayScore < 0){ displayScore = 0; }
	if(displayScore > 99999999){ displayScore = 99999999; }

	for(int32_t i = kScoreDigitCount - 1; i >= 0; --i){
		const int32_t digit = static_cast<int32_t>(displayScore % 10);
		displayScore /= 10;
		scoreDigitSprites_[i]->SetTextureLeftTop({
			kScoreDigitCellSize.x * static_cast<float>(digit) + kScoreDigitSampleInset.x,
			kScoreDigitSampleInset.y
		});
		scoreDigitSprites_[i]->Update();
	}
}

void GameScene::UpdateSpecialGaugeUi(){
	if(!specialGaugeBackgroundSprite_){ return; }
	const float width = kGaugeMaxSize.x * specialGauge_.GetRatio();
	const Vector2 fillSize = {width,kGaugeMaxSize.y};
	specialGaugeBackgroundSprite_->SetSize(kGaugeMaxSize);
	specialGaugeChargeSprite_->SetSize(fillSize);
	specialGaugeReadySprite_->SetSize(fillSize);
	specialGaugeActiveSprite_->SetSize(fillSize);

	if(specialGauge_.IsActivationActive()){
		++specialGaugeRainbowFrame_;
		constexpr int32_t colorCount = static_cast<int32_t>(std::size(kRainbowColors));
		const int32_t color = (specialGaugeRainbowFrame_ / kRainbowFramesPerColor) % colorCount;
		const int32_t next = (color + 1) % colorCount;
		const float t = static_cast<float>(specialGaugeRainbowFrame_ % kRainbowFramesPerColor) /
			static_cast<float>(kRainbowFramesPerColor);
		specialGaugeActiveSprite_->SetColor(LerpColor(kRainbowColors[color],kRainbowColors[next],t));
	}else{
		specialGaugeRainbowFrame_ = 0;
	}

	specialGaugeBackgroundSprite_->Update();
	specialGaugeChargeSprite_->Update();
	specialGaugeReadySprite_->Update();
	specialGaugeActiveSprite_->Update();
}

// スペシャル選択カーソルの位置と色を現在の対象に合わせる
void GameScene::SyncSpecialCursor(){
	if(!specialSelector_.IsSelecting() || !specialCursorObj_){
		return;
	}

	const GridPos target = specialSelector_.GetTarget();
	specialCursorObj_->SetTranslate(board_.GridToWorld(target.x,target.y));

	if(Model::Material* material = specialCursorObj_->GetMaterial()){
		material->color = specialSelector_.CanConfirm(board_) ?
			kSpecialCursorValidColor : kSpecialCursorInvalidColor;
	}

	specialCursorObj_->Update();
}

void GameScene::ConfirmSpecialTarget(){
	if(isGameOver_ || !specialGauge_.IsActivationActive() ||
		!specialSelector_.CanConfirm(board_)){
		return;
	}
	const GridPos target = specialSelector_.GetTarget();
	if(board_.ConvertToStrongest(target.x,target.y)){
		// 通電しなくても変換自体が成功すれば使用済み。十字マスは盤面に残る。
		specialGauge_.Consume();
		// この変換で始まる消去・連鎖からはゲージを再チャージしない。
		suppressSpecialClearCharge_ = board_.IsBusy();
		specialSelector_.Cancel();
	}
}

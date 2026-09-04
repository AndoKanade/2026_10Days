#include "GameScene.h"
#include "CameraManager.h"
#include "ImGuiManager.h"
#include "ModelManager.h"
#include "SoundManager.h"
#include "Input.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "SpriteCommon.h"
#include "Application.h"
#include "Logger.h"
#include "LevelManager.h"
#include "SceneManager.h"

namespace{
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

	// ブロックの種類名（ImGui表示用）
	const char* BlockTypeName(BlockShape::Type type){
		switch(type){
		case BlockShape::Type::T: return "T";
		case BlockShape::Type::L: return "L";
		case BlockShape::Type::I: return "I";
		case BlockShape::Type::J: return "J";
		}
		return "?";
	}

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
	activePlayFrames_ = 0;

	// カメラの生成・設定
	CameraManager::GetInstance()->CreateCamera("default",object3dCommon_->GetDxCommon()->GetDevice());
	auto* defaultCamera = CameraManager::GetInstance()->GetCamera("default");
	defaultCamera->SetTranslate({0.0f, 0.0f, -30.0f});
	CameraManager::GetInstance()->SetActiveCamera("default");
	object3dCommon_->SetDefaultCamera(CameraManager::GetInstance()->GetActiveCamera());

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

	// スペシャルの対象選択カーソルを用意する
	specialCursorObj_ = std::make_unique<Obj3D>();
	specialCursorObj_->Initialize(object3dCommon_);
	specialCursorObj_->SetModel(kBlockModel);
	specialCursorObj_->SetScale({kSpecialCursorScale,kSpecialCursorScale,kSpecialCursorScale});
	if(Model::Material* material = specialCursorObj_->GetMaterial()){
		material->color = kSpecialCursorValidColor;
		material->enableLighting = 0;
	}

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
	for(int32_t i = 0; i < PuzzleConfig::kNextQueueSize; ++i){
		nextQueue_.push_back(PickNextBlockType());
	}
}

// 追加：次に落ちてくるブロックの種類をひとつ抽選して返す。
BlockShape::Type GameScene::PickNextBlockType(){
	// 変更：全種類（L字・T字・I字・J字）を等確率で選ぶ。
	// 種類が増えても直すのは BlockShape::kTypeCount だけで済むよう、
	// Type の並び順をそのまま抽選値として使う。
	std::uniform_int_distribution<int32_t> dist(0,BlockShape::kTypeCount - 1);
	return static_cast<BlockShape::Type>(dist(randomEngine_));
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
		BuildPreviewShape(slotObjs,nextQueue_[i],anchorX,anchorY,nextColor);
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
}

// 追加：1個ぶんのブロックのプレビューを、指定した配列に構築する。
void GameScene::BuildPreviewShape(std::vector<std::unique_ptr<Obj3D>>& objs,BlockShape::Type type,int32_t anchorX,int32_t anchorY,const Vector4& color){
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
		obj->SetTranslate(board_.GridToWorld(anchorX + relative.x,anchorY + relative.y));

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
	// レベル配置JSONのホットリロード確認
	// ファイルが更新されていた場合、自動で再読み込みしてlevelObjects_を作り直す
	if (levelManager_.CheckAndReload()) {
		RebuildLevelObjects();
	}

	// 追加：盤面の更新
	board_.Update();
	for (const Board::ClearResult& result : board_.TakeClearResults()) {
		specialGauge_.AddFromClear(result.cellCount, result.chainCount);
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
				++activePlayFrames_;
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
						SceneManager::GetInstance()->ChangeScene("GAMEOVER");
						return;
					}
				}
				// 下キーは押しっぱなしで加速落下
				fallingBlock_.SetSoftDrop(input_->PushKey(DIK_S));

				// 時間経過を進め、盤面に固定されたら次のブロックを出す
				const bool blockLocked = input_->TriggerKey(DIK_RETURN)
					? fallingBlock_.HardDrop(board_)
					: fallingBlock_.Update(board_,PuzzleConfig::GetFallIntervalFrames(activePlayFrames_));

				if (blockLocked) {
					// 天井より上にはみ出したまま固定された ＝ 積み上がりすぎでゲームオーバー
					const bool lockedAboveCeiling = fallingBlock_.IsLockedAboveCeiling();

					// 次のブロック（ネクスト）を出現させる。出現位置が塞がっていても同様にゲームオーバー
					const bool spawned = SpawnNextBlock();

					if (lockedAboveCeiling || !spawned) {
						isGameOver_ = true;
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
		}

		for (auto& obj : levelObjects_) {
			obj->Update();
		}

		// スペースキーでゲームクリア画面へ遷移
		if (!specialSelector_.IsSelecting() && input_->TriggerKey(DIK_SPACE)) {
			SceneManager::GetInstance()->ChangeScene("GAMECLEAR");
		}

		// --- デバッグUIの表示 ---
#ifdef USE_IMGUI
		if (Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()) {
			// メインデバッグウィンドウ
			ImGui::Begin("GameScene Debug");

			if(ImGui::CollapsingHeader("Fall Speed")){
				ImGui::Text("Active time: %.1f sec",static_cast<double>(activePlayFrames_) / PuzzleConfig::kFrameRate);
				ImGui::Text("Fall interval: %.3f sec / cell",
					PuzzleConfig::GetFallIntervalFrames(activePlayFrames_) / PuzzleConfig::kFrameRate);
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
}

// --- 描画処理 ---
void GameScene::Draw(){
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

	// スペシャルの対象選択カーソルを最後に重ねて描画する
	if(specialSelector_.IsSelecting() && specialCursorObj_){
		specialCursorObj_->Draw();
	}
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
		specialSelector_.Cancel();
	}
}

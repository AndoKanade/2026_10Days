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
	const std::string kLevelJsonFile = "level.json"; // レベル配置情報のJSONファイル名

	// 追加：落下ブロックの描画に使うモデル（盤面と同じキューブで代用する）
	const std::string kBlockModel = "defaultBlock/defaultBlock.obj";

	// 追加：落下中ブロックの色（壁・固定マスと区別できる暖色にする）
	const Vector4 kFallingBlockColor = {0.95f, 0.75f, 0.25f, 1.0f};

	// 追加：着地予測（ゴースト）の色（落下中ブロックを暗く落とした色。影のように見せる）
	const Vector4 kGhostBlockColor = {0.45f, 0.35f, 0.15f, 1.0f};

	// 追加：ゴーストの拡大率（落下中ブロックのマス拡大率に対する倍率）
	constexpr float kGhostScaleRate = 0.7f;

	// スペシャル選択カーソルの見た目
	constexpr float kSpecialCursorScale = 0.50f;
	const Vector4 kSpecialCursorValidColor = {0.25f, 1.0f, 0.45f, 0.75f};
	const Vector4 kSpecialCursorInvalidColor = {1.0f, 0.25f, 0.25f, 0.75f};
}

GameScene::GameScene() = default;
GameScene::~GameScene() = default;

// --- 初期化 ---
void GameScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// カメラの生成・設定
	CameraManager::GetInstance()->CreateCamera("default",object3dCommon_->GetDxCommon()->GetDevice());
	auto* defaultCamera = CameraManager::GetInstance()->GetCamera("default");
	defaultCamera->SetTranslate({0.0f, 0.0f, -30.0f});
	CameraManager::GetInstance()->SetActiveCamera("default");
	object3dCommon_->SetDefaultCamera(CameraManager::GetInstance()->GetActiveCamera());

	SoundManager::GetInstance()->SoundLoadFile(kBgmPath_);

	// 追加：パズルの盤面を初期化する（盤面は3Dオブジェクトで描画する）
	board_.Initialize(object3dCommon_);

	// 追加：次ブロック抽選用の乱数エンジンをシードする
	std::random_device seedGenerator;
	randomEngine_.seed(seedGenerator());

	// 追加：ネクストを抽選してから、最初の落下ブロックを出現させる
	nextType_ = PickNextBlockType();
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
		if(Model::Material* material = obj->GetMaterial()){
			material->color = kFallingBlockColor;
			material->enableLighting = 0; // 2D的な見た目にするため陰影を切る
		}
		fallingObjs_.push_back(std::move(obj));

		// ゴースト（着地予測）。本体より少し小さくして影のように見せる
		auto ghost = std::make_unique<Obj3D>();
		ghost->Initialize(object3dCommon_);
		ghost->SetModel(kBlockModel);
		ghost->SetScale({PuzzleConfig::kCellModelScale * kGhostScaleRate, PuzzleConfig::kCellModelScale * kGhostScaleRate, PuzzleConfig::kCellModelScale * kGhostScaleRate});
		if(Model::Material* material = ghost->GetMaterial()){
			material->color = kGhostBlockColor;
			material->enableLighting = 0;
		}
		ghostObjs_.push_back(std::move(ghost));
	}
	SyncFallingObjs();

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
	// 落下中ブロック本体
	const std::vector<GridPos> cells = fallingBlock_.GetOccupiedCells();
	for(size_t i = 0; i < fallingObjs_.size() && i < cells.size(); ++i){
		fallingObjs_[i]->SetTranslate(board_.GridToWorld(cells[i].x,cells[i].y));
	}

	// ゴースト（いま真下に落とした場合の着地位置）
	const std::vector<GridPos> ghostCells = fallingBlock_.GetLandingCells(board_);
	for(size_t i = 0; i < ghostObjs_.size() && i < ghostCells.size(); ++i){
		ghostObjs_[i]->SetTranslate(board_.GridToWorld(ghostCells[i].x,ghostCells[i].y));
	}
}

// 追加：nextType_ のブロックを出現させ、次の nextType_ を抽選する。
bool GameScene::SpawnNextBlock(){
	// いま決まっているネクストを実際に出現させる
	const bool spawned = fallingBlock_.Spawn(board_,nextType_,nextBlockId_);

	// 次に出現するブロックへ振る元ブロックIDを1つ進める
	++nextBlockId_;

	// 次のネクストを抽選しておく（表示は次の出現まで固定される）
	nextType_ = PickNextBlockType();

	return spawned;
}

// 追加：次に落ちてくるブロックの種類をひとつ抽選して返す。
BlockShape::Type GameScene::PickNextBlockType(){
	// T字とL字を等確率で選ぶ（0 = T字、1 = L字）
	std::uniform_int_distribution<int32_t> dist(0,1);
	return (dist(randomEngine_) == 0) ? BlockShape::Type::T : BlockShape::Type::L;
}

// 追加：ImGui にネクスト（次に落ちてくるブロック）を表示する。
void GameScene::ShowNextBlockGui() const{
#ifdef USE_IMGUI
	if(!ImGui::CollapsingHeader("Next Block")){
		return;
	}

	// ブロックの種類名を表示する
	const char* typeName = "?";
	switch(nextType_){
	case BlockShape::Type::T:
		typeName = "T";
		break;
	case BlockShape::Type::L:
		typeName = "L";
		break;
	}
	ImGui::Text("Type: %s",typeName);

	// 形テーブル（回転0）を小さなグリッドとして描く
	const std::vector<GridPos>& shape = BlockShape::GetCells(nextType_,0);
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
	// ImGui の既定フォントは等幅なので、この方法で形が揃って見える
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

void GameScene::Finalize(){}

// --- 更新処理 ---
void GameScene::Update(){
	// レベル配置JSONのホットリロード確認
	// ファイルが更新されていた場合、自動で再読み込みしてlevelObjects_を作り直す
	if(levelManager_.CheckAndReload()){
		RebuildLevelObjects();
	}

	// 追加：盤面の更新
	board_.Update();
	for(const Board::ClearResult& result : board_.TakeClearResults()){
		specialGauge_.AddFromClear(result.cellCount,result.chainCount);
	}

	// スペシャル発動後は、対象選択中もゲージを減少させる
	if(!isGameOver_){
		specialGauge_.Update();
	}
	if(specialSelector_.IsSelecting() && !specialGauge_.IsActivationActive()){
		// 制限時間内に決定できなかったため、選択を終了する
		specialSelector_.Cancel();
	}

	// Qキーでスペシャルの対象選択を開始する
	if(!isGameOver_ && !board_.IsBusy() && !specialSelector_.IsSelecting() &&
		specialGauge_.CanActivate() && input_->TriggerKey(DIK_Q)){
		if(specialSelector_.Begin(board_)){
			specialGauge_.StartActivation();
		}
	}

	// 対象選択中は落下処理を止め、カーソル操作だけを受け付ける
	if(specialSelector_.IsSelecting()){
		fallingBlock_.SetSoftDrop(false);

		if(input_->TriggerKey(DIK_LEFT)){
			specialSelector_.Move(-1,0,board_);
		}
		if(input_->TriggerKey(DIK_RIGHT)){
			specialSelector_.Move(1,0,board_);
		}
		if(input_->TriggerKey(DIK_UP)){
			specialSelector_.Move(0,-1,board_);
		}
		if(input_->TriggerKey(DIK_DOWN)){
			specialSelector_.Move(0,1,board_);
		}
		if(input_->TriggerKey(DIK_RETURN)){
			ConfirmSpecialTarget();
		}
		SyncSpecialCursor();
	}else 	if (!isGameOver_) {
			// 追加：消去演出中（Board::IsBusy）はブロックの操作・落下・出現を止める
			if (!board_.IsBusy()) {
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
				// 下キーは押しっぱなしで加速落下
				fallingBlock_.SetSoftDrop(input_->PushKey(DIK_S));
	

			// 時間経過を進め、盤面に固定されたら次のブロックを出す
			const bool blockLocked = input_->TriggerKey(DIK_RETURN)
				? fallingBlock_.HardDrop(board_)
				: fallingBlock_.Update(board_);

			if(blockLocked){
				// 天井より上にはみ出したまま固定された ＝ 積み上がりすぎでゲームオーバー
				const bool lockedAboveCeiling = fallingBlock_.IsLockedAboveCeiling();

				// 次のブロック（ネクスト）を出現させる。出現位置が塞がっていても同様にゲームオーバー
				const bool spawned = SpawnNextBlock();

				if(lockedAboveCeiling || !spawned){
					isGameOver_ = true;
					SceneManager::GetInstance()->ChangeScene("GAMEOVER");
					return;
				}
			}

			// 描画オブジェクトを現在の占有マス／着地予測マスに合わせて動かす
			SyncFallingObjs();
		}

		for(auto& obj : fallingObjs_){
			obj->Update();
		}
		for(auto& obj : ghostObjs_){
			obj->Update();
		}
	}

	for(auto& obj : levelObjects_){
		obj->Update();
	}

	// スペースキーでゲームクリア画面へ遷移
	if(!specialSelector_.IsSelecting() && input_->TriggerKey(DIK_SPACE)){
		SceneManager::GetInstance()->ChangeScene("GAMECLEAR");
	}

	// --- デバッグUIの表示 ---
#ifdef USE_IMGUI
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		// メインデバッグウィンドウ
		ImGui::Begin("GameScene Debug");

		// 追加：次に落ちてくるブロック（ネクスト）の表示
		ShowNextBlockGui();

		// カメラ設定
		if(ImGui::CollapsingHeader("Camera Settings")){
			Vector3 camPos = activeCamera->GetTranslate();
			if(ImGui::DragFloat3("Camera Pos",&camPos.x,0.1f)){
				activeCamera->SetTranslate(camPos);
			}

			Vector3 camRot = activeCamera->GetRotate();
			if(ImGui::DragFloat3("Camera Rotate",&camRot.x,0.01f)){
				activeCamera->SetRotate(camRot);
			}
		}

		// ライティング設定
		if(ImGui::CollapsingHeader("Lighting")){
			if(PointLight* pData = object3dCommon_->GetPointLightData()){
				ImGui::Text("Point Light");
				ImGui::ColorEdit4("Point Color",&pData->color.x);
				ImGui::DragFloat3("Point Pos",&pData->position.x,0.1f);
				ImGui::DragFloat("Point Intensity",&pData->intensity,0.1f,0.0f,100.0f);
			}
			if(SpotLight* sData = object3dCommon_->GetSpotLightData()){
				ImGui::Text("Spot Light");
				ImGui::ColorEdit4("Spot Color",&sData->color.x);
			}
		}

		// レベル配置(JSON)の描画切り替え・手動ホットリロード
		if(ImGui::CollapsingHeader("Level Objects")){
			// 読み込んだ配置オブジェクトを描画するかどうかの切り替え
			ImGui::Checkbox("Draw Level Objects",&isLevelObjectsVisible_);

			// ボタン押下でJSONファイルを強制的に再読み込みする
			if(ImGui::Button("Reload Level JSON")){
				levelManager_.LoadJSON(kLevelJsonFile);
				RebuildLevelObjects();
			}
		}

		// スペシャルゲージの加算・消費を単独で確認する
		if(ImGui::CollapsingHeader("Special Gauge")){
			ImGui::Text("Gauge: %d / %d",specialGauge_.GetValue(),specialGauge_.GetMaxValue());
			ImGui::ProgressBar(specialGauge_.GetRatio(),ImVec2(-1.0f,0.0f));
			const char* specialStatus = specialGauge_.IsActivationActive() ? "ACTIVE" :
				(specialGauge_.CanActivate() ? "READY" : "CHARGING");
			ImGui::Text("Status: %s",specialStatus);
			ImGui::Text("Active Limit: %.1f seconds",
				static_cast<float>(PuzzleConfig::kSpecialReadyDurationFrames) / PuzzleConfig::kFrameRate);

			ImGui::InputInt("Cleared Cells",&debugClearedCellCount_);
			ImGui::InputInt("Chain Count",&debugChainCount_);

			if(ImGui::Button("Apply Clear Result")){
				specialGauge_.AddFromClear(debugClearedCellCount_,debugChainCount_);
			}

			if(ImGui::Button("Fill Gauge")){
				specialGauge_.Fill();
			}
			ImGui::SameLine();
			if(ImGui::Button("Complete Use (Debug)")){
				if(specialGauge_.Consume()){
					specialSelector_.Cancel();
				}
			}
			ImGui::SameLine();
			if(ImGui::Button("Reset Gauge")){
				specialGauge_.Reset();
				specialSelector_.Cancel();
			}

			if(!specialSelector_.IsSelecting()){
				if(ImGui::Button("Start Selection")){
					if(!isGameOver_ && !board_.IsBusy() && specialGauge_.CanActivate()){
						if(specialSelector_.Begin(board_)){
							specialGauge_.StartActivation();
						}
					}
				}
			}else{
				const GridPos target = specialSelector_.GetTarget();
				ImGui::Text("Target: (%d, %d)",target.x,target.y);
				ImGui::Text("Target Cell: %s",specialSelector_.CanConfirm(board_) ? "VALID" : "INVALID");
				ImGui::Text("Arrow Keys: Move / Enter: Confirm");
				if(ImGui::Button("Confirm Target")){
					ConfirmSpecialTarget();
				}
				if(ImGui::Button("Force Cancel (Debug)")){
					specialSelector_.Cancel();
					specialGauge_.Reset();
				}
			}
		}

		ModelManager::GetInstance()->UpdateLightGui();
		ImGui::End();

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

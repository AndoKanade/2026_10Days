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

	// 追加：最初の落下ブロック（T字）を出現させる
	if(!fallingBlock_.Spawn(board_,BlockShape::Type::T,nextBlockId_)){
		isGameOver_ = true;
	}

	// 追加：落下中ブロックの描画オブジェクトを、ブロックのマス数だけ用意する
	const size_t cellCount = fallingBlock_.GetOccupiedCells().size();
	for(size_t i = 0; i < cellCount; ++i){
		auto obj = std::make_unique<Obj3D>();
		obj->Initialize(object3dCommon_);
		obj->SetModel(kBlockModel);
		obj->SetScale({PuzzleConfig::kCellModelScale, PuzzleConfig::kCellModelScale, PuzzleConfig::kCellModelScale});

		if(Model::Material* material = obj->GetMaterial()){
			material->color = kFallingBlockColor;
			material->enableLighting = 0; // 2D的な見た目にするため陰影を切る
		}

		fallingObjs_.push_back(std::move(obj));
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

// 追加：落下中ブロックの描画オブジェクトの位置を、現在の占有マスに合わせる
void GameScene::SyncFallingObjs(){
	const std::vector<GridPos> cells = fallingBlock_.GetOccupiedCells();
	for(size_t i = 0; i < fallingObjs_.size() && i < cells.size(); ++i){
		fallingObjs_[i]->SetTranslate(board_.GridToWorld(cells[i].x,cells[i].y));
	}
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

	// スペシャル発動後は、対象選択中もゲージを減少させる
	if(!isGameOver_){
		specialGauge_.Update();
	}
	if(specialSelector_.IsSelecting() && !specialGauge_.IsActivationActive()){
		// 制限時間内に決定できなかったため、選択を終了する
		specialSelector_.Cancel();
	}

	// Qキーでスペシャルの対象選択を開始する
	if(!isGameOver_ && !specialSelector_.IsSelecting() &&
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
		SyncSpecialCursor();
	}else if(!isGameOver_){
		// 追加：落下中ブロックの操作と自動落下
		// 左右移動・回転はトリガー（押した瞬間）で1回ずつ
		if(input_->TriggerKey(DIK_A)){
			fallingBlock_.MoveLeft(board_);
		}
		if(input_->TriggerKey(DIK_D)){
			fallingBlock_.MoveRight(board_);
		}
		if(input_->TriggerKey(DIK_W)){
			fallingBlock_.Rotate(board_);
		}
		// 下キーは押しっぱなしで加速落下
		fallingBlock_.SetSoftDrop(input_->PushKey(DIK_S));

		// 時間経過を進め、盤面に固定されたら次のブロックを出す
		if(fallingBlock_.Update(board_)){
			++nextBlockId_;
			if(!fallingBlock_.Spawn(board_,BlockShape::Type::T,nextBlockId_)){
				isGameOver_ = true;
			}
		}

		// 描画オブジェクトを現在の占有マスに合わせて動かす
		SyncFallingObjs();
		for(auto& obj : fallingObjs_){
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
					if(specialGauge_.CanActivate()){
						if(specialSelector_.Begin(board_)){
							specialGauge_.StartActivation();
						}
					}
				}
			}else{
				const GridPos target = specialSelector_.GetTarget();
				ImGui::Text("Target: (%d, %d)",target.x,target.y);
				ImGui::Text("Target Cell: %s",specialSelector_.CanConfirm(board_) ? "VALID" : "EMPTY");
				ImGui::Text("Arrow Keys: Move");
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

	// 追加：落下中のブロックを描画する
	if(!isGameOver_){
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

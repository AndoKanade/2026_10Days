#include "GameOverScene.h"

#include "Input.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "WinAPI.h"
#include "ImGuiManager.h"
#include <ctime>

namespace{
	// 背景に使うテクスチャ（専用画像がないため既存テクスチャを赤で着色して流用）
	const std::string kBackgroundTexture = "resource/uvChecker.png";
	const Vector4 kOverColor = {1.0f, 0.3f, 0.3f, 1.0f};
}

GameOverScene::GameOverScene() = default;
GameOverScene::~GameOverScene() = default;

// 初期化処理
void GameOverScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// 実行ファイルの隣に保存し、起動時の作業フォルダに依存させない。
	if(!resultRecorded_){
		resultRecorded_ = true;
		wchar_t executable[32768]{};
		const DWORD length = GetModuleFileNameW(nullptr,executable,32768);
		const bool pathValid = length > 0 && length < 32768;
		const auto path = std::filesystem::path(executable).parent_path() / "save" / "score_history.txt";
		historyLoaded_ = pathValid && history_.Load(path);
		ScoreRecord record;
		record.score = SceneManager::GetInstance()->GetFinalScore();
		record.cells = SceneManager::GetInstance()->GetFinalClearedCells();
		const std::time_t now = std::time(nullptr);
		std::tm local{};
		char date[20]{};
		if(localtime_s(&local,&now) == 0){ std::strftime(date,sizeof(date),"%Y-%m-%d %H:%M:%S",&local); }
		record.date = date[0] ? date : "0000-00-00 00:00:00";
		isNewRecord_ = historyLoaded_ && record.score > 0 &&
			(history_.GetRecords().empty() || record.score > history_.GetRecords().front().score);
		// 読み込み失敗時は既存ファイルを変更しない。今回の結果は画面には表示する。
		historySaved_ = historyLoaded_ && ScoreHistory::Append(path,record);
		history_.Add(record);
	}

	// --- 背景スプライトの生成 ---
	TextureManager::GetInstance()->LoadTexture(kBackgroundTexture);

	background_ = std::make_unique<Sprite>();
	background_->Initialize(spriteCommon_,kBackgroundTexture);
	background_->SetPosition({0.0f, 0.0f});
	background_->SetSize({float(WinAPI::kClientWidth), float(WinAPI::kClientHeight)});
	background_->SetColor(kOverColor);
}

void GameOverScene::Finalize(){
	// unique_ptrにより自動解放されるため処理なし
}

// 更新処理
void GameOverScene::Update(){
#ifdef USE_IMGUI
	ImGui::Begin("Game Over Result");
	ImGui::Text("Final Score: %lld",static_cast<long long>(SceneManager::GetInstance()->GetFinalScore()));
	ImGui::Text("Total cleared cells: %lld",static_cast<long long>(SceneManager::GetInstance()->GetFinalClearedCells()));
	if(isNewRecord_){ ImGui::Text("NEW RECORD!"); }
	if(!historyLoaded_){ ImGui::Text("History could not be fully loaded. Existing file was not changed."); }
	if(!historySaved_){ ImGui::Text("This result could not be saved."); }
	ImGui::Separator();
	ImGui::Text("High Scores - Top 10 (including this run)");
	if(ImGui::BeginTable("ScoreHistory",4,ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)){
		ImGui::TableSetupColumn("Rank");
		ImGui::TableSetupColumn("Score");
		ImGui::TableSetupColumn("Cleared cells");
		ImGui::TableSetupColumn("Date");
		ImGui::TableHeadersRow();
		int rank = 1;
		for(const auto& record : history_.GetRecords()){
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%d",rank++);
			ImGui::TableNextColumn(); ImGui::Text("%lld",static_cast<long long>(record.score));
			ImGui::TableNextColumn(); ImGui::Text("%lld",static_cast<long long>(record.cells));
			ImGui::TableNextColumn(); ImGui::TextUnformatted(record.date.c_str());
		}
		ImGui::EndTable();
	}
	ImGui::Text("SPACE: Return to title");
	ImGui::End();
#endif
	if(background_){
		background_->Update();
	}

	// スペースキーでタイトル画面へ遷移
	if(input_->TriggerKey(DIK_SPACE)){
		SceneManager::GetInstance()->ChangeScene("TITLE");
	}
}

// 描画処理
void GameOverScene::Draw(){
	if(spriteCommon_ && background_){
		spriteCommon_->Draw();
		background_->Draw();
	}
}

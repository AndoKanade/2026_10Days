#include "GameOverScene.h"

#include "Input.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "SoundManager.h"
#include "WinAPI.h"
#include "ImGuiManager.h"
#include <ctime>

namespace{
	// 背景に使うテクスチャ（専用画像がないため既存テクスチャを赤で着色して流用）
	const std::string kBackgroundTexture = "resource/uvChecker.png";
	const Vector4 kOverColor = {1.0f, 0.3f, 0.3f, 1.0f};
	const std::string kNumberTexture = "resource/ui/score/numbers.png";
	const std::string kRankingTexture = "resource/ui/score/ranking.png";
	const std::string kRankTexture = "resource/ui/score/rank.png";
	const std::string kScoreTexture = "resource/ui/score/score.png";
	const std::string kYourScoreTexture = "resource/ui/score/yourScore.png";
	const std::string kSolidTexture = "resource/character/white.png";
	const std::string kBgmPath = "resource/music/bgm/As_Time_Carries_Us_Away.mp3";
	constexpr float kBgmVolume = 0.5f;
	constexpr Vector2 kNumberCellSize = {8.0f,12.0f};
	// 線形補間で隣の数字を拾わないよう、セル境界ではなく端のピクセル中心を参照する。
	constexpr Vector2 kNumberSampleInset = {0.5f,0.5f};
	constexpr Vector2 kNumberSampleSize = {7.0f,11.0f};
	constexpr Vector2 kNumberDrawSize = {32.0f,48.0f};
	constexpr int32_t kRankingCount = 5;
	constexpr float kPanelX = 310.0f;
	constexpr float kPanelWidth = 660.0f;
	constexpr float kRankingStartY = 126.0f;
	constexpr float kRankingRowStep = 60.0f;
	constexpr Vector2 kRankingPanelSize = {kPanelWidth,54.0f};
	constexpr float kRankX = 350.0f;
	constexpr float kScoreX = 510.0f;
	const Vector4 kRankingPanelColor = {0.90f,0.92f,1.0f,1.0f};
	const Vector4 kCurrentPanelColor = {1.0f,0.88f,0.35f,1.0f};
	const Vector4 kSeparatorColor = {0.12f,0.10f,0.20f,1.0f};
	constexpr int32_t kRainbowFramesPerColor = 12;
	constexpr int32_t kRainbowColorCount = 7;
	constexpr Vector4 kYourScoreRainbowColors[kRainbowColorCount] = {
		{1.0f,0.15f,0.15f,1.0f}, {1.0f,0.55f,0.10f,1.0f},
		{1.0f,0.95f,0.10f,1.0f}, {0.15f,1.0f,0.25f,1.0f},
		{0.10f,0.85f,1.0f,1.0f}, {0.20f,0.30f,1.0f,1.0f},
		{0.75f,0.20f,1.0f,1.0f}
	};
	Vector4 LerpColor(const Vector4& a,const Vector4& b,float t){
		return {a.x + (b.x-a.x)*t,a.y + (b.y-a.y)*t,
			a.z + (b.z-a.z)*t,a.w + (b.w-a.w)*t};
	}
}

GameOverScene::GameOverScene() = default;
GameOverScene::~GameOverScene() = default;

// 初期化処理
void GameOverScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;
	currentScore_ = SceneManager::GetInstance()->GetFinalScore();
	currentRank_ = 1;
	// 同点は先に登録された記録を上位にするため、既存の同点も今回より上に数える。
	for(const auto& oldRecord : history_.GetRecords()){
		if(oldRecord.score >= currentScore_){ ++currentRank_; }
	}

	// 今回の起動中だけ共有するランキングへ、1プレイにつき1回登録する。
	if(!resultRecorded_){
		resultRecorded_ = true;
		ScoreRecord record;
		record.score = currentScore_;
		record.cells = SceneManager::GetInstance()->GetFinalClearedCells();
		const std::time_t now = std::time(nullptr);
		std::tm local{};
		char date[20]{};
		if(localtime_s(&local,&now) == 0){ std::strftime(date,sizeof(date),"%Y-%m-%d %H:%M:%S",&local); }
		record.date = date[0] ? date : "0000-00-00 00:00:00";
		isNewRecord_ = record.score > 0 &&
			(history_.GetRecords().empty() || record.score > history_.GetRecords().front().score);
		history_.Add(record);
	}

	// --- 背景スプライトの生成 ---
	TextureManager::GetInstance()->LoadTexture(kBackgroundTexture);

	background_ = std::make_unique<Sprite>();
	background_->Initialize(spriteCommon_,kBackgroundTexture);
	background_->SetPosition({0.0f, 0.0f});
	background_->SetSize({float(WinAPI::kClientWidth), float(WinAPI::kClientHeight)});
	background_->SetColor(kOverColor);

	// 上位5件と今回スコアを、ImGuiを使わない通常UIとして作る。
	TextureManager::GetInstance()->LoadTexture(kNumberTexture);
	TextureManager::GetInstance()->LoadTexture(kSolidTexture);
	TextureManager::GetInstance()->LoadTexture(kRankingTexture);
	TextureManager::GetInstance()->LoadTexture(kRankTexture);
	TextureManager::GetInstance()->LoadTexture(kScoreTexture);
	TextureManager::GetInstance()->LoadTexture(kYourScoreTexture);
	rankingRowBackgrounds_.clear();
	rankingNumberSprites_.clear();
	currentNumberSprites_.clear();

	const auto& records = history_.GetRecords();
	for(int32_t row = 0; row < kRankingCount; ++row){
		auto panel = std::make_unique<Sprite>();
		panel->Initialize(spriteCommon_,kSolidTexture);
		panel->SetPosition({kPanelX,kRankingStartY + kRankingRowStep * static_cast<float>(row)});
		panel->SetSize(kRankingPanelSize);
		panel->SetColor(kRankingPanelColor);
		rankingRowBackgrounds_.push_back(std::move(panel));

		if(static_cast<size_t>(row) < records.size()){
			const float numberY = kRankingStartY + 3.0f + kRankingRowStep * static_cast<float>(row);
			AppendNumberSprites(rankingNumberSprites_,row + 1,2,{kRankX,numberY},kNumberDrawSize);
			AppendNumberSprites(rankingNumberSprites_,records[row].score,8,{kScoreX,numberY},kNumberDrawSize);
		}
	}

	separator_ = std::make_unique<Sprite>();
	separator_->Initialize(spriteCommon_,kSolidTexture);
	separator_->SetPosition({kPanelX,436.0f});
	separator_->SetSize({kPanelWidth,8.0f});
	separator_->SetColor(kSeparatorColor);

	currentScoreBackground_ = std::make_unique<Sprite>();
	currentScoreBackground_->Initialize(spriteCommon_,kSolidTexture);
	currentScoreBackground_->SetPosition({kPanelX,512.0f});
	currentScoreBackground_->SetSize({kPanelWidth,64.0f});
	currentScoreBackground_->SetColor(kCurrentPanelColor);
	AppendNumberSprites(currentNumberSprites_,currentRank_,2,{kRankX,520.0f},kNumberDrawSize);
	AppendNumberSprites(currentNumberSprites_,currentScore_,8,{kScoreX,520.0f},kNumberDrawSize);

	auto createLabel = [&](const std::string& texture,const Vector2& position,const Vector2& size){
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_,texture);
		sprite->SetPosition(position);
		sprite->SetSize(size);
		return sprite;
	};
	rankingLabel_ = createLabel(kRankingTexture,{528.0f,12.0f},{224.0f,48.0f});
	rankLabel_ = createLabel(kRankTexture,{350.0f,72.0f},{128.0f,48.0f});
	scoreLabel_ = createLabel(kScoreTexture,{560.0f,72.0f},{160.0f,48.0f});
	yourScoreLabel_ = createLabel(kYourScoreTexture,{480.0f,452.0f},{320.0f,48.0f});
	// 元画像の緑色ではなく透明度を文字の形として使い、全色へ着色可能にする。
	yourScoreLabel_->SetUseAlphaMask(true);
	yourScoreRainbowFrame_ = 0;

	// スコア画面のBGMをロードしてループ再生する。
	SoundManager::GetInstance()->SoundLoadFile(kBgmPath);
	SoundManager::GetInstance()->PlayAudio(kBgmPath,kBgmVolume,true);
}

void GameOverScene::AppendNumberSprites(std::vector<std::unique_ptr<Sprite>>& destination,
	int64_t value,int32_t digitCount,const Vector2& position,const Vector2& digitDrawSize){
	if(value < 0){ value = 0; }
	int64_t maxValue = 1;
	for(int32_t i = 0; i < digitCount; ++i){ maxValue *= 10; }
	if(value >= maxValue){ value = maxValue - 1; }

	std::vector<int32_t> digits(static_cast<size_t>(digitCount));
	for(int32_t i = digitCount - 1; i >= 0; --i){
		digits[static_cast<size_t>(i)] = static_cast<int32_t>(value % 10);
		value /= 10;
	}

	for(int32_t i = 0; i < digitCount; ++i){
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_,kNumberTexture);
		sprite->SetPosition({position.x + digitDrawSize.x * static_cast<float>(i),position.y});
		sprite->SetSize(digitDrawSize);
		sprite->SetTextureSize(kNumberSampleSize);
		sprite->SetTextureLeftTop({
			kNumberCellSize.x * static_cast<float>(digits[static_cast<size_t>(i)]) + kNumberSampleInset.x,
			kNumberSampleInset.y
		});
		destination.push_back(std::move(sprite));
	}
}

void GameOverScene::Finalize(){
	SoundManager::GetInstance()->StopAudio(kBgmPath);

	// unique_ptrにより自動解放されるため処理なし
}

// 更新処理
void GameOverScene::Update(){
#ifdef USE_IMGUI
	SoundManager::GetInstance()->ShowVolumeGui();
	ImGui::Begin("Game Over Result");
	ImGui::Text("Final Score: %lld",static_cast<long long>(SceneManager::GetInstance()->GetFinalScore()));
	ImGui::Text("Total cleared cells: %lld",static_cast<long long>(SceneManager::GetInstance()->GetFinalClearedCells()));
	if(isNewRecord_){ ImGui::Text("NEW RECORD!"); }
	ImGui::Separator();
	ImGui::Text("High Scores - Top 5 (this session)");
	ImGui::Text("Records reset when the game is closed.");
	if(ImGui::BeginTable("ScoreHistory",4,ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)){
		ImGui::TableSetupColumn("Rank");
		ImGui::TableSetupColumn("Score");
		ImGui::TableSetupColumn("Cleared cells");
		ImGui::TableSetupColumn("Date");
		ImGui::TableHeadersRow();
		int rank = 1;
		for(const auto& record : history_.GetRecords()){
			if(rank > kRankingCount){ break; }
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%d",rank++);
			ImGui::TableNextColumn(); ImGui::Text("%lld",static_cast<long long>(record.score));
			ImGui::TableNextColumn(); ImGui::Text("%lld",static_cast<long long>(record.cells));
			ImGui::TableNextColumn(); ImGui::TextUnformatted(record.date.c_str());
		}
		ImGui::EndTable();
	}
	ImGui::Separator();
	ImGui::Text("This Play - Rank %d / Score %lld",currentRank_,static_cast<long long>(currentScore_));
	ImGui::Text("SPACE: Return to title");
	ImGui::End();
#endif
	if(background_){
		background_->Update();
	}
	for(auto& panel : rankingRowBackgrounds_){ panel->Update(); }
	if(separator_){ separator_->Update(); }
	if(currentScoreBackground_){ currentScoreBackground_->Update(); }
	if(rankingLabel_){ rankingLabel_->Update(); }
	if(rankLabel_){ rankLabel_->Update(); }
	if(scoreLabel_){ scoreLabel_->Update(); }
	if(yourScoreLabel_){
		++yourScoreRainbowFrame_;
		const int32_t color = (yourScoreRainbowFrame_ / kRainbowFramesPerColor) % kRainbowColorCount;
		const int32_t next = (color + 1) % kRainbowColorCount;
		const float t = static_cast<float>(yourScoreRainbowFrame_ % kRainbowFramesPerColor) /
			static_cast<float>(kRainbowFramesPerColor);
		yourScoreLabel_->SetColor(LerpColor(kYourScoreRainbowColors[color],kYourScoreRainbowColors[next],t));
		yourScoreLabel_->Update();
	}
	for(auto& number : rankingNumberSprites_){ number->Update(); }
	for(auto& number : currentNumberSprites_){ number->Update(); }

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
		for(const auto& panel : rankingRowBackgrounds_){ panel->Draw(); }
		if(separator_){ separator_->Draw(); }
		if(currentScoreBackground_){ currentScoreBackground_->Draw(); }
		if(rankingLabel_){ rankingLabel_->Draw(); }
		if(rankLabel_){ rankLabel_->Draw(); }
		if(scoreLabel_){ scoreLabel_->Draw(); }
		if(yourScoreLabel_){ yourScoreLabel_->Draw(); }
		for(const auto& number : rankingNumberSprites_){ number->Draw(); }
		for(const auto& number : currentNumberSprites_){ number->Draw(); }
	}
}

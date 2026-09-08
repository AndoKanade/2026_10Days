#include "GameOverScene.h"

#include "Input.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "SoundManager.h"
#include "WinAPI.h"
#include "ImGuiManager.h"
// 修正：コンフリクト解消 天球用のインクルードを残しました
#include "CameraManager.h" // 追加：天球の描画に使うカメラを取得する
#include "Obj3dCommon.h"   // 追加：天球の共通設定の初期化にDXCommonが要る
#include "DXCommon.h"
#include "Skybox.h"        // 追加：背景の天球
#include "SkyboxCommon.h"  // 追加：天球の共通設定
#include "PuzzleConfig.h"  // 追加：天球の調整用定数
#include <cmath>
#include <ctime>

namespace{
	// 削除：仮置きだった背景テクスチャ（uvCheckerの赤着色）。背景は天球に置き換えた。
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
	constexpr float kRankX = 350.0f;
	constexpr float kScoreX = 510.0f;
	const Vector4 kSeparatorColor = {0.12f,0.10f,0.20f,1.0f};
	const Vector4 kRankingLightningColor = {0.15f,0.85f,1.0f,1.0f};
	const Vector4 kCurrentLightningColor = {1.0f,0.85f,0.10f,1.0f};
	// 60fps想定で約2秒ごとに、24フレームかけて光が通る。
	constexpr int32_t kShineCycleFrames = 120;
	constexpr int32_t kShineDurationFrames = 24;
	constexpr int32_t kRainbowFramesPerColor = 12;
	constexpr int32_t kRainbowColorCount = 7;
	// yourScoreLabel_の虹色アニメーション用の色。赤→橙→黄→緑→水→青→紫。
	constexpr Vector4 kYourScoreRainbowColors[kRainbowColorCount] = {
		{1.0f,0.15f,0.15f,1.0f}, {1.0f,0.55f,0.10f,1.0f},
		{1.0f,0.95f,0.10f,1.0f}, {0.15f,1.0f,0.25f,1.0f},
		{0.10f,0.85f,1.0f,1.0f}, {0.20f,0.30f,1.0f,1.0f},
		{0.75f,0.20f,1.0f,1.0f}
	};
	// 2つの色を線形補間する。t=0.0でa、t=1.0でb。
	Vector4 LerpColor(const Vector4& a,const Vector4& b,float t){
		return {a.x + (b.x-a.x)*t,a.y + (b.y-a.y)*t,
			a.z + (b.z-a.z)*t,a.w + (b.w-a.w)*t};
	}
	// 光の通過量を0.0～1.0で返す。周期的に繰り返す。
	float GetShineAmount(int32_t frame,int32_t delay){
		int32_t localFrame = (frame - delay) % kShineCycleFrames;
		if(localFrame < 0){ localFrame += kShineCycleFrames; }
		if(localFrame >= kShineDurationFrames){ return 0.0f; }
		constexpr float kPi = 3.14159265358979323846f;
		return std::sin(kPi * static_cast<float>(localFrame) /
			static_cast<float>(kShineDurationFrames));
	}
	// 光の通過量を元に、明るさを計算してRGBAで返す。1.0が通常の明るさ。
	Vector4 GetBrightnessColor(float shine){
		const float brightness = 1.0f + shine * 2.0f;
		return {brightness,brightness,brightness,1.0f};
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

	// --- 変更：背景を仮置きのスプライトから、ゲーム中と同じ天球に置き換える ---
	// 天球の共通設定（ルートシグネチャとPSO）はエンジン側で作られないため、シーンごとに作る。
	// カメラはゲーム中のものがそのまま残っているため、それを使って見え方を揃える。
	skyboxCommon_ = std::make_unique<SkyboxCommon>();
	skyboxCommon_->Initialize(object3dCommon_->GetDxCommon());
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(skyboxCommon_.get(),PuzzleConfig::kSkyboxBackgroundTexture);
	skyboxRotationY_ = 0.0f;
	skyboxPulseFrame_ = 0;
	// 1フレーム目の描画に間に合うよう、行列をここで一度作っておく
	if(Camera* camera = CameraManager::GetInstance()->GetActiveCamera()){
		skybox_->Update(*camera);
	}

	// 上位5件と今回スコアを、ImGuiを使わない通常UIとして作る。
	TextureManager::GetInstance()->LoadTexture(kNumberTexture);
	TextureManager::GetInstance()->LoadTexture(kSolidTexture);
	TextureManager::GetInstance()->LoadTexture(kRankingTexture);
	TextureManager::GetInstance()->LoadTexture(kRankTexture);
	TextureManager::GetInstance()->LoadTexture(kScoreTexture);
	TextureManager::GetInstance()->LoadTexture(kYourScoreTexture);
	rankingLightningSprites_.clear();
	currentLightningSprites_.clear();
	rankingNumberSprites_.clear();
	currentNumberSprites_.clear();

	const auto& records = history_.GetRecords();
	for(int32_t row = 0; row < kRankingCount; ++row){
		const float rowY = kRankingStartY + kRankingRowStep * static_cast<float>(row);
		AppendLightningUnderline(rankingLightningSprites_,kPanelX,kPanelX + kPanelWidth,
			rowY + 54.0f,kRankingLightningColor);

		if(static_cast<size_t>(row) < records.size()){
			const float numberY = rowY + 3.0f;
			AppendNumberSprites(rankingNumberSprites_,row + 1,2,{kRankX,numberY},kNumberDrawSize);
			AppendNumberSprites(rankingNumberSprites_,records[row].score,8,{kScoreX,numberY},kNumberDrawSize);
		}
	}

	separator_ = std::make_unique<Sprite>();
	separator_->Initialize(spriteCommon_,kSolidTexture);
	separator_->SetPosition({kPanelX,436.0f});
	separator_->SetSize({kPanelWidth,8.0f});
	separator_->SetColor(kSeparatorColor);

	AppendNumberSprites(currentNumberSprites_,currentRank_,2,{kRankX,520.0f},kNumberDrawSize);
	AppendNumberSprites(currentNumberSprites_,currentScore_,8,{kScoreX,520.0f},kNumberDrawSize);
	AppendLightningUnderline(currentLightningSprites_,kPanelX,kPanelX + kPanelWidth,
		576.0f,kCurrentLightningColor);

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
	scoreShineFrame_ = 0;

	// スコア画面のBGMをロードしてループ再生する。
	SoundManager::GetInstance()->SoundLoadFile(kBgmPath);
	SoundManager::GetInstance()->PlayAudio(kBgmPath,kBgmVolume,true);
}
// 電撃風の下線を作る。ジグザグの線を16分割で作る。
void GameOverScene::AppendLightningUnderline(std::vector<std::unique_ptr<Sprite>>& destination,
	float startX,float endX,float baseY,const Vector4& color){
	// ジグザグのオフセットを16分割で作る。最後の要素は終点のオフセットとして使う。
	constexpr int32_t kSegmentCount = 16;
	// 16分割のオフセットは、-6～+6の範囲でランダムに作る。最後の要素は終点のオフセットとして使う。
	constexpr float kOffsets[kSegmentCount + 1] = {
		0.0f,-5.0f,3.0f,-2.0f,6.0f,-4.0f,2.0f,-6.0f,1.0f,
		5.0f,-3.0f,4.0f,-5.0f,2.0f,-1.0f,5.0f,0.0f
	};
	// 16分割の幅を計算し、各セグメントごとに線を作る。
	const float segmentWidth = (endX - startX) / static_cast<float>(kSegmentCount);
	// 16分割の各セグメントを線として作る。線の長さは2点間の距離、角度はatan2で計算する。
	for(int32_t segment = 0; segment < kSegmentCount; ++segment){
		const float x0 = startX + segmentWidth * static_cast<float>(segment);
		const float x1 = x0 + segmentWidth;
		const float y0 = baseY + kOffsets[segment];
		const float y1 = baseY + kOffsets[segment + 1];
		const float dx = x1 - x0;
		const float dy = y1 - y0;

		auto line = std::make_unique<Sprite>();
		line->Initialize(spriteCommon_,kSolidTexture);
		line->SetPosition({x0,y0});
		line->SetSize({std::sqrt(dx * dx + dy * dy),3.0f});
		line->SetRotation(std::atan2(dy,dx));
		line->SetColor(color);
		destination.push_back(std::move(line));
	}
}
// numbers.pngから数字を切り出し、指定位置へ桁数ぶん並べる。
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
	// 変更：仮置きの背景スプライトの代わりに、背景の天球を進める。
	// 動かし方はゲーム中・タイトルと同じ。
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

		if(Camera* camera = CameraManager::GetInstance()->GetActiveCamera()){
			skybox_->Update(*camera);
		}
	}
	++lightningAnimationFrame_;
	for(size_t i = 0; i < rankingLightningSprites_.size(); ++i){
		const bool bright = ((lightningAnimationFrame_ + static_cast<int32_t>(i) * 3) % 11) < 8;
		rankingLightningSprites_[i]->SetColor(bright ? kRankingLightningColor : Vector4{0.05f,0.35f,0.50f,0.65f});
		rankingLightningSprites_[i]->Update();
	}
	for(size_t i = 0; i < currentLightningSprites_.size(); ++i){
		const bool bright = ((lightningAnimationFrame_ + static_cast<int32_t>(i) * 5) % 13) < 10;
		currentLightningSprites_[i]->SetColor(bright ? kCurrentLightningColor : Vector4{0.55f,0.25f,0.05f,0.65f});
		currentLightningSprites_[i]->Update();
	}
	if(separator_){ separator_->Update(); }
	++scoreShineFrame_;
	if(scoreShineFrame_ >= kShineCycleFrames){ scoreShineFrame_ = 0; }
	if(rankingLabel_){
		rankingLabel_->SetColor(GetBrightnessColor(GetShineAmount(scoreShineFrame_,0)));
		rankingLabel_->Update();
	}
	if(rankLabel_){
		rankLabel_->SetColor(GetBrightnessColor(GetShineAmount(scoreShineFrame_,5)));
		rankLabel_->Update();
	}
	if(scoreLabel_){
		scoreLabel_->SetColor(GetBrightnessColor(GetShineAmount(scoreShineFrame_,10)));
		scoreLabel_->Update();
	}
	if(yourScoreLabel_){
		++yourScoreRainbowFrame_;
		const int32_t color = (yourScoreRainbowFrame_ / kRainbowFramesPerColor) % kRainbowColorCount;
		const int32_t next = (color + 1) % kRainbowColorCount;
		const float t = static_cast<float>(yourScoreRainbowFrame_ % kRainbowFramesPerColor) /
			static_cast<float>(kRainbowFramesPerColor);
		const Vector4 rainbowColor = LerpColor(kYourScoreRainbowColors[color],kYourScoreRainbowColors[next],t);
		const float shine = GetShineAmount(scoreShineFrame_,15) * 0.8f;
		yourScoreLabel_->SetColor(LerpColor(rainbowColor,{1.0f,1.0f,1.0f,1.0f},shine));
		yourScoreLabel_->Update();
	}
	for(size_t i = 0; i < rankingNumberSprites_.size(); ++i){
		// 数字を左から右へ順番に光らせる。
		const int32_t delay = 18 + static_cast<int32_t>(i) * 2;
		rankingNumberSprites_[i]->SetColor(GetBrightnessColor(GetShineAmount(scoreShineFrame_,delay)));
		rankingNumberSprites_[i]->Update();
	}
	for(size_t i = 0; i < currentNumberSprites_.size(); ++i){
		const int32_t delay = 24 + static_cast<int32_t>(i) * 2;
		currentNumberSprites_[i]->SetColor(GetBrightnessColor(GetShineAmount(scoreShineFrame_,delay)));
		currentNumberSprites_[i]->Update();
	}

	// スペースキーでタイトル画面へ遷移
	if(input_->TriggerKey(DIK_SPACE)){
		SceneManager::GetInstance()->ChangeScene("TITLE");
	}
}

// 描画処理
void GameOverScene::Draw(){
	// 追加：背景の天球を最初に描く。
	// 天球は深度を書き込まないので、このあとのスプライトがそのまま手前に重なる。
	if(skybox_){
		skybox_->Draw();
	}

	// 変更：背景スプライトを消したため、条件から background_ を外した
	if(spriteCommon_){
		spriteCommon_->Draw();
		// 修正：コンフリクト解消 宣言のない背景パネルと不要になった古い背景スプライトを削除し、雷エフェクトの描画を残しました
		for(const auto& line : rankingLightningSprites_){ line->Draw(); }
		for(const auto& line : currentLightningSprites_){ line->Draw(); }
		if(separator_){ separator_->Draw(); }
		if(rankingLabel_){ rankingLabel_->Draw(); }
		if(rankLabel_){ rankLabel_->Draw(); }
		if(scoreLabel_){ scoreLabel_->Draw(); }
		if(yourScoreLabel_){ yourScoreLabel_->Draw(); }
		for(const auto& number : rankingNumberSprites_){ number->Draw(); }
		for(const auto& number : currentNumberSprites_){ number->Draw(); }
	}
}
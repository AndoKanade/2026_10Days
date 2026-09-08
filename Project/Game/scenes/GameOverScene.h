#pragma once

#include "BaseScene.h"
#include "MyMath.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "ScoreHistory.h"

// --- 前方宣言 ---
class Input;
class Obj3dCommon;
class Sprite;
class SpriteCommon;
class Skybox;       // 追加：背景の天球
class SkyboxCommon; // 追加：天球の共通設定（ルートシグネチャとPSO）

/// <summary>
/// ゲームオーバー画面シーン
/// </summary>
class GameOverScene : public BaseScene{
public:
	// --- コンストラクタ・デストラクタ ---
	GameOverScene();
	~GameOverScene() override;

	// --- BaseScene オーバーライド ---
	void Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	// --- メンバ変数：外部依存 (借りてくるもの) ---
	Obj3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;

	// --- メンバ変数：内部リソース (所有するもの) ---

	// 削除：仮置きだったuvCheckerの全画面スプライト（背景は天球に置き換えた）

	// 追加：ゲーム中・タイトルと同じゲーミングな背景の天球
	std::unique_ptr<SkyboxCommon> skyboxCommon_;
	std::unique_ptr<Skybox> skybox_;

	// 天球の回転角（ラジアン）。毎フレーム少しずつ足して背景を流す。
	float skyboxRotationY_ = 0.0f;

	// 明るさを脈打たせるための経過フレーム数
	int32_t skyboxPulseFrame_ = 0;

	// 修正：コンフリクト解消 削除された古い背景とパネルの変数を消し、雷エフェクトの変数を残しました
	std::vector<std::unique_ptr<Sprite>> rankingLightningSprites_;
	std::vector<std::unique_ptr<Sprite>> currentLightningSprites_;

	std::unique_ptr<Sprite> separator_;
	std::unique_ptr<Sprite> rankingLabel_;
	std::unique_ptr<Sprite> rankLabel_;
	std::unique_ptr<Sprite> scoreLabel_;
	std::unique_ptr<Sprite> yourScoreLabel_;
	std::vector<std::unique_ptr<Sprite>> rankingNumberSprites_;
	std::vector<std::unique_ptr<Sprite>> currentNumberSprites_;
	// シーンを作り直しても共有し、アプリを終了すると破棄する。
	inline static ScoreHistory history_;
	bool resultRecorded_ = false;
	bool isNewRecord_ = false;
	int64_t currentScore_ = 0;
	int32_t currentRank_ = 1;
	int32_t yourScoreRainbowFrame_ = 0;
	int32_t lightningAnimationFrame_ = 0;
	int32_t scoreShineFrame_ = 0;

	// numbers.pngから数字を切り出し、指定位置へ桁数ぶん並べる。
	void AppendNumberSprites(std::vector<std::unique_ptr<Sprite>>& destination,
		int64_t value,int32_t digitCount,const Vector2& position,const Vector2& digitDrawSize);
	// 細い線をジグザグにつなげ、電撃風の下線を作る。
	void AppendLightningUnderline(std::vector<std::unique_ptr<Sprite>>& destination,
		float startX,float endX,float baseY,const Vector4& color);
};

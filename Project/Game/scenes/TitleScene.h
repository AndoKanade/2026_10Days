#pragma once

#include "BaseScene.h"
#include "MyMath.h"
#include "Board.h" // 追加：タイトルの背景として描画するパズルの盤面
#include "FallingBlock.h" // 追加：背景のデモプレイで落とすブロック
#include "Difficulty.h" // 追加：タイトルで選ぶ難易度
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

// --- 前方宣言 ---
class Input;
class Obj3D;
class Obj3dCommon;
class Sprite;
class SpriteCommon;
class Skybox;       // 追加：背景の天球
class SkyboxCommon; // 追加：天球の共通設定（ルートシグネチャとPSO）

/// <summary>
/// タイトル画面シーン
/// </summary>
class TitleScene : public BaseScene{
public:
	// --- コンストラクタ・デストラクタ ---
	TitleScene();
	~TitleScene() override;

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

	// 変更：仮置きだったFenceのモデルとuvCheckerのスプライトを削除し、
	// 代わりにゲーム中と同じ盤面を背景として持つ。
	// タイトル名はこの盤面の上に重ねて描画する。
	Board board_;

	// 修正：コンフリクトを解消し、背景の天球とタイトルロゴの変数を両方残しました

	// --- 追加：ゲーム中と同じゲーミングな背景の天球 ---
	// 天球の共通設定はシーンごとに持つ（GameSceneも同じ持ち方をしている）。
	std::unique_ptr<SkyboxCommon> skyboxCommon_;
	std::unique_ptr<Skybox> skybox_;

	// 天球の回転角（ラジアン）。毎フレーム少しずつ足して背景を流す。
	float skyboxRotationY_ = 0.0f;

	// 明るさを脈打たせるための経過フレーム数
	int32_t skyboxPulseFrame_ = 0;

	// --- 追加：難易度の選択UI ---

	// 見出し「DIFFICULTY」
	std::unique_ptr<Sprite> difficultyLabel_;

	// 難易度の項目。並びは Difficulty の Easy / Normal / Hard に対応する。
	std::vector<std::unique_ptr<Sprite>> difficultySprites_;

	// 選択中の項目（Difficulty に対応する添字）
	int32_t difficultyIndex_ = 0;

	// 追加：タイトルの縦メニューでカーソルが指している項目。
	// Difficulty::Count 未満なら難易度の項目、Difficulty::Count ならオプションの項目を指す。
	int32_t menuIndex_ = 0;

	// 追加：カーソルがオプションの項目を指しているか
	bool IsOptionMenuFocused() const;

	// 選択状態に合わせて項目の色を塗り分け、行列を更新する
	void UpdateDifficultyUi();

	// --- 追加：オプション（音量設定）のUI ---

	// オプションで並べる音量の種類。並び順がそのまま画面の上からの並びになる。
	enum class OptionItem{
		Master,	// 全体の音量
		Bgm,	// BGMだけの音量
		Se,		// SEだけの音量
		Count,	// 項目の総数
	};

	// オプションを開いているか。開いている間はタイトルの操作を受け付けない。
	bool isOptionOpen_ = false;

	// 追加：難易度の項目の下に並べる「OPTION」のメニュー項目
	std::unique_ptr<Sprite> optionMenuItem_;

	// 選択中の項目（OptionItem に対応する添字）
	int32_t optionIndex_ = 0;

	// 背景を暗くする暗幕
	std::unique_ptr<Sprite> optionOverlay_;

	// 見出し「OPTION」
	std::unique_ptr<Sprite> optionHeader_;

	// 各項目のラベル。並びは OptionItem に対応する。
	std::vector<std::unique_ptr<Sprite>> optionLabels_;

	// 音量バーの下地（最大値ぶんの長さで固定）
	std::vector<std::unique_ptr<Sprite>> optionBarBacks_;

	// 音量バーの中身（現在の音量に応じて横幅を変える）
	std::vector<std::unique_ptr<Sprite>> optionBarFills_;

	// オプションの開閉と、音量の増減操作を処理する
	void UpdateOptionInput();

	// 選択状態と現在の音量に合わせて、色とバーの長さを更新する
	void UpdateOptionUi();

	// 指定した項目の音量を取得する
	float GetOptionVolume(int32_t index) const;

	// 指定した項目の音量を設定する
	void SetOptionVolume(int32_t index,float volume);

	// 追加：盤面の上に重ねて表示するタイトルロゴ（title.obj）
	std::unique_ptr<Obj3D> titleObj_;

	// 追加：タイトルロゴをふわふわ浮遊させるための経過フレーム数
	int32_t titleFloatTimer_ = 0;

	// --- メンバ変数：背景のデモプレイ ---
	// プレイヤーの代わりに、出現ごとに決めた回数だけ回転・左右移動してから落とす。

	// デモで落下中のブロック
	FallingBlock fallingBlock_;

	// 落下中ブロックの各マスを描画する3Dオブジェクト
	std::vector<std::unique_ptr<Obj3D>> fallingObjs_;

	// ブロックの種類と操作内容の抽選に使う乱数エンジン
	std::mt19937 randomEngine_;

	// 次に出現するブロックへ振る元ブロックID。出現のたびに増やす。
	int32_t nextBlockId_ = 0;

	// 次の操作までの残りフレーム数
	int32_t demoActionTimer_ = 0;

	// 今のブロックで、これから行う回転の残り回数
	int32_t demoPendingRotations_ = 0;

	// 今のブロックで、これから行う左右移動の残り回数（負なら左、正なら右）
	int32_t demoPendingMoves_ = 0;

private:
	// --- 内部処理：背景のデモプレイ ---

	// デモプレイを1フレーム進める（操作・落下・固定・積み上がり時のやり直し）
	void UpdateDemoPlay();

	// デモ用のブロックを1個出現させ、そのブロックで行う操作内容を決める。
	// 出現位置が塞がっていた場合は false を返す。
	bool SpawnDemoBlock();

	// 落下中ブロックの描画オブジェクトを、現在の占有マスと種類の色に合わせる
	void SyncDemoBlockObjs();

	// 天井まで積み上がったときに、盤面を空に戻してデモを最初からやり直す
	void ResetDemoPlay();
};
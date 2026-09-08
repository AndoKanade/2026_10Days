#pragma once

#include "systems/BaseScene.h"
#include "MyMath.h"
#include "LevelManager.h"
#include "Board.h" // 追加：パズルの盤面クラス
#include "FallingBlock.h" // 追加：落下中のブロック
#include <cstdint>
#include "SpecialGauge.h"
#include "SpecialSelector.h"
#include "ScoreSystem.h"
#include <memory>
#include <random>
#include <string>
#include <vector>

// 前方宣言
class Input;
class Obj3D;
class Obj3dCommon;
class SpriteCommon;
class Sprite;
class Application;

class GameScene : public BaseScene{
public:
	GameScene();
	~GameScene() override;

	// シーン管理
	void Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	// 外部依存
	Obj3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;
	Application* app_ = nullptr;

	// 設定・状態
	// 変更：ゲーム画面のBGMをサクラカゼに差し替え
	const std::string kBgmPath_ = "resource/music/bgm/サクラカゼ.mp3";
	// 削除：どこからも使われていなかった isPaused_ は、下のポーズ画面の項目へまとめた

	// レベル配置オブジェクト
	std::vector<std::shared_ptr<Obj3D>> levelObjects_;

	// レベルJSONの読み込み・更新監視マネージャー
	LevelManager levelManager_;

	// 追加：パズルの盤面（10×10の壁とマスを描画する）
	Board board_;

	// 追加：落下中のブロック（現状はT字のみ出現する）
	FallingBlock fallingBlock_;

	// 追加：落下中ブロックの各マスを描画する3Dオブジェクト
	std::vector<std::unique_ptr<Obj3D>> fallingObjs_;

	// 追加：着地予測（ゴースト。「ここに落とすとこうなる」の表示）の各マスを描画する3Dオブジェクト
	std::vector<std::unique_ptr<Obj3D>> ghostObjs_;

	// 追加：次に出現するブロックへ振る元ブロックID。出現のたびに増やす。
	int32_t nextBlockId_ = 0;

	// ネクストキュー（先読み表示するブロックの種類。先頭が次に出現する）
	std::vector<BlockShape::Type> nextQueue_;

	// 追加：ブロックの出現に使う袋。全種類を1個ずつ入れてシャッフルし、
	// 空になったら詰め直す。取り出すのは末尾から。
	std::vector<BlockShape::Type> blockBag_;

	// ホールド中のブロックの種類。hasHeldBlock_ が false の間は未使用。
	BlockShape::Type holdType_ = BlockShape::Type::T;

	// ホールド中のブロックがあるかどうか
	bool hasHeldBlock_ = false;

	// 今出現しているブロックについて、まだホールドを使っていないかどうか。
	// 1個のブロックにつきホールドは1回まで（固定されると再び使えるようになる）。
	bool canHold_ = true;

	// 追加：次ブロックの種類抽選に使う乱数エンジン（Initializeでシードする）
	std::mt19937 randomEngine_;

	// 追加：天井到達などでこれ以上ブロックを出せない状態かどうか
	bool isGameOver_ = false;

	// スペシャル選択・消去演出を除いた、落下速度上昇用の経過時間。
	int64_t activePlayFrames_ = 0;
	bool debugManualFallSpeed_ = false;
	int debugFallIntervalFrames_ = PuzzleConfig::kFallIntervalFrames;
	int32_t GetCurrentFallInterval() const{
		return debugManualFallSpeed_ ? debugFallIntervalFrames_ :
			PuzzleConfig::GetFallIntervalFrames(activePlayFrames_);
	}

	// スペシャル発動に使用するゲージ
	SpecialGauge specialGauge_;
	// スペシャルによって始まった消去・連鎖では、ゲージを自己充電させない。
	bool suppressSpecialClearCharge_ = false;
	ScoreSystem score_;

	// スペシャルで最強マスにする対象の選択状態
	SpecialSelector specialSelector_;

	// 対象選択中のマスを示すカーソル
	std::unique_ptr<Obj3D> specialCursorObj_;

	// Releaseでも表示するスペシャルゲージ。背景・チャージ・満タン・発動中の4層。
	std::unique_ptr<Sprite> specialGaugeBackgroundSprite_;
	std::unique_ptr<Sprite> specialGaugeChargeSprite_;
	std::unique_ptr<Sprite> specialGaugeReadySprite_;
	std::unique_ptr<Sprite> specialGaugeActiveSprite_;
	int32_t specialGaugeRainbowFrame_ = 0;

	// Releaseでも表示するゲーム中スコア（数字画像を1文字ずつ切り出す）。
	std::vector<std::unique_ptr<Sprite>> scoreDigitSprites_;

	// --- 追加：ポーズ画面 ---

	// ポーズ中に表示する画面の種類
	enum class PauseMode{
		Menu,     // 項目を選ぶメニュー
		Tutorial, // チュートリアル（内容はこれから追加する）
	};

	// ポーズメニューの項目。並び順がそのまま表示順になる。
	enum class PauseMenuItem{
		Restart,  // 最初からやり直す
		Title,    // タイトルへ戻る
		Tutorial, // チュートリアルを開く
		Count,    // 項目数（末尾に置くこと）
	};

	// ポーズ中かどうか
	bool isPaused_ = false;

	// ポーズ中に表示している画面
	PauseMode pauseMode_ = PauseMode::Menu;

	// 選択中の項目（PauseMenuItem に対応する添字）
	int32_t pauseMenuIndex_ = 0;

	// 画面全体を暗くする暗幕
	std::unique_ptr<Sprite> pauseOverlaySprite_;

	// ポーズ画面の見出し
	std::unique_ptr<Sprite> pauseHeaderSprite_;

	// メニュー項目のラベル。並びは PauseMenuItem に対応する。
	std::vector<std::unique_ptr<Sprite>> pauseMenuSprites_;

	// チュートリアル画面の見出し
	std::unique_ptr<Sprite> pauseTutorialHeaderSprite_;

	// ImGuiから消去結果を再現するための入力値
	int32_t debugClearedCellCount_ = 3;
	int32_t debugChainCount_ = 1;

	// ネクストキュー各枠のプレビュー用3Dオブジェクト（枠ごとに最大4マス）
	std::vector<std::vector<std::unique_ptr<Obj3D>>> nextPreviewObjs_;

	// ホールド枠のプレビュー用3Dオブジェクト（未ホールド時は空）
	std::vector<std::unique_ptr<Obj3D>> holdPreviewObjs_;

	// 追加：NEXT/HOLDの見出しラベル（character/next.obj, character/hold.obj）
	std::unique_ptr<Obj3D> nextLabelObj_;
	std::unique_ptr<Obj3D> holdLabelObj_;

	// レベル配置データからオブジェクトを再構築する
	void RebuildLevelObjects();

	// 追加：落下中ブロックの描画オブジェクトの位置を、現在の占有マスに合わせる
	void SyncFallingObjs();

	// ネクストキューの先頭のブロックを出現させ、キューの末尾に新しい種類を補充する。
	// 出現できなかった場合は false を返す（天井まで積み上がった＝ゲームオーバー）。
	bool SpawnNextBlock();

	// ネクストキューを規定個数（PuzzleConfig::kNextQueueSize）ぶん抽選して満たす。
	void FillNextQueue();

	// 追加：次に落ちてくるブロックの種類をひとつ抽選して返す。
	BlockShape::Type PickNextBlockType();

	// 追加：ブロックの袋に全種類を1個ずつ詰め直し、取り出す順番をシャッフルする。
	void RefillBlockBag();

	// ホールド操作。今のブロックをホールドへ預け、代わりにホールド済みの
	// ブロック（未ホールドならネクスト先頭のブロック）を出現させる。
	// ホールド済みで今のブロックにまだホールドを使っていない場合のみ実行する。
	// 出現できなかった場合は false を返す（ゲームオーバー）。ホールドを使わなかった
	// （既に使用済みだった）場合は true を返し、何もしない。
	bool SwapHold();

	// ネクストキュー・ホールドのプレビュー用3Dオブジェクトを、現在の中身に合わせて作り直す。
	// キューやホールドの中身が変わったタイミングでのみ呼べばよい（毎フレーム呼ぶ必要はない）。
	void RebuildPreviewObjs();

	// 1個ぶんのブロックのプレビュー（形なりに並べた3Dオブジェクト）を
	// 指定した配列に構築する。anchorX・anchorYは形の基準マス(0,0)を置く盤面マス座標
	// （盤面範囲外の値を渡してよい。Board::GridToWorldは範囲外でも計算できる）。
	// extraYOffsetWorldは、マス目盛りでは表せない微調整用のワールド単位の下方向オフセット。
	void BuildPreviewShape(std::vector<std::unique_ptr<Obj3D>>& objs,BlockShape::Type type,int32_t anchorX,int32_t anchorY,const Vector4& color,float extraYOffsetWorld = 0.0f);

	// 追加：NEXT/HOLDラベルの位置を、現在の盤面幅に合わせたプレビューの配置に合わせて更新する。
	void SyncPreviewLabels();

	// ImGui にネクストキュー・ホールドを表示する。
	void ShowNextBlockGui() const;

	// スペシャル選択カーソルの位置と色を現在の対象に合わせる
	void SyncSpecialCursor();
	// EnterまたはデバッグUIから共通の決定処理を呼ぶ。
	void ConfirmSpecialTarget();
	void UpdateSpecialGaugeUi();
	void UpdateScoreUi();

	// --- 追加：ポーズ画面 ---

	// ポーズ画面のスプライトを生成して初期配置する（Initializeから呼ぶ）
	void InitializePauseUi();

	// ポーズ中の入力を受け付け、UIを更新する
	void UpdatePauseMenu();

	// ポーズ画面のスプライトを、いまの選択状態に合わせて更新する
	void UpdatePauseUi();

	// 選択中の項目を決定したときの処理
	void ConfirmPauseMenuItem();

	// ポーズ画面を描画する（ほかのUIより手前に重ねる）
	void DrawPause();

	// レベル配置オブジェクトを描画するかどうか（ImGuiで切り替え）
	bool isLevelObjectsVisible_ = false;
};

#pragma once

#include "systems/BaseScene.h"
#include "MyMath.h"
#include "LevelManager.h"
#include "Board.h" // 追加：パズルの盤面クラス
#include "FallingBlock.h" // 追加：落下中のブロック
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

// 前方宣言
class Input;
class Obj3D;
class Obj3dCommon;
class SpriteCommon;
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
	const std::string kBgmPath_ = "resource/You_and_Me.mp3";
	bool isPaused_ = false;

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

	// ネクストキュー各枠のプレビュー用3Dオブジェクト（枠ごとに最大4マス）
	std::vector<std::vector<std::unique_ptr<Obj3D>>> nextPreviewObjs_;

	// ホールド枠のプレビュー用3Dオブジェクト（未ホールド時は空）
	std::vector<std::unique_ptr<Obj3D>> holdPreviewObjs_;

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
	void BuildPreviewShape(std::vector<std::unique_ptr<Obj3D>>& objs,BlockShape::Type type,int32_t anchorX,int32_t anchorY,const Vector4& color);

	// ImGui にネクストキュー・ホールドを表示する。
	void ShowNextBlockGui() const;

	// レベル配置オブジェクトを描画するかどうか（ImGuiで切り替え）
	bool isLevelObjectsVisible_ = false;
};
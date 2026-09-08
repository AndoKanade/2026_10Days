#pragma once

#include "BaseScene.h"
#include "MyMath.h"
#include "Board.h" // 追加：タイトルの背景として描画するパズルの盤面
#include "FallingBlock.h" // 追加：背景のデモプレイで落とすブロック
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
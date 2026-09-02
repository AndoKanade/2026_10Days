#pragma once

#include <array>
#include <memory>
#include <vector>
#include <cstdint>

#include "MyMath.h"
#include "Cell.h"
#include "GridPos.h" // 追加：マス座標の共通型
#include "PuzzleConfig.h"

// 前方宣言
class Obj3dCommon;
class Obj3D;

// =============================================================================
// 盤面クラス
// 10×10のマス配列（データ）を保持し、外周をU字に囲む壁の描画を担当する。
// 見た目は2Dだが、描画は3Dモデル（defaultBlock のキューブ）を並べて表現する。
// 空きマスは描画しない。マスの中身の描画はブロック実装時に追加する。
// ブロックの落下や通電などのゲームロジックはまだ持たない。
// =============================================================================
class Board{
public:

	// 前方宣言した Obj3D を unique_ptr で持つため、コンストラクタ・デストラクタは cpp 側で定義する
	Board();
	~Board();

	// モデルの読み込みとオブジェクト生成などの初期化
	void Initialize(Obj3dCommon* object3dCommon);

	// 毎フレームの更新
	void Update();

	// 盤面（U字の壁）の描画
	void Draw();

	// --- ゲッター ---

	// 現在有効な盤面の幅（マス数）
	int32_t GetWidth() const{ return width_; }

	// 盤面の高さ（マス数）
	int32_t GetHeight() const{ return PuzzleConfig::kBoardHeight; }

	// 指定マスのデータを取得する（範囲チェックは呼び出し側の責任）
	const Cell& GetCell(int32_t x,int32_t y) const{ return cells_[y][x]; }

	// --- ブロック配置（3人分担の境界インターフェース。担当Aが中身を実装する）---

	// 追加：指定したマス群すべてが盤面内かつ空きなら配置可能とみなす。
	// 現状はスタブ。担当Aが実装するまで常に false を返す。
	bool CanPlace(const std::vector<GridPos>& cells) const;

	// 追加：指定したマス群へ blockId と端子ビットを書き込んで盤面に固定する。
	// terminals は cells と同じ並び・同じ要素数を想定する（対応する要素が無いマスは0のまま）。
	// 固定と同時に通電判定を行い、ゴールまで繋がっていれば対象マスを消す。
	void Place(const std::vector<GridPos>& cells,int32_t blockId,const std::vector<uint8_t>& terminals);

	// --- 判定・座標変換 ---

	// 指定マス座標が現在の盤面の範囲内かどうか
	bool IsInside(int32_t x,int32_t y) const;

	// 盤面のマス座標を、そのマスの中心のワールド座標に変換する
	Vector3 GridToWorld(int32_t x,int32_t y) const;

	// --- セッター ---

	// 盤面の幅を切り替える（6と10の切り替え用。デバッグUIから呼ぶ想定）
	void SetWidth(int32_t width);

	// 追加：消去演出中かどうか。true の間は呼び出し側でブロックの操作・落下を止める想定。
	bool IsBusy() const{ return isClearing_; }

private:

	// 借りてくるポインタ（このクラスでは生成・解放しない）
	Obj3dCommon* object3dCommon_ = nullptr;

	// 盤面データ。幅は最大値で確保し、実際に使う範囲を width_ で制御する。
	std::array<std::array<Cell,PuzzleConfig::kBoardWidthMax>,PuzzleConfig::kBoardHeight> cells_{};

	// 現在有効な盤面の幅（マス数）
	int32_t width_ = PuzzleConfig::kBoardWidth;

	// 盤面の外周をU字（左・下・右、上は開き）に囲む壁ブロックのオブジェクト
	std::vector<std::unique_ptr<Obj3D>> wallObjs_;

	// 追加：盤面に固定されたマスの見た目を表す3Dオブジェクト
	std::vector<std::unique_ptr<Obj3D>> cellObjs_;

	// 追加：消去演出中に、消える予定として保持しているマス
	std::vector<GridPos> clearingCells_;

	// 追加：消去演出中かどうか
	bool isClearing_ = false;

	// 追加：消去演出の経過フレーム数
	int32_t clearTimer_ = 0;

	// U字の壁ブロックを1個生成して wallObjs_ に追加する
	void CreateWallBlock(int32_t x,int32_t y);

	// 追加：cells_ の埋まっているマスから cellObjs_ を作り直す
	void RebuildCellObjects();

	// 追加：指定マスが現在消去演出中かどうか
	bool IsClearingCell(int32_t x,int32_t y) const;

	// 追加：電源（最下段）から実際にどこまで通電が届いているかを幅優先探索で調べる。
	// ゴールに届いているかは問わない。見た目のハイライト（2.6の常時可視化）に使う。
	std::array<std::array<bool,PuzzleConfig::kBoardWidthMax>,PuzzleConfig::kBoardHeight> ComputePoweredMask() const;

	// 追加：電源（最下段）から幅優先探索で通電範囲を調べ、ゴール（左右端）まで
	// 繋がっていれば、その範囲のマスを消去演出の対象にする（この時点ではまだ消さない）。
	void ResolveConduction();

	// 追加：空きマスを詰めるように、各列のマスをマス単位で下へ落とす
	void ApplyGravity();
};

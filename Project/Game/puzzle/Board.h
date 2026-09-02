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

	// 追加：指定したマス群へ blockId を書き込んで盤面に固定する。
	// 現状はスタブ。担当Aが実装するまで何もしない。
	void Place(const std::vector<GridPos>& cells,int32_t blockId);

	// 追加：落下中ブロック用の配置可否判定。
	// 天井より上（y < 0）は空中とみなして通す。左右の壁・床・既存ブロックとの重なりのみ不可とする。
	bool CanFall(const std::vector<GridPos>& cells) const;

	// --- 判定・座標変換 ---

	// 指定マス座標が現在の盤面の範囲内かどうか
	bool IsInside(int32_t x,int32_t y) const;

	// 盤面のマス座標を、そのマスの中心のワールド座標に変換する
	Vector3 GridToWorld(int32_t x,int32_t y) const;

	// --- セッター ---

	// 盤面の幅を切り替える（6と10の切り替え用。デバッグUIから呼ぶ想定）
	void SetWidth(int32_t width);

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

	// U字の壁ブロックを1個生成して wallObjs_ に追加する
	void CreateWallBlock(int32_t x,int32_t y);

	// 追加：cells_ の埋まっているマスから cellObjs_ を作り直す
	void RebuildCellObjects();
};

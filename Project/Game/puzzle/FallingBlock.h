#pragma once

#include <cstdint>
#include <vector>

#include "GridPos.h"
#include "BlockShape.h"
#include "PuzzleConfig.h"

// 前方宣言
class Board;

// =============================================================================
// 落下中のブロック
// 基準座標＋回転indexを持ち、自動落下・左右移動・回転（衝突時は拒否）・
// 次ブロックの出現を担当する。
//
// 担当B（BlockShape・FallingBlock）が中身を実装する。
// =============================================================================
class FallingBlock{
public:

	FallingBlock() = default;
	~FallingBlock() = default;

	// 追加：指定した種類・元ブロックIDで、出現列の最上段にブロックを出現させる。
	// 出現位置が既に埋まっていて置けない場合は false を返す（＝ゲームオーバー）。
	bool Spawn(const Board& board,BlockShape::Type type,int32_t blockId);

	// 追加：自動落下と固定猶予の時間経過を1フレーム分進める。
	// このフレームで盤面に固定されたら true を返す（呼び出し側が次ブロックを出す）。
	bool Update(Board& board);

	// 落下可能な最下段まで移動し、即座に盤面へ固定する。
	bool HardDrop(Board& board);

	// 追加：左に1マス移動する。移動先が壁や既存ブロックと重なる場合は動かさない。
	void MoveLeft(const Board& board);

	// 追加：右に1マス移動する。移動先が壁や既存ブロックと重なる場合は動かさない。
	void MoveRight(const Board& board);

	// 追加：時計回りに1段階回転する。回転後が壁や既存ブロックと重なる場合は回転しない。
	void Rotate(const Board& board);

	// 追加：下キーによる加速落下の入り切りを設定する。
	void SetSoftDrop(bool enable){ isSoftDrop_ = enable; }

	// 現在このブロックが占めている盤面マスの絶対座標を返す。
	// Board::CanPlace / Board::Place にそのまま渡せる形にする。
	std::vector<GridPos> GetOccupiedCells() const;

	// 追加：いまこの瞬間に真下までまっすぐ落とした場合に着地する位置の占有マスを返す。
	// 「ここに落とすとこうなる」を見せるゴースト表示に使う。盤面自体は変更しない。
	std::vector<GridPos> GetLandingCells(const Board& board) const;

	// 追加：描画の色分けなどに使うブロックの種類。
	BlockShape::Type GetType() const{ return type_; }

	// 追加：直前の Update() で、天井より上（y < 0）にマスを残したまま固定されたかどうか。
	// Update() が true を返した直後に参照する。true ならゲームオーバー。
	bool IsLockedAboveCeiling() const{ return lockedAboveCeiling_; }

	// このブロックの元ブロックID。盤面に固定するときに各マスへ書き込む。
	int32_t GetBlockId() const;

private:

	// 追加：指定した基準座標・回転でブロックが占める盤面マスの絶対座標を計算する。
	std::vector<GridPos> CalcCells(GridPos origin,int32_t rotation) const;

	// 追加：固定したマス群の中に天井より上（y < 0）のものが含まれていれば
	// lockedAboveCeiling_ を立てる。通常落下・ハードドロップ両方の固定時に呼ぶ。
	void UpdateLockedAboveCeiling(const std::vector<GridPos>& lockedCells);

	// このブロックの種類（初期値は仮）
	BlockShape::Type type_ = BlockShape::Type::L;

	// 回転index（0〜BlockShape::kRotationCount-1）
	int32_t rotation_ = 0;

	// 基準となる盤面マス座標
	GridPos origin_{};

	// このブロックの元ブロックID
	int32_t blockId_ = PuzzleConfig::kEmptyBlockId;

	// 追加：自動落下の経過フレーム数
	int32_t fallTimer_ = 0;

	// 追加：着地してから盤面に固定されるまでの猶予の経過フレーム数
	int32_t lockTimer_ = 0;

	// 追加：下キーによる加速落下中かどうか
	bool isSoftDrop_ = false;

	// 追加：天井より上にマスを残したまま固定されたかどうか
	bool lockedAboveCeiling_ = false;
};

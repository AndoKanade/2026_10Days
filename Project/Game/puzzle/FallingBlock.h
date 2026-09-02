#pragma once

#include <cstdint>
#include <vector>

#include "GridPos.h"
#include "BlockShape.h"
#include "PuzzleConfig.h"

// =============================================================================
// 落下中のブロック
// 基準座標＋回転indexを持ち、自動落下・左右移動・回転（衝突時は拒否）・
// 次ブロックの出現を担当する。
//
// 担当B（BlockShape・FallingBlock）が中身を実装する。
// ここは3人分担の境界インターフェースとして、外から使う関数の宣言のみ確定させた状態。
// =============================================================================
class FallingBlock{
public:

	FallingBlock() = default;
	~FallingBlock() = default;

	// 現在このブロックが占めている盤面マスの絶対座標を返す。
	// Board::CanPlace / Board::Place にそのまま渡せる形にする。
	// 現状はスタブ。担当Bが実装するまで空の配列を返す。
	std::vector<GridPos> GetOccupiedCells() const;

	// このブロックの元ブロックID。盤面に固定するときに各マスへ書き込む。
	// 現状はスタブ。担当Bが採番の仕組みを実装する。
	int32_t GetBlockId() const;

private:

	// このブロックの種類（初期値は仮）
	BlockShape::Type type_ = BlockShape::Type::L;

	// 回転index（0〜BlockShape::kRotationCount-1）
	int32_t rotation_ = 0;

	// 基準となる盤面マス座標
	GridPos origin_{};

	// このブロックの元ブロックID
	int32_t blockId_ = PuzzleConfig::kEmptyBlockId;
};

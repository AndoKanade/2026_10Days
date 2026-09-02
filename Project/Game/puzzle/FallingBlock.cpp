#include "FallingBlock.h"

// 現在このブロックが占めている盤面マスの絶対座標を返す。
// 現状はスタブ。常に空の配列を返す。
std::vector<GridPos> FallingBlock::GetOccupiedCells() const{
	// 担当B実装予定：BlockShape::GetCells(type_, rotation_) の各相対座標に
	// origin_ を足した絶対座標を返す
	return {};
}

// このブロックの元ブロックID。
// 現状はスタブ。初期値をそのまま返す。
int32_t FallingBlock::GetBlockId() const{
	return blockId_;
}

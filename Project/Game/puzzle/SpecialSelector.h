#pragma once

#include <cstdint>

#include "GridPos.h"

class Board;

// =============================================================================
// スペシャルで最強マスへ変換する対象座標の選択状態を管理する
// =============================================================================
class SpecialSelector{
public:
	// 固定済みのマスが存在するときだけ選択を開始する
	bool Begin(const Board& board);

	// カーソルを盤面内で移動する
	void Move(int32_t deltaX,int32_t deltaY,const Board& board);

	// 選択を終了する。ゲージの消費は行わない
	void Cancel();

	// 現在の対象が固定済みのマスか
	bool CanConfirm(const Board& board) const;

	bool IsSelecting() const{ return isSelecting_; }
	GridPos GetTarget() const{ return target_; }

private:
	bool isSelecting_ = false;
	GridPos target_{};
};

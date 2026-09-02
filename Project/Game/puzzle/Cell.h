#pragma once

#include <cstdint>

#include "PuzzleConfig.h"

// =============================================================================
// 盤面の1マス分のデータ
// =============================================================================
struct Cell{

	// このマスを埋めている元ブロックのID。
	// 空きマスのときは PuzzleConfig::kEmptyBlockId が入る。
	int32_t blockId = PuzzleConfig::kEmptyBlockId;

	// 上下左右の端子の有無を表す4bit（Day2の配線描画・通電判定で使用する）。
	// ビットの割り当ては配線の実装時に決めるため、今はまだ使わない。
	uint8_t terminals = 0;

	// このマスが空かどうか
	bool IsEmpty() const{
		return blockId == PuzzleConfig::kEmptyBlockId;
	}
};

#pragma once

#include <cstdint>

#include "PuzzleConfig.h"

// =============================================================================
// マスの端子ビット（上下左右）
// Cell::terminals はこのビットをORした値で持つ。
// 隣接する2マスが繋がる条件は、互いに向き合う辺のビットが両方立っていること。
// =============================================================================
namespace Terminal{
	constexpr uint8_t kUp    = 1 << 0; // 上辺の端子
	constexpr uint8_t kDown  = 1 << 1; // 下辺の端子
	constexpr uint8_t kLeft  = 1 << 2; // 左辺の端子
	constexpr uint8_t kRight = 1 << 3; // 右辺の端子
}

// =============================================================================
// 盤面の1マス分のデータ
// =============================================================================
struct Cell{

	// このマスを埋めている元ブロックのID。
	// 空きマスのときは PuzzleConfig::kEmptyBlockId が入る。
	int32_t blockId = PuzzleConfig::kEmptyBlockId;

	// 変更：上下左右の端子の有無を表す4bit（Terminal::の値をORして持つ）。
	// ブロック配置時に BlockShape::GetTerminals() の値を書き込む。
	// ブロック同士（マス同士）の通電判定で使用する。先端は反対側も露出しているため、
	// 別ブロックの先端同士が向き合えば繋がる。
	uint8_t terminals = 0;

	// 追加：壁（ゴール）・床（電源）に実際に届くかどうかの判定専用の端子ビット。
	// BlockShape::GetWallTerminals() の値を書き込む。ブロック同士の接続には使わない
	// （先端露出を含まない、形ごとに定めた「本来の先端」だけのビット）。
	uint8_t wallTerminals = 0;

	// このマスが空かどうか
	bool IsEmpty() const{
		return blockId == PuzzleConfig::kEmptyBlockId;
	}
};

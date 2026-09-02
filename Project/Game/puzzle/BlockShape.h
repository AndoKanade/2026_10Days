#pragma once

#include <cstdint>
#include <vector>

#include "GridPos.h"

// =============================================================================
// ブロックの形（配線の形）の定義
// 形ごとに4回転分のマス相対座標テーブルを固定データで持つ。
// ランダムな割り当てはしない。形と配線は常に一致する。
//
// 担当B（BlockShape・FallingBlock）が中身を実装する。
// ここは3人分担の境界インターフェースとして、型と関数の宣言のみ確定させた状態。
// =============================================================================
namespace BlockShape{

	// ブロックの種類。初期実装は L字・T字の2種類。
	enum class Type{
		L,
		T,
	};

	// 回転の総数（0〜3の4方向）
	constexpr int32_t kRotationCount = 4;

	// 指定した種類・回転のブロックが占めるマスの相対座標を返す。
	// 相対座標の基準（原点）の取り方は担当Bが決める。
	// rotation は 0〜kRotationCount-1 を想定する。
	//
	// 現状はスタブ。担当Bが形テーブルを実装するまで空の配列を返す。
	const std::vector<GridPos>& GetCells(Type type,int32_t rotation);
}

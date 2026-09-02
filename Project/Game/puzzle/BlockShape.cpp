#include "BlockShape.h"

#include <array>

#include "Cell.h"

// このファイル内だけで使う定数・データ
namespace{

	// まだ形テーブルを実装していない種類のために返す空の配列。
	const std::vector<GridPos> kEmptyCells{};

	// 追加：T字ブロックの4回転分のマス相対座標テーブル。
	// 基準(0,0)は横棒の中央マス。回転してもこのマスが動かないため、回転の見た目が安定する。
	// rotation はここでは時計回り。
	//   rot0：上に出っ張り   rot1：右に出っ張り   rot2：下に出っ張り   rot3：左に出っ張り
	// x は右方向、y は下方向が正（GridPos と同じ向き）。
	const std::array<std::vector<GridPos>,BlockShape::kRotationCount> kTShapeCells = {{
		// rot0： .X.
		//        XXX
		{ {-1, 0}, {0, 0}, {1, 0}, {0, -1} },
		// rot1： X.
		//        XX
		//        X.
		{ {0, -1}, {0, 0}, {0, 1}, {1, 0} },
		// rot2： XXX
		//        .X.
		{ {-1, 0}, {0, 0}, {1, 0}, {0, 1} },
		// rot3： .X
		//        XX
		//        .X
		{ {0, -1}, {0, 0}, {0, 1}, {-1, 0} },
	}};

	// 追加：L字ブロック（Lテトロミノ・4マス）の4回転分のマス相対座標テーブル。
	// 基準(0,0)は縦棒／横棒の中央マス。rot0 から時計回りに回転する。
	//   rot0：縦棒＋右下の足   rot1：横棒＋左下の足
	//   rot2：縦棒＋左上の足   rot3：横棒＋右上の足
	const std::array<std::vector<GridPos>,BlockShape::kRotationCount> kLShapeCells = {{
		// rot0： X.
		//        X.
		//        XX
		{ {0, -1}, {0, 0}, {0, 1}, {1, 1} },
		// rot1： XXX
		//        X..
		{ {-1, 0}, {0, 0}, {1, 0}, {-1, 1} },
		// rot2： XX
		//        .X
		//        .X
		{ {0, -1}, {0, 0}, {0, 1}, {-1, -1} },
		// rot3： ..X
		//        XXX
		{ {-1, 0}, {0, 0}, {1, 0}, {1, -1} },
	}};
}

namespace BlockShape{

	// 指定した種類・回転のブロックが占めるマスの相対座標を返す。
	const std::vector<GridPos>& GetCells(Type type,int32_t rotation){
		// 追加：回転indexを 0〜kRotationCount-1 の範囲に丸める（負の値にも対応する）
		const int32_t r = ((rotation % kRotationCount) + kRotationCount) % kRotationCount;

		switch(type){
		case Type::T:
			return kTShapeCells[r];
		case Type::L:
			return kLShapeCells[r];
		}

		return kEmptyCells;
	}

	// 指定した種類・回転のブロックの各マスの端子ビットを返す。
	// ブロック同士は向き・形に関係なく常に導通する仕様のため、
	// 実際に置かれるマス数ぶん、全方向の端子ビットを返す。
	std::vector<uint8_t> GetTerminals(Type type,int32_t rotation){
		const size_t cellCount = GetCells(type,rotation).size();
		constexpr uint8_t kAllDirections = Terminal::kUp | Terminal::kDown | Terminal::kLeft | Terminal::kRight;

		return std::vector<uint8_t>(cellCount,kAllDirections);
	}
}

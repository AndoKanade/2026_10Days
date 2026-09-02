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

	// 追加：まだ端子テーブルを持たない種類のために返す空の配列。
	const std::vector<uint8_t> kEmptyTerminals{};

	// 追加：ブロック内で隣接するマス同士を見て、各マスの内部配線ビットを計算する。
	// 形そのものが配線になる仕様のため、端子専用のテーブルは持たずここから毎回計算する。
	std::vector<uint8_t> ComputeTerminals(const std::vector<GridPos>& cells){
		std::vector<uint8_t> terminals(cells.size(),0);

		for(size_t i = 0; i < cells.size(); ++i){
			for(size_t j = 0; j < cells.size(); ++j){
				if(i == j){
					continue;
				}

				const int32_t dx = cells[j].x - cells[i].x;
				const int32_t dy = cells[j].y - cells[i].y;

				// 上下左右いずれかに隣接するマスがあれば、その向きの端子ビットを立てる
				if(dx == 0 && dy == -1){
					terminals[i] |= Terminal::kUp;
				} else if(dx == 0 && dy == 1){
					terminals[i] |= Terminal::kDown;
				} else if(dx == -1 && dy == 0){
					terminals[i] |= Terminal::kLeft;
				} else if(dx == 1 && dy == 0){
					terminals[i] |= Terminal::kRight;
				}
			}
		}

		// 追加：ブロック内の繋がりが1方向だけのマス（先端）は、
		// 反対側の辺にも配線を露出させる。これが無いと、先端は常に
		// 自分のブロックの中心側だけを向いてしまい、隣に置いた別ブロックと
		// 絶対に繋がる余地が無くなってしまう。
		for(size_t i = 0; i < cells.size(); ++i){
			const uint8_t bits = terminals[i];

			// 立っているビット数（＝ブロック内での隣接数）を数える
			int32_t degree = 0;
			if(bits & Terminal::kUp)    ++degree;
			if(bits & Terminal::kDown)  ++degree;
			if(bits & Terminal::kLeft)  ++degree;
			if(bits & Terminal::kRight) ++degree;

			if(degree == 1){
				// 唯一繋がっている方向の、真逆の辺を露出させる
				if(bits & Terminal::kUp){
					terminals[i] |= Terminal::kDown;
				} else if(bits & Terminal::kDown){
					terminals[i] |= Terminal::kUp;
				} else if(bits & Terminal::kLeft){
					terminals[i] |= Terminal::kRight;
				} else if(bits & Terminal::kRight){
					terminals[i] |= Terminal::kLeft;
				}
			}
		}

		return terminals;
	}

	// 追加：T字・L字の4回転分の端子ビットテーブル。各形の Cells から計算する。
	const std::array<std::vector<uint8_t>,BlockShape::kRotationCount> kTShapeTerminals = {
		ComputeTerminals(kTShapeCells[0]),
		ComputeTerminals(kTShapeCells[1]),
		ComputeTerminals(kTShapeCells[2]),
		ComputeTerminals(kTShapeCells[3]),
	};
	const std::array<std::vector<uint8_t>,BlockShape::kRotationCount> kLShapeTerminals = {
		ComputeTerminals(kLShapeCells[0]),
		ComputeTerminals(kLShapeCells[1]),
		ComputeTerminals(kLShapeCells[2]),
		ComputeTerminals(kLShapeCells[3]),
	};
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
	const std::vector<uint8_t>& GetTerminals(Type type,int32_t rotation){
		// GetCells()と同じ丸め方で回転indexを 0〜kRotationCount-1 の範囲に収める
		const int32_t r = ((rotation % kRotationCount) + kRotationCount) % kRotationCount;

		switch(type){
		case Type::T:
			return kTShapeTerminals[r];
		case Type::L:
			return kLShapeTerminals[r];
		}

		return kEmptyTerminals;
	}
}

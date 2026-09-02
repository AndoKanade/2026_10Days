#include "BlockShape.h"

// このファイル内だけで使う定数・データ
namespace{

	// スタブ実装で返す空の配列。
	// 担当Bが形テーブルを実装したらこれは不要になる。
	const std::vector<GridPos> kEmptyCells{};
}

namespace BlockShape{

	// 指定した種類・回転のブロックが占めるマスの相対座標を返す。
	// 現状はスタブ。常に空の配列を返す。
	const std::vector<GridPos>& GetCells(Type type,int32_t rotation){
		// 担当B実装予定：type と rotation から固定の形テーブルを引いて返す
		(void)type;
		(void)rotation;
		return kEmptyCells;
	}
}

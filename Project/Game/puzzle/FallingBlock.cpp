#include "FallingBlock.h"

#include "Board.h"

// 追加：指定した基準座標・回転でブロックが占める盤面マスの絶対座標を計算する。
std::vector<GridPos> FallingBlock::CalcCells(GridPos origin,int32_t rotation) const{
	// 形テーブルから相対座標の並びを取得する
	const std::vector<GridPos>& shape = BlockShape::GetCells(type_,rotation);

	std::vector<GridPos> result;
	result.reserve(shape.size());

	// 各相対座標に基準座標を足して絶対座標にする
	for(const GridPos& relative : shape){
		result.push_back({origin.x + relative.x, origin.y + relative.y});
	}

	return result;
}

// 追加：指定した種類・元ブロックIDで、出現列の最上段にブロックを出現させる。
bool FallingBlock::Spawn(const Board& board,BlockShape::Type type,int32_t blockId){
	type_ = type;
	rotation_ = 0;
	blockId_ = blockId;

	// タイマー類を初期化する
	fallTimer_ = 0;
	lockTimer_ = 0;
	isSoftDrop_ = false;

	// 出現位置。T字の基準(0,0)は横棒の中央マスで、rot0では1マス上に出っ張りがあるため、
	// 出っ張りが盤面からはみ出さないよう y は 1 から始める。
	origin_ = {PuzzleConfig::kSpawnColumn, 1};

	// 出現位置が既に埋まっていれば置けない（ゲームオーバー）
	return board.CanPlace(GetOccupiedCells());
}

// 追加：自動落下と固定猶予の時間経過を1フレーム分進める。
bool FallingBlock::Update(Board& board){
	// 真下に1マス動けるかどうか
	const GridPos down = {origin_.x, origin_.y + 1};
	const bool canFall = board.CanPlace(CalcCells(down,rotation_));

	if(canFall){
		// 空中にいる間は固定猶予をリセットしておく
		lockTimer_ = 0;

		// 加速中かどうかで落下間隔を切り替える
		const int32_t fallInterval = isSoftDrop_ ? PuzzleConfig::kFallIntervalFramesFast : PuzzleConfig::kFallIntervalFrames;

		++fallTimer_;
		if(fallTimer_ >= fallInterval){
			fallTimer_ = 0;
			origin_ = down;
		}
		return false;
	}

	// 着地している。固定猶予を毎フレーム進める
	fallTimer_ = 0;
	++lockTimer_;
	if(lockTimer_ < PuzzleConfig::kLockDelayFrames){
		return false;
	}

	// 猶予を使い切ったので盤面に固定する
	board.Place(GetOccupiedCells(),blockId_);
	return true;
}

// 追加：左に1マス移動する。
void FallingBlock::MoveLeft(const Board& board){
	const GridPos moved = {origin_.x - 1, origin_.y};
	if(board.CanPlace(CalcCells(moved,rotation_))){
		origin_ = moved;
		// 動かせたら固定猶予をリセットする（着地際の操作を受け付けるため）
		lockTimer_ = 0;
	}
}

// 追加：右に1マス移動する。
void FallingBlock::MoveRight(const Board& board){
	const GridPos moved = {origin_.x + 1, origin_.y};
	if(board.CanPlace(CalcCells(moved,rotation_))){
		origin_ = moved;
		lockTimer_ = 0;
	}
}

// 追加：時計回りに1段階回転する。重なる場合は回転しない（押し戻しはしない）。
void FallingBlock::Rotate(const Board& board){
	const int32_t nextRotation = (rotation_ + 1) % BlockShape::kRotationCount;
	if(board.CanPlace(CalcCells(origin_,nextRotation))){
		rotation_ = nextRotation;
		lockTimer_ = 0;
	}
}

// 現在このブロックが占めている盤面マスの絶対座標を返す。
std::vector<GridPos> FallingBlock::GetOccupiedCells() const{
	return CalcCells(origin_,rotation_);
}

// このブロックの元ブロックID。
int32_t FallingBlock::GetBlockId() const{
	return blockId_;
}

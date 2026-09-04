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

	// タイマー・フラグ類を初期化する
	fallTimer_ = 0;
	lockTimer_ = 0;
	isSoftDrop_ = false;
	lockedAboveCeiling_ = false;

	// 出現位置。天井より上の空中（kSpawnRow は負値）から落とし始める。
	origin_ = {PuzzleConfig::kSpawnColumn, PuzzleConfig::kSpawnRow};

	// 出現位置が壁や既存ブロックと重なっていれば出現できない
	return board.CanFall(GetOccupiedCells());
}

// 追加：自動落下と固定猶予の時間経過を1フレーム分進める。
bool FallingBlock::Update(Board& board,int32_t normalFallInterval){
	// 真下に1マス動けるかどうか
	const GridPos down = {origin_.x, origin_.y + 1};
	const bool canFall = board.CanFall(CalcCells(down,rotation_));

	if(canFall){
		// 空中にいる間は固定猶予をリセットしておく
		lockTimer_ = 0;

		// 加速中かどうかで落下間隔を切り替える
		// ソフトドロップで通常落下より遅くならないようにする。
		const int32_t safeInterval = normalFallInterval > 0 ? normalFallInterval : 1;
		const int32_t fallInterval = isSoftDrop_ && PuzzleConfig::kFallIntervalFramesFast < safeInterval
			? PuzzleConfig::kFallIntervalFramesFast : safeInterval;

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
	// この形（回転込み）の端子ビットも一緒に渡す
	const std::vector<GridPos> lockedCells = GetOccupiedCells();
	board.Place(lockedCells,blockId_,BlockShape::GetTerminals(type_,rotation_),type_);
	UpdateLockedAboveCeiling(lockedCells);
	return true;
}

// 落下可能な最下段まで移動し、即座に盤面へ固定する。
bool FallingBlock::HardDrop(Board& board){
	while(true){
		const GridPos down = {origin_.x,origin_.y + 1};
		// 天井より上（y < 0）は空中として通す必要があるため CanFall を使う。
		// ここで CanPlace を使うと、天井より上にいる間は1マスも落とせず、
		// その場で固定されたことになって誤ってゲームオーバー扱いになる。
		if(!board.CanFall(CalcCells(down,rotation_))){
			break;
		}
		origin_ = down;
	}

	fallTimer_ = 0;
	lockTimer_ = 0;
	const std::vector<GridPos> lockedCells = GetOccupiedCells();
	board.Place(lockedCells,blockId_,BlockShape::GetTerminals(type_,rotation_),type_);
	UpdateLockedAboveCeiling(lockedCells);

	return true;
}

// 追加：固定したマス群の中に天井より上（y < 0）のものが含まれていれば
// lockedAboveCeiling_ を立てる。通常落下・ハードドロップ両方の固定時に呼ぶ。
void FallingBlock::UpdateLockedAboveCeiling(const std::vector<GridPos>& lockedCells){
	for(const GridPos& cell : lockedCells){
		if(cell.y < 0){
			lockedAboveCeiling_ = true;
			return;
		}
	}
}

void FallingBlock::MoveLeft(const Board& board){
	const GridPos moved = {origin_.x - 1, origin_.y};
	if(board.CanFall(CalcCells(moved,rotation_))){
		origin_ = moved;
		// 動かせたら固定猶予をリセットする（着地際の操作を受け付けるため）
		lockTimer_ = 0;
	}
}

// 追加：右に1マス移動する。
void FallingBlock::MoveRight(const Board& board){
	const GridPos moved = {origin_.x + 1, origin_.y};
	if(board.CanFall(CalcCells(moved,rotation_))){
		origin_ = moved;
		lockTimer_ = 0;
	}
}

// 追加：時計回りに1段階回転する。重なる場合は回転しない（押し戻しはしない）。
void FallingBlock::Rotate(const Board& board){
	const int32_t nextRotation = (rotation_ + 1) % BlockShape::kRotationCount;
	if(board.CanFall(CalcCells(origin_,nextRotation))){
		rotation_ = nextRotation;
		lockTimer_ = 0;
	}
}

// 現在このブロックが占めている盤面マスの絶対座標を返す。
std::vector<GridPos> FallingBlock::GetOccupiedCells() const{
	return CalcCells(origin_,rotation_);
}

// 追加：いまこの瞬間に真下まで落とした場合に着地する位置の占有マスを返す。
std::vector<GridPos> FallingBlock::GetLandingCells(const Board& board) const{
	// 現在の基準座標から、真下に1マスずつ下げられなくなるまで下げる
	GridPos landing = origin_;
	while(board.CanFall(CalcCells({landing.x, landing.y + 1},rotation_))){
		landing.y += 1;
	}
	return CalcCells(landing,rotation_);
}

// このブロックの元ブロックID。
int32_t FallingBlock::GetBlockId() const{
	return blockId_;
}

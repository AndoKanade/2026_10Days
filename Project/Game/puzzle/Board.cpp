#include "Board.h"

#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "Model.h"

// このファイル内だけで使う定数
namespace{

	// 盤面に固定されたマスに使うモデル（当面は defaultBlock のキューブで代用する）
	const std::string kBlockModel = "defaultBlock/defaultBlock.obj";

	// 左右の壁（ゴール）に使うモデル
	const std::string kGoalBlockModel = "goalBlock/goalBlock.obj";

	// 下の壁（電源）に使うモデル
	const std::string kSupplyBlockModel = "supplyBlock/supplyBlock.obj";

	// 追加：盤面に固定されたマスの色
	const Vector4 kFilledCellColor = {0.55f, 0.65f, 0.85f, 1.0f};

	// 消去演出中のマスの色（通電したことが分かるよう明るい色で光らせる）
	const Vector4 kClearingCellColor = {1.0f, 0.95f, 0.4f, 1.0f};

	// 電源から通電が届いているマスの色（まだゴールには届いていない状態）。
	// 「あと1マスで届く」が見えるよう、通常のマスとはっきり違う明るさにする。
	const Vector4 kPoweredCellColor = {0.4f, 0.95f, 0.9f, 1.0f};

	// 通電判定で使う4方向の隣接オフセット。
	// ブロック同士は向きに関係なく常に導通する仕様のため、
	// 実際に隣接している（触れている）マスかどうかだけを見る。
	constexpr GridPos kDirs[4] = { {0,-1}, {0,1}, {-1,0}, {1,0} };
}

// コンストラクタ・デストラクタ
// Obj3D の完全な型が見えるこの場所で定義する必要がある
Board::Board() = default;
Board::~Board() = default;

// モデルの読み込みとオブジェクト生成などの初期化
void Board::Initialize(Obj3dCommon* object3dCommon){
	object3dCommon_ = object3dCommon;

	// 使用するモデルを読み込む
	ModelManager::GetInstance()->LoadModel(kBlockModel);
	ModelManager::GetInstance()->LoadModel(kGoalBlockModel);
	ModelManager::GetInstance()->LoadModel(kSupplyBlockModel);

	// --- U字の壁ブロックの生成 ---
	// 盤面のマス領域のすぐ外側を、左・下・右の順に囲む。上辺は開けておく（U字）。
	// 左右の壁はゴール（goalBlock）、下の壁は電源（supplyBlock）で描画する。
	for(int32_t y = 0; y < PuzzleConfig::kBoardHeight; ++y){
		CreateWallBlock(-1,y,kGoalBlockModel);           // 左の壁（ゴール）
	}
	for(int32_t y = 0; y < PuzzleConfig::kBoardHeight; ++y){
		CreateWallBlock(width_,y,kGoalBlockModel);       // 右の壁（ゴール）
	}
	for(int32_t x = -1; x <= width_; ++x){
		CreateWallBlock(x,PuzzleConfig::kBoardHeight,kSupplyBlockModel); // 下の壁（電源。左右の角を含む）
	}
}

// U字の壁ブロックを1個生成して wallObjs_ に追加する
void Board::CreateWallBlock(int32_t x,int32_t y,const std::string& modelPath){
	auto obj = std::make_unique<Obj3D>();
	obj->Initialize(object3dCommon_);
	obj->SetModel(modelPath);
	obj->SetScale({PuzzleConfig::kCellModelScale, PuzzleConfig::kCellModelScale, PuzzleConfig::kCellModelScale});
	obj->SetTranslate(GridToWorld(x,y));

	if(Model::Material* wallMaterial = obj->GetMaterial()){
		wallMaterial->enableLighting = 0; // 2D的な見た目にするため陰影を切る（色はモデルのテクスチャそのまま）
	}

	wallObjs_.push_back(std::move(obj));
}

// U字の壁ブロックを1個生成して wallObjs_ に追加する処理と同じ手順で、
// 追加：盤面に固定されたマスの見た目を cells_ から作り直す
void Board::RebuildCellObjects(){
	cellObjs_.clear();

	// どのマスまで実際に通電が届いているかを調べ、見た目のハイライトに使う
	const auto powered = ComputePoweredMask();

	for(int32_t y = 0; y < PuzzleConfig::kBoardHeight; ++y){
		for(int32_t x = 0; x < width_; ++x){
			// 空きマスは描画しない
			if(cells_[y][x].IsEmpty()){
				continue;
			}

			auto obj = std::make_unique<Obj3D>();
			obj->Initialize(object3dCommon_);
			obj->SetModel(kBlockModel);
			obj->SetScale({PuzzleConfig::kCellModelScale, PuzzleConfig::kCellModelScale, PuzzleConfig::kCellModelScale});
			obj->SetTranslate(GridToWorld(x,y));

			// 追加：消去演出中＞通電中＞通常、の優先順で色を決める
			const bool isClearingCell = IsClearingCell(x,y);

			Vector4 color = kFilledCellColor;
			if(isClearingCell){
				color = kClearingCellColor;
			} else if(powered[y][x]){
				color = kPoweredCellColor;
			}

			if(Model::Material* cellMaterial = obj->GetMaterial()){
				cellMaterial->color = color;
				cellMaterial->enableLighting = 0; // 2D的な見た目にするため陰影を切る
			}

			cellObjs_.push_back(std::move(obj));
		}
	}
}

// 電源（最下段）から実際にどこまで通電が届いているかを幅優先探索で調べる。
// ゴールに届いているかは問わない。見た目のハイライトに使う。
std::array<std::array<bool,PuzzleConfig::kBoardWidthMax>,PuzzleConfig::kBoardHeight> Board::ComputePoweredMask() const{
	std::array<std::array<bool,PuzzleConfig::kBoardWidthMax>,PuzzleConfig::kBoardHeight> powered{};

	const int32_t bottomY = PuzzleConfig::kBoardHeight - 1;

	std::vector<GridPos> queue;
	size_t queueHead = 0;

	// 最下段のマスは電源に接しているとみなし、まとめて探索の起点にする
	for(int32_t x = 0; x < width_; ++x){
		if(!cells_[bottomY][x].IsEmpty() && !powered[bottomY][x]){
			powered[bottomY][x] = true;
			queue.push_back({x, bottomY});
		}
	}

	while(queueHead < queue.size()){
		const GridPos current = queue[queueHead];
		++queueHead;

		// 4方向を調べ、隣接している（触れている）マスへ探索を広げる
		for(int32_t dir = 0; dir < 4; ++dir){
			const int32_t nx = current.x + kDirs[dir].x;
			const int32_t ny = current.y + kDirs[dir].y;

			if(!IsInside(nx,ny) || powered[ny][nx] || cells_[ny][nx].IsEmpty()){
				continue;
			}

			powered[ny][nx] = true;
			queue.push_back({nx, ny});
		}
	}

	return powered;
}

// 指定マスが現在消去演出中かどうか
bool Board::IsClearingCell(int32_t x,int32_t y) const{
	for(const GridPos& pos : clearingCells_){
		if(pos.x == x && pos.y == y){
			return true;
		}
	}
	return false;
}

// 毎フレームの更新
void Board::Update(){
	// カメラ移動などに追従できるよう、行列は毎フレーム更新する。
	for(auto& wallObj : wallObjs_){
		wallObj->Update();
	}
	// 追加：固定されたマスの行列も毎フレーム更新する
	for(auto& cellObj : cellObjs_){
		cellObj->Update();
	}

	// 消去演出中は経過時間を進め、演出が終わったら実際にマスを消して落下させる
	if(isClearing_){
		++clearTimer_;
		if(clearTimer_ >= PuzzleConfig::kClearEffectFrames){
			for(const GridPos& pos : clearingCells_){
				cells_[pos.y][pos.x] = Cell{};
			}
			clearingCells_.clear();
			isClearing_ = false;

			// マス単位で下に詰める（ブロックの形は保持しない）
			ApplyGravity();

			// 落下後に再度通電判定を行う。まだ繋がっていれば連鎖してまた消去演出に入る
			// （isClearing_ は直前で false にしてあるため、ここで判定が素通りされることはない）
			ResolveConduction();

			// 消去・落下後の見た目を作り直す
			RebuildCellObjects();
		}
	}
}

// 盤面（U字の壁）の描画
void Board::Draw(){
	for(auto& wallObj : wallObjs_){
		wallObj->Draw();
	}
	// 追加：固定されたマスを描画する
	for(auto& cellObj : cellObjs_){
		cellObj->Draw();
	}
}

// 追加：指定したマス群すべてが盤面内かつ空きなら配置可能とみなす。
bool Board::CanPlace(const std::vector<GridPos>& cells) const{
	for(const GridPos& pos : cells){
		// 盤面の範囲外に出るマスがあれば配置不可
		if(!IsInside(pos.x,pos.y)){
			return false;
		}
		// 既に別のブロックで埋まっているマスがあれば配置不可
		if(!cells_[pos.y][pos.x].IsEmpty()){
			return false;
		}
	}
	return true;
}

// 指定したマス群へ blockId と端子ビットを書き込んで盤面に固定する。
void Board::Place(const std::vector<GridPos>& cells,int32_t blockId,const std::vector<uint8_t>& terminals){
	for(size_t i = 0; i < cells.size(); ++i){
		const GridPos& pos = cells[i];

		// 安全のため範囲外のマスには書き込まない
		if(!IsInside(pos.x,pos.y)){
			continue;
		}

		cells_[pos.y][pos.x].blockId = blockId;

		// 対応する端子ビットがあれば書き込む（無ければ0のまま）
		if(i < terminals.size()){
			cells_[pos.y][pos.x].terminals = terminals[i];
		}
	}

	// 通電判定を行い、ゴールまで繋がっていれば対象マスを消す
	ResolveConduction();

	// 盤面に置かれたマスの見た目を作り直す（消去があった場合もここで1回だけ反映する）
	RebuildCellObjects();
}

// 電源（最下段）から幅優先探索で通電範囲（一かたまり）を調べ、ゴール（左右端）まで
// 繋がっていれば、その一かたまりから行き止まりの枝を取り除いた残りのマスを
// 消去演出の対象にする（電源とゴールを実際に繋ぐのに使われているマスだけが残る。
// 複数ルートやループも、行き止まりでなければすべて残る）。この時点ではまだ消さない。
void Board::ResolveConduction(){
	// 既に消去演出中なら、演出が終わるまで新たな判定はしない
	if(isClearing_){
		return;
	}

	// マスごとの訪問済みフラグ（同じマスを何度も探索しないようにする）
	std::array<std::array<bool,PuzzleConfig::kBoardWidthMax>,PuzzleConfig::kBoardHeight> visited{};

	// 通電が確定したマスをまとめて集める
	std::vector<GridPos> cellsToClear;

	const int32_t bottomY = PuzzleConfig::kBoardHeight - 1;

	// 最下段のマスは電源に接しているとみなし、1マスずつ探索の起点にする
	for(int32_t startX = 0; startX < width_; ++startX){
		if(cells_[bottomY][startX].IsEmpty() || visited[bottomY][startX]){
			continue;
		}

		// この起点マスから繋がっている一かたまり（成分）を幅優先探索ですべて集める
		std::vector<GridPos> component;
		std::vector<GridPos> queue;
		size_t queueHead = 0;

		queue.push_back({startX, bottomY});
		visited[bottomY][startX] = true;

		bool reachedGoal = false;

		while(queueHead < queue.size()){
			const GridPos current = queue[queueHead];
			++queueHead;
			component.push_back(current);

			// ゴール判定：電源（最下段）と同様、左右端のマスに到達した時点でゴールとする。
			if(current.x == 0 || current.x == width_ - 1){
				reachedGoal = true;
			}

			// 4方向を調べ、隣接している（触れている）マスへ探索を広げる
			for(int32_t dir = 0; dir < 4; ++dir){
				const int32_t nx = current.x + kDirs[dir].x;
				const int32_t ny = current.y + kDirs[dir].y;

				if(!IsInside(nx,ny) || visited[ny][nx] || cells_[ny][nx].IsEmpty()){
					continue;
				}

				visited[ny][nx] = true;
				queue.push_back({nx, ny});
			}
		}

		// ゴールまで届いていない一かたまりは対象外
		if(!reachedGoal){
			continue;
		}

		// この一かたまりから、行き止まりの枝（他のマスと1本以下しか繋がっていないマス）を
		// 繰り返し取り除いていく。電源・ゴールに直接触れているマスは、行き止まりに
		// 見えても例外として絶対に取り除かない。残ったマスが消去対象になる。
		std::array<std::array<bool,PuzzleConfig::kBoardWidthMax>,PuzzleConfig::kBoardHeight> inSet{};
		std::array<std::array<int32_t,PuzzleConfig::kBoardWidthMax>,PuzzleConfig::kBoardHeight> degree{};
		std::array<std::array<bool,PuzzleConfig::kBoardWidthMax>,PuzzleConfig::kBoardHeight> isProtected{};

		for(const GridPos& pos : component){
			inSet[pos.y][pos.x] = true;
			isProtected[pos.y][pos.x] = (pos.y == bottomY) || (pos.x == 0) || (pos.x == width_ - 1);
		}
		for(const GridPos& pos : component){
			int32_t d = 0;
			for(int32_t dir = 0; dir < 4; ++dir){
				const int32_t nx = pos.x + kDirs[dir].x;
				const int32_t ny = pos.y + kDirs[dir].y;
				if(IsInside(nx,ny) && inSet[ny][nx]){
					++d;
				}
			}
			degree[pos.y][pos.x] = d;
		}

		// 保護されていない、繋がりが1本以下のマスを取り除きの起点にする
		std::vector<GridPos> pruneQueue;
		for(const GridPos& pos : component){
			if(!isProtected[pos.y][pos.x] && degree[pos.y][pos.x] <= 1){
				pruneQueue.push_back(pos);
			}
		}

		size_t pruneHead = 0;
		while(pruneHead < pruneQueue.size()){
			const GridPos current = pruneQueue[pruneHead];
			++pruneHead;

			// 既に取り除き済みなら何もしない
			if(!inSet[current.y][current.x]){
				continue;
			}
			inSet[current.y][current.x] = false;

			// 取り除いた分、隣のマスの繋がり本数を減らし、1本以下になったら追加で取り除く
			for(int32_t dir = 0; dir < 4; ++dir){
				const int32_t nx = current.x + kDirs[dir].x;
				const int32_t ny = current.y + kDirs[dir].y;

				if(!IsInside(nx,ny) || !inSet[ny][nx]){
					continue;
				}

				--degree[ny][nx];
				if(!isProtected[ny][nx] && degree[ny][nx] <= 1){
					pruneQueue.push_back({nx, ny});
				}
			}
		}

		// 取り除かれずに残ったマスを消去対象にする
		for(const GridPos& pos : component){
			if(inSet[pos.y][pos.x]){
				cellsToClear.push_back(pos);
			}
		}
	}

	// 消去対象があれば、即座には消さず消去演出の状態に入る。
	// 実際にマスを消す処理は Update() 側でタイマーが満了したときに行う。
	if(!cellsToClear.empty()){
		clearingCells_ = std::move(cellsToClear);
		isClearing_ = true;
		clearTimer_ = 0;
	}
}

// 空きマスを詰めるように、各列のマスをマス単位で下へ落とす。
// ブロックの形は保持しない（欠片ごとにバラバラに落ちる）。
void Board::ApplyGravity(){
	for(int32_t x = 0; x < width_; ++x){
		// この列に残っているマスを上から順に集める
		std::vector<Cell> remaining;
		for(int32_t y = 0; y < PuzzleConfig::kBoardHeight; ++y){
			if(!cells_[y][x].IsEmpty()){
				remaining.push_back(cells_[y][x]);
			}
		}

		// 列をいったん空にする
		for(int32_t y = 0; y < PuzzleConfig::kBoardHeight; ++y){
			cells_[y][x] = Cell{};
		}

		// 集めたマスを、並び順を保ったまま下端に詰めて書き戻す
		int32_t writeY = PuzzleConfig::kBoardHeight - 1;
		for(auto it = remaining.rbegin(); it != remaining.rend(); ++it){
			cells_[writeY][x] = *it;
			--writeY;
		}
	}
}

// 指定マス座標が現在の盤面の範囲内かどうか
bool Board::IsInside(int32_t x,int32_t y) const{
	return x >= 0 && x < width_ && y >= 0 && y < PuzzleConfig::kBoardHeight;
}

// 盤面のマス座標を、そのマスの中心のワールド座標に変換する
Vector3 Board::GridToWorld(int32_t x,int32_t y) const{
	// 盤面全体が原点を中心に来るように、中心からのオフセットで計算する
	const float halfWidth = (static_cast<float>(width_) - 1.0f) * 0.5f;
	const float halfHeight = (static_cast<float>(PuzzleConfig::kBoardHeight) - 1.0f) * 0.5f;

	return {
		(static_cast<float>(x) - halfWidth) * PuzzleConfig::kCellWorldSize,
		// y は 0 が盤面の一番上。上ほどワールド座標の Y を大きくする。
		(halfHeight - static_cast<float>(y)) * PuzzleConfig::kCellWorldSize,
		PuzzleConfig::kBoardCenterZ
	};
}

// 盤面の幅を切り替える
void Board::SetWidth(int32_t width){
	// 想定外の値は無視する
	if(width <= 0 || width > PuzzleConfig::kBoardWidthMax){
		return;
	}
	width_ = width;
}

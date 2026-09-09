#include "Board.h"
#include <unordered_set>
#include <unordered_map>

#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "Model.h"

// このファイル内だけで使う定数
namespace{

	// 変更：盤面に固定されたマスに使うモデル。辺と角を面取りしたキューブにして、
	// 平行光源のハイライトが辺に乗るようにする。
	const std::string kBlockModel = "blockBevel/blockBevel.obj";

	// 追加：配線に使うモデル。細く引き伸ばして棒にするため、面取りのない素のキューブを使う。
	const std::string kWireModel = "defaultBlock/defaultBlock.obj";

	// 変更：左右の壁（ゴール）に使うモデル。落下ブロックと同じツヤを出すため面取り版にする。
	const std::string kGoalBlockModel = "goalBlock/goalBlockBevel.obj";

	// 変更：下の壁（電源）に使うモデル。こちらも面取り版にする。
	const std::string kSupplyBlockModel = "supplyBlock/supplyBlockBevel.obj";

	// 追加：壁ブロックへ渡す色。テクスチャの色をそのまま出したいので白にし、
	// ライティングによる減衰ぶんだけ持ち上げる。
	const Vector4 kWallColor = PuzzleConfig::ApplyLitGain(PuzzleConfig::ToLinearColor({1.0f, 1.0f, 1.0f, 1.0f}));

	// 削除：通常の固定マスの色は PuzzleConfig::GetBlockColor() でブロックの種類ごとに引くようにした

	// 変更：以下の色はすべて「画面に出したい色」（sRGB）で書き、
	// PuzzleConfig::ToLinearColor() で線形へ変換したものを保持する。
	// 描画先が sRGB のレンダーターゲットなので、変換せずに渡すと色が淡く浮いてしまう。

	// 最強マス（十字マス）の色。元ブロックの種類を持たないため専用色で描く
	const Vector4 kStrongestCellColor = PuzzleConfig::ToLinearColor({0.9f, 0.45f, 1.0f, 1.0f});

	// 消去演出中のマスの色（通電したことが分かるよう明るい色で光らせる）
	const Vector4 kClearingCellColor = PuzzleConfig::ToLinearColor({1.0f, 0.95f, 0.4f, 1.0f});

	// 削除：通電中のマスは専用色で塗らず、PuzzleConfig::MakePoweredColor() で
	// 本来の色を明るくするだけにした（元のブロックの種類が分かるようにするため）。

	// 通電判定で使う4方向の隣接オフセットと、向き合う辺の端子ビットの対応表。
	// kSelfBits[i] は自分がその方向を向くための端子、kOtherBits[i] は隣のマスが
	// こちら側を向くために必要な端子（互いに両方立っていて初めて繋がる）。
	constexpr GridPos kDirs[4]      = { {0,-1}, {0,1}, {-1,0}, {1,0} };
	constexpr uint8_t kSelfBits[4]  = { Terminal::kUp,   Terminal::kDown, Terminal::kLeft,  Terminal::kRight };
	constexpr uint8_t kOtherBits[4] = { Terminal::kDown, Terminal::kUp,   Terminal::kRight, Terminal::kLeft };

	// 追加：配線描画（2.6の可視化）用の定数。
	// マス中心から辺へ向かって伸びる細い棒として、端子が立っている方向だけ描画する。
	constexpr float kWireThicknessScale = 0.08f; // 配線の太さ

	// 変更：配線の長さ（＝マス中心からのずらし量）はマスの半分ちょうどにする。
	// こうすると繋がっている隣同士の配線が境界で接して1本の線に見え、
	// 繋がっていない端子はマスの外へ出ない短い突起として残る。
	constexpr float kWireLengthScale = PuzzleConfig::kCellWorldSize * 0.5f;

	// 追加：配線をマスの手前へ出す量。
	// カメラは盤面より -Z 側にあるので、マスの前面（-Z 側の面）よりさらに手前へ置く。
	// 配線の厚みの半分だけマスへめり込ませて、面が重なるちらつきを避ける。
	constexpr float kWireFrontOffset = PuzzleConfig::kCellModelScale + kWireThicknessScale * 0.5f;

	// 変更：通電中の配線の色。電気そのものを表す水色にする。
	// マスの色（黄緑・紅・山吹・藤紫）とは色相が離れているので、
	// どの種類のブロックの上でも配線が埋もれない。
	const Vector4 kWireLitColor = PuzzleConfig::ToLinearColor({0.35f, 0.95f, 1.0f, 1.0f});

	// 変更：非通電の配線の色。マスの色に沈む暗い線にする。
	const Vector4 kWireUnlitColor = PuzzleConfig::ToLinearColor({0.10f, 0.11f, 0.16f, 1.0f});
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
	ModelManager::GetInstance()->LoadModel(kWireModel); // 追加：配線用のモデルもここで読み込む
	ModelManager::GetInstance()->LoadModel(kGoalBlockModel);
	ModelManager::GetInstance()->LoadModel(kSupplyBlockModel);

	// U字の壁ブロックを生成する（幅は width_ の初期値を使う）
	RebuildWalls();
}

// 追加：現在の width_ に合わせて、U字の壁ブロックを作り直す。
// 盤面のマス領域のすぐ外側を、左・下・右の順に囲む。上辺は開けておく（U字）。
// 左右の壁はゴール（goalBlock）、下の壁は電源（supplyBlock）で描画する。
void Board::RebuildWalls(){
	wallObjs_.clear();

	for(int32_t y = 0; y < PuzzleConfig::kBoardHeight; ++y){
		CreateWallBlock(-1,y,kGoalBlockModel);           // 左の壁（ゴール）
	}
	for(int32_t y = 0; y < PuzzleConfig::kBoardHeight; ++y){
		CreateWallBlock(width_,y,kGoalBlockModel);       // 右の壁（ゴール。位置は幅で変わる）
	}
	for(int32_t x = -1; x <= width_; ++x){
		CreateWallBlock(x,PuzzleConfig::kBoardHeight,kSupplyBlockModel); // 下の壁（電源。左右の角を含む。範囲は幅で変わる）
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
		// 変更：落下ブロックと同じようにライティングを有効にし、面取り面へツヤを乗せる。
		// テクスチャが黒ベースで拡散光では光らないため、鏡面反射と映り込みは壁専用の強めの値を使う。
		wallMaterial->color = kWallColor;
		wallMaterial->enableLighting = 1;
		wallMaterial->shininess = PuzzleConfig::kWallShininess;
		wallMaterial->environmentCoefficient = PuzzleConfig::kWallEnvironmentCoefficient;
	}

	wallObjs_.push_back(std::move(obj));
}

// 消去演出中のマス1個ぶんについて、電源からの光の波が届いているかどうかと、
// 届いていた場合にどれだけ膨らませるか(popScale)を計算する。
// RebuildCellObjects()（初回構築）と UpdateClearingCellVisuals()（毎フレームの軽量更新）の
// 両方から同じ計算式を使うための共通処理。
void Board::ComputeClearWaveState(int32_t y,bool& waveReached,float& popScale) const{
	waveReached = false;
	popScale = 1.0f;

	const int32_t distance = (PuzzleConfig::kBoardHeight - 1) - y;
	// 消去が確定するまでの最後の描画フレーム（clearTimer_ の最大値）で波が
	// ちょうど一番遠いマスまで届くよう、分母を1フレーム分小さくしておく。
	// そうしないと一番遠いマスだけ光る前に消えてしまう。
	const int32_t waveDurationFrames = PuzzleConfig::kClearEffectFrames > 1 ? PuzzleConfig::kClearEffectFrames - 1 : 1;
	const float clearProgress = static_cast<float>(clearTimer_) / static_cast<float>(waveDurationFrames);
	const float waveFront = clearProgress * static_cast<float>(clearWaveMaxDistance_);
	waveReached = static_cast<float>(distance) <= waveFront;
	if(waveReached){
		popScale = 1.0f + PuzzleConfig::kClearPopScaleAmount * clearProgress * clearProgress;
	}
}

// U字の壁ブロックを1個生成して wallObjs_ に追加する処理と同じ手順で、
// 追加：盤面に固定されたマスの見た目を cells_ から作り直す
// 注意：GPU用の定数バッファをマスごとに新規確保する重い処理のため、消去演出中の
// 毎フレーム更新には使わない（そちらは UpdateClearingCellVisuals() を使う）。
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

			// 消去演出中だけは一瞬の演出なので専用色で塗りつぶす。
			const bool isClearingCell = IsClearingCell(x,y);

			// 消去演出中は、電源に近い行から順に「光の波」が届いたことにする。
			// 波が届く前は通常の通電表示のまま、届いた瞬間に消去色へ切り替わり、
			// 消えるまでの残り時間ぶんだけマスが膨らんで弾ける直前のように見せる。
			bool waveReached = false;
			float popScale = 1.0f;
			if(isClearingCell){
				ComputeClearWaveState(y,waveReached,popScale);
			}

			auto obj = std::make_unique<Obj3D>();
			obj->Initialize(object3dCommon_);
			obj->SetModel(kBlockModel);
			obj->SetScale({
				PuzzleConfig::kCellModelScale * popScale,
				PuzzleConfig::kCellModelScale * popScale,
				PuzzleConfig::kCellModelScale * popScale
			});
			obj->SetTranslate(GridToWorld(x,y));

			// 変更：まずマス本来の色（最強マスは専用の紫、それ以外はブロックの種類色）を決め、
			// 通電中はその色を塗り替えず明るくするだけにする。
			// 別の色で塗りつぶすと元のブロックの種類が分からなくなるため。
			Vector4 color = cells_[y][x].IsStrongest()
				? kStrongestCellColor
				: PuzzleConfig::GetBlockColor(cells_[y][x].type);
			if(waveReached){
				color = kClearingCellColor;
			} else if(powered[y][x]){
				color = PuzzleConfig::MakePoweredColor(color);
			}

			// 決めた色に、ライティングによる減衰ぶんの補正をまとめてかける
			color = PuzzleConfig::ApplyLitGain(color);

			if(Model::Material* cellMaterial = obj->GetMaterial()){
				cellMaterial->color = color;

				// 変更：面取り面が光を拾うようライティングを有効にし、光沢を乗せる
				cellMaterial->enableLighting = 1;
				cellMaterial->shininess = PuzzleConfig::kBlockShininess;
				cellMaterial->environmentCoefficient = PuzzleConfig::kBlockEnvironmentCoefficient;
			}

			cellObjs_.push_back(std::move(obj));

			// 追加：このマスの配線を、端子が立っている方向だけ中心から辺へ伸びる細い棒で描画する。
			// 通電中（このマスが光っている）なら明るく、そうでなければ暗く表示する。
			const Vector4 wireColor = (waveReached || powered[y][x]) ? kWireLitColor : kWireUnlitColor;

			for(int32_t dir = 0; dir < 4; ++dir){
				if(!(cells_[y][x].terminals & kSelfBits[dir])){
					continue;
				}

				auto wireObj = std::make_unique<Obj3D>();
				wireObj->Initialize(object3dCommon_);
				wireObj->SetModel(kWireModel);

				// 上下方向（dir 0,1）は縦長、左右方向（dir 2,3）は横長のスケールにする
				const bool isVertical = (dir == 0 || dir == 1);
				wireObj->SetScale(isVertical
					? Vector3{kWireThicknessScale, kWireLengthScale, kWireThicknessScale}
					: Vector3{kWireLengthScale, kWireThicknessScale, kWireThicknessScale});

				// マス中心から、その方向へ配線の長さぶんだけずらした位置に置く
				// （グリッドのy方向とワールドのY軸は向きが逆なので符号を反転する）
				Vector3 wirePos = GridToWorld(x,y);
				wirePos.x += static_cast<float>(kDirs[dir].x) * kWireLengthScale;
				wirePos.y -= static_cast<float>(kDirs[dir].y) * kWireLengthScale;

				// 変更：マスの手前へ出す。ここを外すとマスの立方体に埋まって配線が見えない。
				wirePos.z -= kWireFrontOffset;
				wireObj->SetTranslate(wirePos);

				if(Model::Material* wireMaterial = wireObj->GetMaterial()){
					wireMaterial->color = wireColor;
					wireMaterial->enableLighting = 0;
				}

				cellObjs_.push_back(std::move(wireObj));
			}
		}
	}
}

// 消去演出中、毎フレームの「光の波」アニメーションを反映する軽量な更新。
// RebuildCellObjects() と違ってオブジェクトを作り直さず、既に存在する cellObjs_ の
// 色・スケールだけをその場で書き換える（GPU用の定数バッファを毎フレーム確保すると
// 消去のたびに大量の生成が発生し、負荷で描画が乱れるため）。
// cellObjs_ の並びは RebuildCellObjects() と同じ順番（y,x の順で、各マスにつき
// 本体→端子の立っている方向の配線の順）で作られている前提で、同じ順番になぞって書き換える。
void Board::UpdateClearingCellVisuals(){
	const auto powered = ComputePoweredMask();
	size_t objIndex = 0;

	for(int32_t y = 0; y < PuzzleConfig::kBoardHeight; ++y){
		for(int32_t x = 0; x < width_; ++x){
			if(cells_[y][x].IsEmpty()){
				continue;
			}
			// 安全策：想定と cellObjs_ の構成がずれていたら、それ以上は触らない
			if(objIndex >= cellObjs_.size()){
				return;
			}

			const bool isClearingCell = IsClearingCell(x,y);
			bool waveReached = false;
			float popScale = 1.0f;
			if(isClearingCell){
				ComputeClearWaveState(y,waveReached,popScale);
			}

			Obj3D* bodyObj = cellObjs_[objIndex].get();
			++objIndex;

			bodyObj->SetScale({
				PuzzleConfig::kCellModelScale * popScale,
				PuzzleConfig::kCellModelScale * popScale,
				PuzzleConfig::kCellModelScale * popScale
			});

			Vector4 color = cells_[y][x].IsStrongest()
				? kStrongestCellColor
				: PuzzleConfig::GetBlockColor(cells_[y][x].type);
			if(waveReached){
				color = kClearingCellColor;
			} else if(powered[y][x]){
				color = PuzzleConfig::MakePoweredColor(color);
			}
			color = PuzzleConfig::ApplyLitGain(color);

			if(Model::Material* cellMaterial = bodyObj->GetMaterial()){
				cellMaterial->color = color;
			}

			const Vector4 wireColor = (waveReached || powered[y][x]) ? kWireLitColor : kWireUnlitColor;

			for(int32_t dir = 0; dir < 4; ++dir){
				if(!(cells_[y][x].terminals & kSelfBits[dir])){
					continue;
				}
				if(objIndex >= cellObjs_.size()){
					return;
				}
				Obj3D* wireObj = cellObjs_[objIndex].get();
				++objIndex;

				if(Model::Material* wireMaterial = wireObj->GetMaterial()){
					wireMaterial->color = wireColor;
				}
			}
		}
	}
}

// 追加：マスの中身と消去演出の状態をすべて初期状態へ戻す（壁はそのまま）。
void Board::Reset(){
	// マスをすべて空にする
	cells_ = {};

	// 消去演出の途中だった場合に備えて、その状態も消す
	clearingCells_.clear();
	isClearing_ = false;
	clearTimer_ = 0;
	chainCount_ = 0;
	clearResults_.clear();

	// 空になった盤面に合わせて見た目を作り直す
	RebuildCellObjects();
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

		const Cell& currentCell = cells_[current.y][current.x];

		// 4方向を調べ、互いに向き合う端子ビットが両方立っているマスへ探索を広げる
		for(int32_t dir = 0; dir < 4; ++dir){
			const int32_t nx = current.x + kDirs[dir].x;
			const int32_t ny = current.y + kDirs[dir].y;

			if(!IsInside(nx,ny) || powered[ny][nx] || cells_[ny][nx].IsEmpty()){
				continue;
			}
			if(!(currentCell.terminals & kSelfBits[dir]) || !(cells_[ny][nx].terminals & kOtherBits[dir])){
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
			// マスを消す前に控えておく。あわせて、消えるマスが元々属していた
			// ブロックID（同じ元ブロックの残骸を後で見つけるため）も控えておく。
			std::vector<GridPos> clearedCells = clearingCells_;
			std::vector<int32_t> clearedBlockIds;
			for(const GridPos& pos : clearingCells_){
				const int32_t id = cells_[pos.y][pos.x].blockId;
				if(id != PuzzleConfig::kEmptyBlockId){
					clearedBlockIds.push_back(id);
				}
			}

			// 消去数と連鎖数を記録し、GameSceneからゲージへ一度だけ渡す。
			clearResults_.push_back({static_cast<int32_t>(clearingCells_.size()),chainCount_});

			for(const GridPos& pos : clearingCells_){
				cells_[pos.y][pos.x] = Cell{};
			}
			clearingCells_.clear();
			isClearing_ = false;

			// 変更：仕様2.8通り、消去後に同じ元ブロックIDのマスが盤面に1マスだけ
			// 残ったときだけ最強マスへ変換する。判定は必ず落下前に行う
			// （落下後だと別ブロックの断片が隣接し、正しく数えられなくなるため）。
			const std::unordered_set<int32_t> affectedBlockIds(clearedBlockIds.begin(),clearedBlockIds.end());

			// 今回の消去に関係した元ブロックIDごとに、残っているマスの位置を集める
			std::unordered_map<int32_t,std::vector<GridPos>> remainingCellsByBlockId;
			for(int32_t y = 0; y < GetHeight(); ++y){
				for(int32_t x = 0; x < width_; ++x){
					const Cell& cell = cells_[y][x];
					if(!cell.IsEmpty() && affectedBlockIds.contains(cell.blockId)){
						remainingCellsByBlockId[cell.blockId].push_back({x,y});
					}
				}
			}

			// 残りがちょうど1マスだった元ブロックIDだけ、そのマスを最強マスに変換する
			for(const auto& [blockId,remainingCells] : remainingCellsByBlockId){
				if(remainingCells.size() != 1){
					continue;
				}
				const GridPos& pos = remainingCells.front();
				cells_[pos.y][pos.x].MakeStrongest();
				// 十字化した列も、新しい落下処理に渡す対象へ含める。
				clearedCells.push_back(pos);
			}

			// マス単位で下に詰める（ブロックの形は保持しない）。
			// 残骸（別の列に残っている出っ張り部分など）が乗っている列も対象にする。
			// 支えにしていたマスが消えて構造的に浮いた状態のため、自分の列自体には
			// 消去が起きていなくても落とす必要がある。それ以外の、本当に無関係な列は
			// 従来通り触らない。
			// Easy は行を丸ごと消す仕様なので、その行に元々空きマスだった列も
			// 含めて全列を強制的に詰め直す（forceAllColumns）。これをしないと、
			// たまたま消去行が空きマスだった列だけ上のブロックが落ちてこない。
			ApplyGravity(clearedCells,clearedBlockIds,difficulty_ == Difficulty::Easy);

			// 落下後に再度通電判定を行う。まだ繋がっていれば連鎖してまた消去演出に入る
			// （isClearing_ は直前で false にしてあるため、ここで判定が素通りされることはない）
			ResolveConduction();

			// 消去・落下後の見た目を作り直す
			RebuildCellObjects();
		} else{
			// 消去演出中は毎フレーム、電源に近い（下段の）マスから順に光っていく
			// 光の波が経路を伝って進む様子をアニメーションさせる。
			// RebuildCellObjects() は使わず、既存オブジェクトの色・スケールだけを更新する
			// 軽量な処理にする（毎フレーム作り直すとGPUリソースの生成が過剰になるため）。
			UpdateClearingCellVisuals();
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

// 追加：落下中ブロック用の配置可否判定。
// 天井より上（y < 0）は空中とみなして通す。左右の壁・床・既存ブロックとの重なりのみ不可とする。
bool Board::CanFall(const std::vector<GridPos>& cells) const{
	for(const GridPos& pos : cells){
		// 左右の壁・床は盤面幅・高さの外なので必ず不可
		if(pos.x < 0 || pos.x >= width_ || pos.y >= PuzzleConfig::kBoardHeight){
			return false;
		}
		// 天井より上（y < 0）は盤面データを持たない空中なので、常に通す
		if(pos.y < 0){
			continue;
		}
		// 盤面内は、既に別のブロックで埋まっていれば不可
		if(!cells_[pos.y][pos.x].IsEmpty()){
			return false;
		}
	}
	return true;
}

// 指定したマス群へ blockId・端子ビットを書き込んで盤面に固定する。
void Board::Place(const std::vector<GridPos>& cells,int32_t blockId,const std::vector<uint8_t>& terminals,BlockShape::Type type){
	for(size_t i = 0; i < cells.size(); ++i){
		const GridPos& pos = cells[i];

		// 安全のため範囲外のマスには書き込まない
		if(!IsInside(pos.x,pos.y)){
			continue;
		}

		cells_[pos.y][pos.x].blockId = blockId;

		// 変更：描画の色分けに使う元ブロックの種類を控えておく
		cells_[pos.y][pos.x].type = type;

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
// 繋がっていれば、そのかたまりを消去演出の対象にする。
//
// どこまで消すかは難易度で変わる（詳しくは Difficulty.h）。
//   Easy   ：かたまりが通っている横列を丸ごと消す。通電に関係ないマスも片付く。
//   Normal ：かたまりのマスをすべて消す。枝分かれもループも含む。
//   Hard   ：電源とゴールを実際に結んでいるマスだけを消す。枝は盤面に残る。
//
// この時点ではまだ消さない。
void Board::ResolveConduction(){
	// 既に消去演出中なら、演出が終わるまで新たな判定はしない
	if(isClearing_){
		return;
	}

	// マスごとの訪問済みフラグ（同じマスを何度も探索しないようにする）
	std::array<std::array<bool,PuzzleConfig::kBoardWidthMax>,PuzzleConfig::kBoardHeight> visited{};

	// 消去対象のマス。Normal と Hard はここへ直接ためる。
	// 一かたまりどうしは visited を共有していて重ならないため、重複はしない。
	std::vector<GridPos> cellsToClear;

	// Easy 用。通電が成立した一かたまりが通っている横列に印を付ける。
	// 行単位でまとめるので、複数のかたまりが同じ行を通っても重複しない。
	std::array<bool,PuzzleConfig::kBoardHeight> rowsToClear{};

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

			const Cell& currentCell = cells_[current.y][current.x];

			// ゴール判定：左右端のマスに位置していて、かつそのマスの配線が外側
			// （壁側）を向いていればゴールとする。
			if((current.x == 0 && (currentCell.terminals & Terminal::kLeft)) ||
				(current.x == width_ - 1 && (currentCell.terminals & Terminal::kRight))){
				reachedGoal = true;
			}

			// 4方向を調べ、互いに向き合う端子ビットが両方立っているマスへ探索を広げる
			for(int32_t dir = 0; dir < 4; ++dir){
				const int32_t nx = current.x + kDirs[dir].x;
				const int32_t ny = current.y + kDirs[dir].y;

				if(!IsInside(nx,ny) || visited[ny][nx] || cells_[ny][nx].IsEmpty()){
					continue;
				}
				if(!(currentCell.terminals & kSelfBits[dir]) || !(cells_[ny][nx].terminals & kOtherBits[dir])){
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

		// 難易度ごとに、このかたまりから消す範囲を決める
		switch(difficulty_){
		case Difficulty::Easy:
			// かたまりが通っている横列に印を付ける。実際に集めるのはループを抜けたあと。
			for(const GridPos& pos : component){
				rowsToClear[pos.y] = true;
			}
			break;

		case Difficulty::Normal:
			// かたまりのマスをそのまま消す
			cellsToClear.insert(cellsToClear.end(),component.begin(),component.end());
			break;

		case Difficulty::Hard:{
			// 行き止まりの枝を取り除き、電源とゴールを結ぶマスだけを消す
			const std::vector<GridPos> trunk = PruneDeadEndCells(component);
			cellsToClear.insert(cellsToClear.end(),trunk.begin(),trunk.end());
			break;
		}

		default:
			break;
		}
	}

	// Easy のみ：印の付いた横列にあるマスを、通電しているかどうかに関係なくすべて集める。
	// 行単位で1回だけ走査するので重複しない（重複するとマス数を二重に数えて
	// スコアとスペシャルゲージがずれる）。
	if(difficulty_ == Difficulty::Easy){
		for(int32_t y = 0; y < PuzzleConfig::kBoardHeight; ++y){
			if(!rowsToClear[y]){
				continue;
			}
			for(int32_t x = 0; x < width_; ++x){
				if(cells_[y][x].IsEmpty()){
					continue;
				}
				cellsToClear.push_back({x, y});
			}
		}
	}

	// 消去対象があれば、即座には消さず消去演出の状態に入る。
	// 実際にマスを消す処理は Update() 側でタイマーが満了したときに行う。
	if(!cellsToClear.empty()){
		++chainCount_;
		clearingCells_ = std::move(cellsToClear);
		isClearing_ = true;
		clearTimer_ = 0;

		// 電源から見た行の距離のうち、いちばん遠いマスまでの距離を控えておく。
		// 消去演出中、この距離を基準に「光の波」が下段から上段へ届くまでの進み具合を計算する。
		clearWaveMaxDistance_ = 0;
		for(const GridPos& pos : clearingCells_){
			const int32_t distance = bottomY - pos.y;
			if(distance > clearWaveMaxDistance_){
				clearWaveMaxDistance_ = distance;
			}
		}
	}else{
		chainCount_ = 0;
	}
}

bool Board::ConvertToStrongest(int32_t x,int32_t y){
	if(IsBusy() || !IsInside(x,y)){
		return false;
	}
	Cell& cell = cells_[y][x];
	if(cell.IsEmpty() || cell.IsStrongest()){
		return false;
	}
	cell.MakeStrongest();
	chainCount_ = 0;

	// 追加：変換後のマスが自分の列で浮いた状態になっていないよう、
	// 通常の消去処理と同じくここでも列単位の落下処理をかける
	// （消去を経由しない変換ではこれまで ApplyGravity が呼ばれておらず、
	// 空中に浮いたまま表示されるバグがあったため）。
	ApplyGravity({{x,y}},{});

	ResolveConduction();
	RebuildCellObjects();
	return true;
}

std::vector<Board::ClearResult> Board::TakeClearResults(){
	std::vector<ClearResult> results;
	results.swap(clearResults_);
	return results;
}

// 空きマスを詰めるように、各列のマスをマス単位で下へ落とす。
// ブロックの形は保持しない（欠片ごとにバラバラに落ちる）。
// 対象にする列は次の2種類。
//   1. clearedCells に1マスでも含まれる列（今回の消去でマスが空いた列）
//   2. clearedBlockIds と同じ元ブロックIDの残骸が残っている列
//      （消去で同じ元ブロックの一部が失われ、支えを失って構造的に浮いた
//      可能性があるマス。自分の列自体には消去が起きていなくても対象にする）
// これ以外の、本当に無関係な列は触らない。もともと宙に浮いていた
// 出っ張りマスを巻き込んで落とさないようにするため。
// 対象になった列は、その列全体を空きマスが無くなるまで下に詰め直す
// （落下はマス単位で行い、ブロックの形は保持しない仕様のため）。
// 追加：Hard で使う。通電した一かたまりから、行き止まりの枝（他のマスと1本以下しか
// 繋がっていないマス）を繰り返し取り除き、残ったマスを返す。
// 残るのは電源とゴールを実際に結ぶのに使われているマスだけで、複数ルートやループも
// 行き止まりでなければ残る。
std::vector<GridPos> Board::PruneDeadEndCells(const std::vector<GridPos>& component) const{
	const int32_t bottomY = PuzzleConfig::kBoardHeight - 1;

	std::array<std::array<bool,PuzzleConfig::kBoardWidthMax>,PuzzleConfig::kBoardHeight> inSet{};
	std::array<std::array<int32_t,PuzzleConfig::kBoardWidthMax>,PuzzleConfig::kBoardHeight> degree{};

	for(const GridPos& pos : component){
		inSet[pos.y][pos.x] = true;
	}
	for(const GridPos& pos : component){
		const Cell& posCell = cells_[pos.y][pos.x];
		int32_t d = 0;
		for(int32_t dir = 0; dir < 4; ++dir){
			const int32_t nx = pos.x + kDirs[dir].x;
			const int32_t ny = pos.y + kDirs[dir].y;
			if(!IsInside(nx,ny) || !inSet[ny][nx]){
				continue;
			}
			// 位置が隣り合っていても、互いに向き合う端子ビットが両方立っていなければ
			// 繋がっているとは数えない（別ルート経由で同じ一かたまりに入っているだけの場合がある）
			if(!(posCell.terminals & kSelfBits[dir]) || !(cells_[ny][nx].terminals & kOtherBits[dir])){
				continue;
			}
			++d;
		}
		// 電源（最下段）・ゴール（左右端）は、外部と繋がっている仮想の1本があるものとして数える
		if(pos.y == bottomY){
			++d;
		}
		if(pos.x == 0 || pos.x == width_ - 1){
			++d;
		}
		degree[pos.y][pos.x] = d;
	}

	// 繋がりが1本以下のマスを取り除きの起点にする
	std::vector<GridPos> pruneQueue;
	for(const GridPos& pos : component){
		if(degree[pos.y][pos.x] <= 1){
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

		const Cell& currentCell = cells_[current.y][current.x];

		// 取り除いた分、端子ビットで実際に繋がっていた隣のマスだけ繋がり本数を減らし、
		// 1本以下になったら追加で取り除く
		for(int32_t dir = 0; dir < 4; ++dir){
			const int32_t nx = current.x + kDirs[dir].x;
			const int32_t ny = current.y + kDirs[dir].y;

			if(!IsInside(nx,ny) || !inSet[ny][nx]){
				continue;
			}
			if(!(currentCell.terminals & kSelfBits[dir]) || !(cells_[ny][nx].terminals & kOtherBits[dir])){
				continue;
			}

			--degree[ny][nx];
			if(degree[ny][nx] <= 1){
				pruneQueue.push_back({nx, ny});
			}
		}
	}

	// 取り除かれずに残ったマスを返す
	std::vector<GridPos> trunk;
	for(const GridPos& pos : component){
		if(inSet[pos.y][pos.x]){
			trunk.push_back(pos);
		}
	}
	return trunk;
}

void Board::ApplyGravity(const std::vector<GridPos>& clearedCells,const std::vector<int32_t>& clearedBlockIds,bool forceAllColumns){
	// どの列を対象にするかを、列単位のフラグに変換しておく
	std::array<bool,PuzzleConfig::kBoardWidthMax> clearedColumns{};

	// Easy の横列消去は行を丸ごと消す仕様のため、その行に元々空きマスだった
	// 列も含めて全列を強制的に詰め直す（そうしないと、その列だけ上のブロックが
	// 落ちてこず、他の列との間で高さがずれて見える）。
	if(forceAllColumns){
		for(int32_t x = 0; x < width_; ++x){
			clearedColumns[x] = true;
		}
	}

	// 1. 今回の消去でマスが空いた列
	for(const GridPos& pos : clearedCells){
		if(pos.x >= 0 && pos.x < width_){
			clearedColumns[pos.x] = true;
		}
	}

	// 2. 消えたマスと同じ元ブロックIDの残骸が残っている列
	if(!clearedBlockIds.empty()){
		for(int32_t y = 0; y < PuzzleConfig::kBoardHeight; ++y){
			for(int32_t x = 0; x < width_; ++x){
				if(cells_[y][x].IsEmpty()){
					continue;
				}
				for(int32_t id : clearedBlockIds){
					if(cells_[y][x].blockId == id){
						clearedColumns[x] = true;
						break;
					}
				}
			}
		}
	}

	for(int32_t x = 0; x < width_; ++x){
		// 消去が起きていない列は触らない
		if(!clearedColumns[x]){
			continue;
		}

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

// 変更：盤面の幅を切り替える。壁の位置・固定マスの見た目は幅に依存するため、
// width_ を書き換えるだけでなくここで作り直す。
void Board::SetWidth(int32_t width){
	// 想定外の値・変化なしは無視する
	if(width <= 0 || width > PuzzleConfig::kBoardWidthMax || width == width_){
		return;
	}

	width_ = width;

	// 盤面データを作り直す。幅が変わると列の意味が変わり、元の幅で埋まっていた
	// マスをそのまま残すと壁の外に隠れて存在してしまう（幅を戻すと復活する）ため、
	// 固定マス・消去演出の状態はいったんクリアする。
	cells_ = {};
	clearingCells_.clear();
	isClearing_ = false;
	clearTimer_ = 0;
	chainCount_ = 0;

	// 壁を新しい幅の位置・範囲で作り直す
	RebuildWalls();

	// 固定マスの見た目も作り直す（cells_をクリアしたので実質空になる）
	RebuildCellObjects();
}

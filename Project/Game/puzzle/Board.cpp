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

			if(Model::Material* cellMaterial = obj->GetMaterial()){
				cellMaterial->color = kFilledCellColor;
				cellMaterial->enableLighting = 0; // 2D的な見た目にするため陰影を切る
			}

			cellObjs_.push_back(std::move(obj));
		}
	}
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

// 追加：指定したマス群へ blockId を書き込んで盤面に固定する。
void Board::Place(const std::vector<GridPos>& cells,int32_t blockId){
	for(const GridPos& pos : cells){
		// 安全のため範囲外のマスには書き込まない
		if(!IsInside(pos.x,pos.y)){
			continue;
		}
		cells_[pos.y][pos.x].blockId = blockId;
	}

	// 追加：盤面に置かれたマスの見た目を作り直す
	RebuildCellObjects();
}

// 追加：落下中ブロック用の配置可否判定。
bool Board::CanFall(const std::vector<GridPos>& cells) const{
	for(const GridPos& pos : cells){
		// 左右の壁の外に出るマスがあれば不可
		if(pos.x < 0 || pos.x >= width_){
			return false;
		}
		// 床より下に出るマスがあれば不可
		if(pos.y >= PuzzleConfig::kBoardHeight){
			return false;
		}
		// 天井より上（y < 0）は空中。盤面データを持たないので衝突しない
		if(pos.y < 0){
			continue;
		}
		// 盤面内で既に別のブロックに埋まっているマスがあれば不可
		if(!cells_[pos.y][pos.x].IsEmpty()){
			return false;
		}
	}
	return true;
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

#include "Board.h"

#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "Model.h"

// このファイル内だけで使う定数
namespace{

	// 壁ブロックに使うモデル（当面は defaultBlock のキューブで代用する）
	const std::string kBlockModel = "defaultBlock/defaultBlock.obj";

	// U字の壁ブロックの色
	const Vector4 kWallColor = {0.35f, 0.38f, 0.48f, 1.0f};
}

// コンストラクタ・デストラクタ
// Obj3D の完全な型が見えるこの場所で定義する必要がある
Board::Board() = default;
Board::~Board() = default;

// モデルの読み込みとオブジェクト生成などの初期化
void Board::Initialize(Obj3dCommon* object3dCommon){
	object3dCommon_ = object3dCommon;

	// 共通モデルを読み込む
	ModelManager::GetInstance()->LoadModel(kBlockModel);

	// --- U字の壁ブロックの生成 ---
	// 盤面のマス領域のすぐ外側を、左・下・右の順に囲む。上辺は開けておく（U字）。
	for(int32_t y = 0; y < PuzzleConfig::kBoardHeight; ++y){
		CreateWallBlock(-1,y);           // 左の壁
	}
	for(int32_t y = 0; y < PuzzleConfig::kBoardHeight; ++y){
		CreateWallBlock(width_,y);       // 右の壁
	}
	for(int32_t x = -1; x <= width_; ++x){
		CreateWallBlock(x,PuzzleConfig::kBoardHeight); // 下の壁（左右の角を含む）
	}
}

// U字の壁ブロックを1個生成して wallObjs_ に追加する
void Board::CreateWallBlock(int32_t x,int32_t y){
	auto obj = std::make_unique<Obj3D>();
	obj->Initialize(object3dCommon_);
	obj->SetModel(kBlockModel);
	obj->SetScale({PuzzleConfig::kCellModelScale, PuzzleConfig::kCellModelScale, PuzzleConfig::kCellModelScale});
	obj->SetTranslate(GridToWorld(x,y));

	if(Model::Material* wallMaterial = obj->GetMaterial()){
		wallMaterial->color = kWallColor;
		wallMaterial->enableLighting = 0; // 2D的な見た目にするため陰影を切る
	}

	wallObjs_.push_back(std::move(obj));
}

// 毎フレームの更新
void Board::Update(){
	// カメラ移動などに追従できるよう、行列は毎フレーム更新する。
	for(auto& wallObj : wallObjs_){
		wallObj->Update();
	}
}

// 盤面（U字の壁）の描画
void Board::Draw(){
	for(auto& wallObj : wallObjs_){
		wallObj->Draw();
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

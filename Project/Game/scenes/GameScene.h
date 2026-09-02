#pragma once

#include "systems/BaseScene.h"
#include "MyMath.h"
#include "LevelManager.h"
#include "Board.h" // 追加：パズルの盤面クラス
#include "FallingBlock.h" // 追加：落下中のブロック
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

// 前方宣言
class Input;
class Obj3D;
class Obj3dCommon;
class SpriteCommon;
class Application;

class GameScene : public BaseScene{
public:
	GameScene();
	~GameScene() override;

	// シーン管理
	void Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	// 外部依存
	Obj3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;
	Application* app_ = nullptr;

	// 設定・状態
	const std::string kBgmPath_ = "resource/You_and_Me.mp3";
	bool isPaused_ = false;

	// レベル配置オブジェクト
	std::vector<std::shared_ptr<Obj3D>> levelObjects_;

	// レベルJSONの読み込み・更新監視マネージャー
	LevelManager levelManager_;

	// 追加：パズルの盤面（10×10の壁とマスを描画する）
	Board board_;

	// 追加：落下中のブロック（現状はT字のみ出現する）
	FallingBlock fallingBlock_;

	// 追加：落下中ブロックの各マスを描画する3Dオブジェクト
	std::vector<std::unique_ptr<Obj3D>> fallingObjs_;

	// 追加：着地予測（ゴースト。「ここに落とすとこうなる」の表示）の各マスを描画する3Dオブジェクト
	std::vector<std::unique_ptr<Obj3D>> ghostObjs_;

	// 追加：次に出現するブロックへ振る元ブロックID。出現のたびに増やす。
	int32_t nextBlockId_ = 0;

	// 追加：次に出現するブロックの種類（ネクスト表示に使う）
	BlockShape::Type nextType_ = BlockShape::Type::T;

	// 追加：次ブロックの種類抽選に使う乱数エンジン（Initializeでシードする）
	std::mt19937 randomEngine_;

	// 追加：天井到達などでこれ以上ブロックを出せない状態かどうか
	bool isGameOver_ = false;

	// レベル配置データからオブジェクトを再構築する
	void RebuildLevelObjects();

	// 追加：落下中ブロックの描画オブジェクトの位置を、現在の占有マスに合わせる
	void SyncFallingObjs();

	// 追加：nextType_ のブロックを出現させ、次の nextType_ を抽選する。
	// 出現できなかった場合は false を返す（天井まで積み上がった＝ゲームオーバー）。
	bool SpawnNextBlock();

	// 追加：次に落ちてくるブロックの種類をひとつ抽選して返す。
	BlockShape::Type PickNextBlockType();

	// 追加：ImGui にネクスト（次に落ちてくるブロック）を表示する。
	void ShowNextBlockGui() const;

	// レベル配置オブジェクトを描画するかどうか（ImGuiで切り替え）
	bool isLevelObjectsVisible_ = false;
};
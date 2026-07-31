#pragma once

#include "systems/BaseScene.h"
#include "MyMath.h"
#include "LevelManager.h"
#include <memory>
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

	// レベル配置データからオブジェクトを再構築する
	void RebuildLevelObjects();

	// レベル配置オブジェクトを描画するかどうか（ImGuiで切り替え）
	bool isLevelObjectsVisible_ = false;
};
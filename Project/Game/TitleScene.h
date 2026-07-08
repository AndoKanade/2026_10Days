#pragma once

#include "BaseScene.h"
#include "MyMath.h"
#include <memory>
#include <string>

// --- 前方宣言 ---
class Input;
class Obj3D;
class Obj3dCommon;
class Sprite;
class SpriteCommon;

/// <summary>
/// タイトル画面シーン
/// </summary>
class TitleScene : public BaseScene{
public:
	// --- コンストラクタ・デストラクタ ---
	TitleScene();
	~TitleScene() override;

	// --- BaseScene オーバーライド ---
	void Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	// --- メンバ変数：外部依存 (借りてくるもの) ---
	Obj3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;

	// --- メンバ変数：内部リソース (所有するもの) ---
	std::unique_ptr<Obj3D> titleObject_;
	std::unique_ptr<Sprite> sprite_;

	// --- メンバ変数：パラメータ ---
	Vector4 spriteColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
};
#pragma once

#include "BaseScene.h"
#include "MyMath.h"
#include <memory>
#include <string>
#include <vector>

// 前方宣言
class Input;
class Obj3D;
class Obj3dCommon;
class ParticleEmitter;
class SpriteCommon;
class Skybox;
class SkyboxCommon;
class Application;
class Animation;
class AnimationController;

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

	// 3Dオブジェクト
	std::shared_ptr<Obj3D> planeObj_;
	std::shared_ptr<Obj3D> fenceObj_;
	std::shared_ptr<Obj3D> sphereObj_;
	std::shared_ptr<Obj3D> terrainObj_;
	std::shared_ptr<Obj3D> simpleSkinObj_;
	std::shared_ptr<Obj3D> animationCube_;
	std::shared_ptr<Obj3D> humanObj_;

	// パーティクル管理
	std::unique_ptr<ParticleEmitter> ringEmitter_;
	std::unique_ptr<ParticleEmitter> circleEmitter_;
	std::unique_ptr<ParticleEmitter> cylinderEmitter_;
	std::unique_ptr<ParticleEmitter> shockwaveEmitter_;

	// 環境・エフェクト
	std::unique_ptr<SkyboxCommon> skyboxCommon_;
	std::unique_ptr<Skybox> skybox_;

	// アニメーション
	std::unique_ptr<Animation> animation_;
	std::unique_ptr<AnimationController> animationController_;
	float animationTime_ = 0.0f;

	// 設定・状態
	const std::string kBgmPath_ = "resource/You_and_Me.mp3";
	bool isPaused_ = false;
};
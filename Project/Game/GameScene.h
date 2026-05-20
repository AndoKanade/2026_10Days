#pragma once
#include "BaseScene.h"
#include "Animation.h"
#include <memory>
#include <string>

class Input;
class Obj3D;
class Obj3dCommon;
class ParticleEmitter;
class SpriteCommon;
class Skybox;
class SkyboxCommon;
class Application;

class GameScene : public BaseScene{
public:
	GameScene();
	~GameScene() override;

	void Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	// 外部依存ポインタ
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

	// パーティクル
	std::unique_ptr<ParticleEmitter> ringEmitter_;
	std::unique_ptr<ParticleEmitter> circleEmitter_;
	std::unique_ptr<ParticleEmitter> cylinderEmitter_;

	// スカイボックス
	std::unique_ptr<SkyboxCommon> skyboxCommon_;
	std::unique_ptr<Skybox> skybox_;

	// アニメーションデータと制御
	Animation animation_;
	AnimationController animationController_;
	float animationTime_ = 0.0f;

	// 音声と状態フラグ
	const std::string kBgmPath_ = "resource/You_and_Me.mp3";
	bool isPaused_ = false;
};
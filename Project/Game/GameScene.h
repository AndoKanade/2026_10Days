#pragma once
#include "BaseScene.h"
#include <memory>
#include <string>

class Input;
class Obj3D;
class Obj3dCommon;
class ParticleEmitter;
class SpriteCommon;
class Skybox;
class SkyboxCommon;

class GameScene : public BaseScene{
public:
	GameScene();
	~GameScene() override;

	void Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	// 外部依存
	Obj3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;

	// 内部リソース
	std::shared_ptr<Obj3D> planeObj_;
	std::shared_ptr<Obj3D> fenceObj_;
	std::shared_ptr<Obj3D> sphereObj_;
	std::shared_ptr<Obj3D> terrainObj_;
	std::unique_ptr<ParticleEmitter> particleEmitter_;

	std::unique_ptr<SkyboxCommon> skyboxCommon_;
	std::unique_ptr<Skybox> skybox_;

	// パラメータ・フラグ
	const std::string kBgmPath_ = "resource/You_and_Me.mp3";
	bool isPaused_ = false;
};
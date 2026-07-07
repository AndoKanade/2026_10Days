#include "GameScene.h"
#include "CameraManager.h"
#include "ImGuiManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "SoundManager.h"
#include "TextureManager.h"
#include "Skybox.h"
#include "SkyboxCommon.h"
#include "Input.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ParticleEmitter.h"
#include "SpriteCommon.h"
#include "Animation.h"
#include "Application.h"
#include "Logger.h"

namespace{
	const std::string kTextureChecker = "resource/uvChecker.png";
	const std::string kTextureBall = "resource/Sphere/monsterball.png";
	const std::string kTextureCircle = "resource/circle.png";
	const std::string kSkyboxTexture = "resource/Skybox/rostock_laage_airport_4k.dds";
	const std::string kTextureCircle2 = "resource/circle2.png";
	const std::string kTexturegradationLine = "resource/gradationLine.png";

	const std::string kModelPlane = "Plane/plane.obj";
	const std::string kModelFence = "Fence/fence.obj";
	const std::string kModelSphere = "Sphere/sphere.obj";
	const std::string kModelTerrain = "Terrain/terrain.obj";
	const std::string kModelSimpleSkin = "simpleSkin/simpleSkin.gltf";
	const std::string kModelAnimationCube = "AnimatedCube/AnimatedCube.gltf";
	const std::string kModelHuman = "human/walk.gltf";

	const std::string kParticlePrimitive = "Circle";
	const std::string kParticleRing = "Ring";
	const std::string kParticleCylinder = "Cylinder";
}

GameScene::GameScene() = default;
GameScene::~GameScene() = default;

// 初期化
void GameScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// カメラの生成・設定
	CameraManager::GetInstance()->CreateCamera("default",object3dCommon_->GetDxCommon()->GetDevice());
	auto* defaultCamera = CameraManager::GetInstance()->GetCamera("default");
	defaultCamera->SetTranslate({0.0f, 0.0f, -30.0f});
	CameraManager::GetInstance()->SetActiveCamera("default");
	object3dCommon_->SetDefaultCamera(CameraManager::GetInstance()->GetActiveCamera());

	// リソースのロード
	TextureManager::GetInstance()->LoadTexture(kTextureChecker);
	TextureManager::GetInstance()->LoadTexture(kTextureBall);
	TextureManager::GetInstance()->LoadTexture(kTextureCircle);
	TextureManager::GetInstance()->LoadTexture(kSkyboxTexture);
	TextureManager::GetInstance()->LoadTexture(kTextureCircle2);
	TextureManager::GetInstance()->LoadTexture(kTexturegradationLine);

	ModelManager::GetInstance()->LoadModel(kModelPlane);
	ModelManager::GetInstance()->LoadModel(kModelFence);
	ModelManager::GetInstance()->LoadModel(kModelSphere);
	ModelManager::GetInstance()->LoadModel(kModelTerrain);
	ModelManager::GetInstance()->LoadModel(kModelSimpleSkin);
	ModelManager::GetInstance()->LoadModel(kModelAnimationCube);
	ModelManager::GetInstance()->LoadModel(kModelHuman);

	SoundManager::GetInstance()->SoundLoadFile(kBgmPath_);

	// オブジェクトの生成と初期化
	planeObj_ = std::make_unique<Obj3D>();
	planeObj_->Initialize(object3dCommon_);
	planeObj_->SetModel(kModelPlane);
	planeObj_->SetTexture(kTextureCircle2);

	fenceObj_ = std::make_unique<Obj3D>();
	fenceObj_->Initialize(object3dCommon_);
	fenceObj_->SetModel(kModelFence);
	fenceObj_->SetParent(planeObj_);
	fenceObj_->SetTranslate({2.0f, 0.0f, 0.0f});

	sphereObj_ = std::make_unique<Obj3D>();
	sphereObj_->Initialize(object3dCommon_);
	sphereObj_->SetModel(kModelSphere);

	terrainObj_ = std::make_unique<Obj3D>();
	terrainObj_->Initialize(object3dCommon_);
	terrainObj_->SetModel(kModelTerrain);

	simpleSkinObj_ = std::make_unique<Obj3D>();
	simpleSkinObj_->Initialize(object3dCommon_);
	simpleSkinObj_->SetModel(kModelSimpleSkin);
	if(auto* material = simpleSkinObj_->GetMaterial()){
		material->environmentCoefficient = 1.0f;
	}

	// スカイボックスの生成
	skyboxCommon_ = std::make_unique<SkyboxCommon>();
	skyboxCommon_->Initialize(object3dCommon_->GetDxCommon());
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(skyboxCommon_.get(),kSkyboxTexture);

	// アニメーションオブジェクトの生成
	animationCube_ = std::make_shared<Obj3D>();
	animationCube_->Initialize(object3dCommon_);
	animationCube_->SetModel(kModelAnimationCube);

	animation_ = std::make_unique<Animation>();
	*animation_ = LoadAnimationFile("resource/AnimatedCube/","AnimatedCube.gltf");
	animationController_ = std::make_unique<AnimationController>();
	animationController_->Initialize();
	animationController_->Play();

	// キャラクターとスケルトンの生成
	humanObj_ = std::make_shared<Obj3D>();
	humanObj_->Initialize(object3dCommon_);
	humanObj_->SetModel(kModelHuman);
	humanObj_->SetTexture("resource/human/white.png");
	humanObj_->LoadAnimation("resource/human/","walk.gltf");

	// パーティクルの設定
	ParticleManager::GetInstance()->CreateParticleGroup(kParticleRing,kTexturegradationLine,true,false);
	Transform ringConfig = {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 0.0f}};
	ringEmitter_ = std::make_unique<ParticleEmitter>(kParticleRing,ringConfig,1,0.5f);
	ringEmitter_->SetVelocity({0.0f, 0.5f, 0.0f});
	ringEmitter_->SetLifeTime(1.0f);

	ParticleManager::GetInstance()->CreateParticleGroup(kParticlePrimitive,kTextureCircle2,false,false);
	Transform circleConfig = {{0.05f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 0.0f}};
	circleEmitter_ = std::make_unique<ParticleEmitter>(kParticlePrimitive,circleConfig,3,0.5f);
	circleEmitter_->SetVelocity({0.0f, 0.5f, 0.0f});
	circleEmitter_->SetLifeTime(1.0f);

	ParticleManager::GetInstance()->CreateParticleGroup(kParticleCylinder,kTexturegradationLine,false,true);
	Transform cylinderConfig = {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
	cylinderEmitter_ = std::make_unique<ParticleEmitter>(kParticleCylinder,cylinderConfig,1,0.1f);
	cylinderEmitter_->SetColor({0.2f, 0.5f, 1.0f, 0.8f});
	cylinderEmitter_->SetLifeTime(2.0f);
	cylinderEmitter_->SetVelocity({0.0f, 0.0f, 0.0f});

	ParticleManager::GetInstance()->CreateParticleGroup("Shockwave",kTexturegradationLine,true,false,true);
	ParticleManager::GetInstance()->CreateParticleGroup("Spark",kTextureCircle2,false,false,false,true);
	ParticleManager::GetInstance()->CreateParticleGroup("Smoke",kTextureCircle2,false,false,false,false,true);
	ParticleManager::GetInstance()->CreateParticleGroup("Charge",kTexturegradationLine,false,false,false,false,false,true);
	ParticleManager::GetInstance()->CreateParticleGroup("Aura",kTextureCircle2,false,false,false,false,false,false,true);
	ParticleManager::GetInstance()->CreateParticleGroup("Warp",kTexturegradationLine,false,true,false,false,false,false,false,true);
}

void GameScene::Finalize(){}

// 更新処理
void GameScene::Update(){
	// オブジェクトの更新
	if(sphereObj_) sphereObj_->Update();
	if(terrainObj_) terrainObj_->Update();
	if(simpleSkinObj_) simpleSkinObj_->Update();
	if(skybox_) skybox_->Update(*CameraManager::GetInstance()->GetActiveCamera());
	if(planeObj_) planeObj_->Update();

	// 入力イベント
	if(input_->TriggerKey(DIK_1)) EmitShockwave({0.0f, 0.0f, 0.0f});
	if(input_->TriggerKey(DIK_2)) EmitSpark({0.0f, 0.0f, 0.0f});
	if(input_->TriggerKey(DIK_3)) EmitSmoke({0.0f, 0.0f, 0.0f});
	if(input_->TriggerKey(DIK_4)) EmitCharge({0.0f, 0.0f, 0.0f});
	if(input_->TriggerKey(DIK_5)) EmitAura({0.0f, 0.0f, 0.0f});
	if(input_->TriggerKey(DIK_6)) EmitWarp();

	// キャラクター更新
	if(humanObj_){
		humanObj_->SetTranslate({0, 0, 5});
		humanObj_->SetScale({1, 1, 1});
		if(Camera* cam = CameraManager::GetInstance()->GetActiveCamera()) humanObj_->SetCamera(cam);
		humanObj_->Update();
	}

	// アニメーション更新
	animationController_->UpdateKeyframes(*animation_,1.0f / 60.0f);
	if(animationCube_){
		animationCube_->SetScale(animationController_->GetCurrentScale());
		animationCube_->SetTranslate(animationController_->GetCurrentTranslate());
		animationCube_->SetQuaternion(animationController_->GetCurrentRotate());
		animationCube_->Update();
	}

	// パーティクル更新
	if(Camera* cam = CameraManager::GetInstance()->GetActiveCamera()){
		ParticleManager::GetInstance()->Update(cam);
	}

	// --- デバッグUIの表示 ---
#ifdef USE_IMGUI
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		// --- メインデバッグウィンドウ ---
		ImGui::Begin("GameScene Debug");

		// カメラ設定
		if(ImGui::CollapsingHeader("Camera Settings")){
			Vector3 camPos = activeCamera->GetTranslate();
			if(ImGui::DragFloat3("Camera Pos",&camPos.x,0.1f)) activeCamera->SetTranslate(camPos);

			Vector3 camRot = activeCamera->GetRotate();
			if(ImGui::DragFloat3("Camera Rotate",&camRot.x,0.01f)) activeCamera->SetRotate(camRot);
		}

		// オブジェクト操作
		if(planeObj_ && ImGui::CollapsingHeader("Object Settings")){
			Vector3 pPos = planeObj_->GetTranslate();
			if(ImGui::DragFloat3("Parent(Plane) Pos",&pPos.x,0.1f)) planeObj_->SetTranslate(pPos);
		}

		// ライティング
		if(ImGui::CollapsingHeader("Lighting")){
			if(PointLight* pData = object3dCommon_->GetPointLightData()){
				ImGui::Text("Point Light");
				ImGui::ColorEdit4("Point Color",&pData->color.x);
				ImGui::DragFloat3("Point Pos",&pData->position.x,0.1f);
				ImGui::DragFloat("Point Intensity",&pData->intensity,0.1f,0.0f,100.0f);
			}
			if(SpotLight* sData = object3dCommon_->GetSpotLightData()){
				ImGui::Text("Spot Light");
				ImGui::ColorEdit4("Spot Color",&sData->color.x);
				// ... (SpotLightの各パラメータ)
			}
		}

		// キャラクターボーン制御
		if(humanObj_ && ImGui::CollapsingHeader("Character Bone Control")){
			auto& skeleton = humanObj_->GetSkeleton();
			if(!skeleton.joints.empty()){
				auto& rootJoint = skeleton.joints[0];
				static float rootPos[3] = {0, 0, 0};
				static float rootRot[3] = {0, 0, 0};
				static bool isManualControl = false;

				if(ImGui::DragFloat3("Root Pos",rootPos,0.1f)){
					rootJoint.transform.translate = {rootPos[0], rootPos[1], rootPos[2]};
				}
				if(ImGui::DragFloat3("Root Rot (Euler)",rootRot,0.01f)){
					rootJoint.transform.rotate = MakeQuaternionFromEuler(rootRot[0],rootRot[1],rootRot[2]);
				}

				if(ImGui::Checkbox("Manual Control",&isManualControl)){
					// 手動操作へ切り替わった瞬間に反映
					if(isManualControl) skeleton.Update();
				}
			}
		}

		ModelManager::GetInstance()->UpdateLightGui();
		ImGui::End();

		// --- パーティクル制御ウィンドウ ---
		ImGui::Begin("Particle Control");
		const char* particleButtons[] = {
			"Shockwave", "Spark", "Ring", "Circle", "Cylinder", "Smoke", "Charge", "Aura", "Warp"
		};

		// 各パーティクル発行ボタン
		if(ImGui::Button("Emit Shockwave")) EmitShockwave({0,0,0}); ImGui::SameLine();
		if(ImGui::Button("Emit Spark")) EmitSpark({0,0,0});
		if(ImGui::Button("Emit Ring")){ if(ringEmitter_) ringEmitter_->Emit(); } ImGui::SameLine();
		if(ImGui::Button("Emit Circle")){ if(circleEmitter_) circleEmitter_->Emit(); }
		// ... (他のボタンも同様)
		ImGui::End();

		Application::GetInstance()->ShowPostProcessUI();
	}
#endif
}


// 描画処理
void GameScene::Draw(){
	object3dCommon_->Draw();
	if(humanObj_) humanObj_->Draw();

	if(Camera* cam = CameraManager::GetInstance()->GetActiveCamera()){
		Matrix4x4 viewProj = Multiply(cam->GetViewMatrix(),cam->GetProjectionMatrix());
		ParticleManager::GetInstance()->Draw(viewProj);
	}
}

// パーティクル発生処理
void GameScene::EmitShockwave(const Vector3& pos){
	ParticleManager::GetInstance()->Emit("Shockwave",{{0.1f, 0.1f, 0.1f}, {0, 0, 0}, pos},1,{1,1,1,1},{0,0,0},0.3f);
}

void GameScene::EmitSpark(const Vector3& position){
	Transform transform;
	transform.translate = position;
	transform.scale = {0.05f, 0.05f, 0.05f};
	transform.rotate = {0.0f, 0.0f, 0.0f};

	ParticleManager::GetInstance()->Emit(
		"Spark",transform,20,{1.0f, 0.5f, 0.0f, 1.0f},{0.0f, 0.0f, 0.0f},0.5f
	);
}

void GameScene::EmitSmoke(const Vector3& position){
	Transform transform;
	transform.translate = position;
	transform.scale = {0.2f, 0.2f, 0.2f};
	transform.rotate = {0.0f, 0.0f, 0.0f};

	ParticleManager::GetInstance()->Emit(
		"Smoke",transform,5,{0.5f, 0.5f, 0.5f, 0.8f},{0.0f, 1.0f, 0.0f},1.5f
	);
}

void GameScene::EmitCharge(const Vector3& position){
	Transform transform;
	transform.translate = position;
	transform.scale = {0.1f, 0.5f, 0.1f};
	transform.rotate = {0.0f, 0.0f, 0.0f};

	ParticleManager::GetInstance()->Emit(
		"Charge",transform,30,{0.2f, 0.8f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f},0.6f
	);
}

void GameScene::EmitAura(const Vector3& position){
	Transform transform;
	transform.translate = position;
	transform.scale = {0.3f, 0.3f, 0.3f};
	transform.rotate = {0.0f, 0.0f, 0.0f};

	ParticleManager::GetInstance()->Emit(
		"Aura",transform,1,{0.8f, 1.0f, 0.2f, 0.6f},{0.0f, 2.0f, 0.0f},1.0f
	);
}

void GameScene::EmitWarp(){
	Transform transform;
	transform.translate = {0.0f, 0.0f, 0.0f};
	transform.scale = {1.0f, 1.0f, 1.0f};
	transform.rotate = {0.0f, 0.0f, 0.0f};

	ParticleManager::GetInstance()->Emit(
		"Warp",transform,100,{0.5f, 0.8f, 1.0f, 0.8f},{0.0f, 0.0f, 0.0f},1.0f
	);
}

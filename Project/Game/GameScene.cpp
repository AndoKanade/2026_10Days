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

void GameScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// 1. カメラを先に生成・登録
	CameraManager::GetInstance()->CreateCamera("default",object3dCommon_->GetDxCommon()->GetDevice());
	auto* defaultCamera = CameraManager::GetInstance()->GetCamera("default");
	defaultCamera->SetTranslate({0.0f, 0.0f, -30.0f});
	CameraManager::GetInstance()->SetActiveCamera("default");

	// 2. ★超重要：ここを呼び出してから Obj3D を初期化する
	object3dCommon_->SetDefaultCamera(CameraManager::GetInstance()->GetActiveCamera());

	// --- リソースのロード ---
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

	// --- オブジェクトの生成と初期化 ---
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
	auto* simpleSkinMaterial = simpleSkinObj_->GetMaterial();
	if(simpleSkinMaterial){
		simpleSkinMaterial->environmentCoefficient = 1.0f;
	}

	// --- スカイボックスの生成と初期化 ---
	skyboxCommon_ = std::make_unique<SkyboxCommon>();
	skyboxCommon_->Initialize(object3dCommon_->GetDxCommon());
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(skyboxCommon_.get(),kSkyboxTexture);

	// --- アニメーションオブジェクトの生成と初期化 ---
	animationCube_ = std::make_shared<Obj3D>();
	animationCube_->Initialize(object3dCommon_);
	animationCube_->SetModel(kModelAnimationCube);

	animation_ = std::make_unique<Animation>();
	*animation_ = LoadAnimationFile("resource/AnimatedCube/","AnimatedCube.gltf");
	animationController_ = std::make_unique<AnimationController>();
	animationController_->Initialize();
	animationController_->Play();

	// --- キャラクターとスケルトンの生成と初期化 ---
	humanObj_ = std::make_shared<Obj3D>();
	humanObj_->Initialize(object3dCommon_);
	humanObj_->SetModel(kModelHuman);
	humanObj_->SetTexture("resource/human/white.png");

	// ★ ここで読み込み結果を必ずチェック
	auto* model = ModelManager::GetInstance()->FindModel(kModelHuman);
	if(!model){
		OutputDebugStringA("FATAL ERROR: Model not found!\n");
		return;
	}

	// ★ ここでスケルトンの基になるノードがあるかチェック
	if(model->GetRootNode().name.empty()){
		OutputDebugStringA("FATAL ERROR: RootNode is empty!\n");
		return;
	}

	humanObj_->LoadAnimation("resource/human/","walk.gltf");

	// --- パーティクルの設定 ---
	ParticleManager::GetInstance()->CreateParticleGroup(kParticleRing,kTexturegradationLine,true,false);
	Transform ringConfig;
	ringConfig.translate = {1.0f, 2.0f, 0.0f};
	ringConfig.scale = {0.5f, 0.5f, 0.5f};
	ringEmitter_ = std::make_unique<ParticleEmitter>(kParticleRing,ringConfig,1,0.5f);
	ringEmitter_->SetVelocity({0.0f, 0.5f, 0.0f});
	ringEmitter_->SetLifeTime(1.0f);

	ParticleManager::GetInstance()->CreateParticleGroup(kParticlePrimitive,kTextureCircle2,false,false);
	Transform circleConfig;
	circleConfig.translate = {1.0f, 2.0f, 0.0f};
	circleConfig.scale = {0.05f, 1.0f, 1.0f};
	circleEmitter_ = std::make_unique<ParticleEmitter>(kParticlePrimitive,circleConfig,3,0.5f);
	circleEmitter_->SetVelocity({0.0f, 0.5f, 0.0f});
	circleEmitter_->SetLifeTime(1.0f);

	ParticleManager::GetInstance()->CreateParticleGroup(kParticleCylinder,kTexturegradationLine,false,true);
	Transform cylinderConfig;
	cylinderConfig.translate = {0.0f, 0.0f, 0.0f};
	cylinderConfig.scale = {1.0f, 1.0f, 1.0f};
	cylinderEmitter_ = std::make_unique<ParticleEmitter>(kParticleCylinder,cylinderConfig,1,0.1f);
	cylinderEmitter_->SetColor({0.2f, 0.5f, 1.0f, 0.8f});
	cylinderEmitter_->SetLifeTime(2.0f);
	cylinderEmitter_->SetVelocity({0.0f, 0.0f, 0.0f});
}

void GameScene::Finalize(){}

void GameScene::Update(){
	// --- 基本オブジェクトの更新 ---
	if(sphereObj_) sphereObj_->Update();
	if(terrainObj_) terrainObj_->Update();
	if(simpleSkinObj_) simpleSkinObj_->Update();
	if(skybox_) skybox_->Update(*CameraManager::GetInstance()->GetActiveCamera());
	if(planeObj_) planeObj_->Update();

	// --- パーティクルの更新 ---
	if(ringEmitter_) ringEmitter_->Update();
	if(circleEmitter_) circleEmitter_->Update();
	if(cylinderEmitter_) cylinderEmitter_->Update();

	// --- キャラクターアニメーションと骨格の更新 ---
	if(humanObj_){
		humanObj_->SetTranslate({0, 0, 5});
		humanObj_->SetScale({1, 1, 1});

		Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
		if(activeCamera){
			humanObj_->SetCamera(activeCamera);
		}
		humanObj_->Update();
	}

	// --- アニメーションキューブの更新 ---
	animationController_->UpdateKeyframes(*animation_,1.0f / 60.0f);
	if(animationCube_){
		animationCube_->SetScale(animationController_->GetCurrentScale());
		animationCube_->SetTranslate(animationController_->GetCurrentTranslate());
		animationCube_->SetQuaternion(animationController_->GetCurrentRotate());
		animationCube_->Update();
	}

	// --- カメラ依存の更新 ---
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	if(activeCamera){
		ParticleManager::GetInstance()->Update(activeCamera);
	}

	// --- デバッグUIの表示 ---
#ifdef USE_IMGUI
	if(activeCamera){
		ImGui::Begin("GameScene Debug");

		Vector3 camPos = activeCamera->GetTranslate();
		if(ImGui::DragFloat3("Camera Pos",&camPos.x,0.1f)){
			activeCamera->SetTranslate(camPos);
		}

		Vector3 camRot = activeCamera->GetRotate();
		if(ImGui::DragFloat3("Camera Rotate",&camRot.x,0.01f)){
			activeCamera->SetRotate(camRot);
		}

		if(planeObj_){
			Vector3 pPos = planeObj_->GetTranslate();
			ImGui::DragFloat3("Parent(Plane) Pos",&pPos.x,0.1f);
			planeObj_->SetTranslate(pPos);
		}

		ImGui::Separator();
		ImGui::Text("Point Light");
		PointLight* pData = object3dCommon_->GetPointLightData();
		if(pData){
			ImGui::ColorEdit4("Point Color",&pData->color.x);
			ImGui::DragFloat3("Point Pos",&pData->position.x,0.1f);
			ImGui::DragFloat("Point Intensity",&pData->intensity,0.1f,0.0f,100.0f);
			ImGui::DragFloat("Point Radius",&pData->radius,0.1f,0.0f,100.0f);
			ImGui::DragFloat("Point Decay",&pData->decay,0.1f,0.0f,10.0f);
		} else{
			ImGui::Text("PointLight Data is Null!");
		}

		SpotLight* sData = object3dCommon_->GetSpotLightData();
		if(sData){
			ImGui::Separator();
			ImGui::Text("Spot Light");
			ImGui::ColorEdit4("Spot Color",&sData->color.x);
			ImGui::DragFloat3("Spot Pos",&sData->position.x,0.1f);
			ImGui::DragFloat3("Spot Dir",&sData->direction.x,0.01f);
			ImGui::DragFloat("Spot Intensity",&sData->intensity,0.1f);
			ImGui::DragFloat("Spot Distance",&sData->distance,0.1f);

			static float spotDegree = 30.0f;
			static float spotDegreeStart = 20.0f;

			ImGui::DragFloat("Spot Angle",&spotDegree,0.1f,0.0f,90.0f);
			ImGui::DragFloat("Spot FalloffStart",&spotDegreeStart,0.1f,0.0f,90.0f);

			sData->cosAngle = cosf(spotDegree * 3.141592f / 180.0f);
			sData->cosFalloffStart = cosf(spotDegreeStart * 3.141592f / 180.0f);
		}

		if(simpleSkinObj_){
			ImGui::Separator();
			ImGui::Text("SimpleSkin Object (Environment)");

			auto* material = simpleSkinObj_->GetMaterial();
			if(material){
				ImGui::SliderFloat("Env Coefficient",&material->environmentCoefficient,0.0f,1.0f);
			} else{
				ImGui::TextColored(ImVec4(1,0,0,1),"Material is NULL!");
			}

			Vector3 skinPos = simpleSkinObj_->GetTranslate();
			if(ImGui::DragFloat3("Skin Pos",&skinPos.x,0.1f)){
				simpleSkinObj_->SetTranslate(skinPos);
			}
		}

		if(humanObj_){
			ImGui::Separator();
			ImGui::Text("Character Bone Control");

			// スケルトンの取得
			auto& skeleton = humanObj_->GetSkeleton();
			if(!skeleton.joints.empty()){
				auto& rootJoint = skeleton.joints[0]; // ルートボーン(インデックス0)を操作

				// ImGui用のstatic変数
				static float rootPos[3] = {0.0f, 0.0f, 0.0f};
				static float rootRot[3] = {0.0f, 0.0f, 0.0f};

				if(ImGui::DragFloat3("Root Bone Pos",rootPos,0.1f)){
					rootJoint.transform.translate = {rootPos[0], rootPos[1], rootPos[2]};
				}

				if(ImGui::DragFloat3("Root Bone Rot (Euler)",rootRot,0.01f)){
					// Eulerからクォータニオンに変換してセット
					rootJoint.transform.rotate = MakeQuaternionFromEuler(rootRot[0],rootRot[1],rootRot[2]);
				}

				// ★重要: アニメーションを停止して手動操作を有効にするためのボタン
				static bool isManualControl = false;
				ImGui::Checkbox("Manual Control",&isManualControl);

				if(isManualControl){
					// 手動操作時はアニメーションの更新をスキップさせる（Updateロジックで制御が必要）
					// ここで Skeleton::Update() を呼んで反映させる
					skeleton.Update();
				}
			}
		}


		ModelManager::GetInstance()->UpdateLightGui();
		ImGui::End();

		Application::GetInstance()->ShowPostProcessUI();
	}
#endif
}

void GameScene::Draw(){
	object3dCommon_->Draw();

	if(humanObj_){
		humanObj_->Draw();
	}
}
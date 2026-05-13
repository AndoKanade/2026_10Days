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

	auto* simpleSkinMaterial = simpleSkinObj_->GetMaterial();
	if(simpleSkinMaterial){
		simpleSkinMaterial->environmentCoefficient = 1.0f;
	}

	// スカイボックスの生成と初期化
	skyboxCommon_ = std::make_unique<SkyboxCommon>();
	skyboxCommon_->Initialize(object3dCommon_->GetDxCommon());
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(skyboxCommon_.get(),kSkyboxTexture);

	// パーティクルの設定
	//  リング状のパーティクル (第3引数 true)
	ParticleManager::GetInstance()->CreateParticleGroup(kParticleRing,kTexturegradationLine,true,false);
	Transform ringConfig;
	ringConfig.translate = {1.0f, 2.0f, 0.0f}; 
	ringConfig.scale = {0.5f, 0.5f, 0.5f};
	ringEmitter_ = std::make_unique<ParticleEmitter>(kParticleRing,ringConfig,1,0.5f);
	ringEmitter_->SetVelocity({0.0f, 0.5f, 0.0f});
	ringEmitter_->SetLifeTime(1.0f);

	//  板ポリの設定 (第3引数 false)
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
	cylinderConfig.scale = {1.0f, 1.0f, 1.0f}; // 縦に長くして柱にする

	cylinderEmitter_ = std::make_unique<ParticleEmitter>(kParticleCylinder,cylinderConfig,1,0.1f);
	cylinderEmitter_->SetColor({0.2f, 0.5f, 1.0f, 0.8f}); // ポータルらしい青色
	cylinderEmitter_->SetLifeTime(2.0f);
	cylinderEmitter_->SetVelocity({0.0f, 0.0f, 0.0f});   // その場に固定

	// カメラの設定
	CameraManager::GetInstance()->CreateCamera("default",object3dCommon_->GetDxCommon()->GetDevice());
	auto* defaultCamera = CameraManager::GetInstance()->GetCamera("default");
	if(defaultCamera){
		defaultCamera->SetTranslate({0.0f, 0.0f, -30.0f});
		CameraManager::GetInstance()->SetActiveCamera("default");
	}
	object3dCommon_->SetDefaultCamera(CameraManager::GetInstance()->GetActiveCamera());
}

void GameScene::Finalize(){}

void GameScene::Update(){
	// 各オブジェクトの更新
	if(sphereObj_){
		sphereObj_->Update();
	}
	if(terrainObj_){
		terrainObj_->Update();
	}
	if(simpleSkinObj_){
		simpleSkinObj_->Update();
	}
	if(skybox_){
		skybox_->Update(*CameraManager::GetInstance()->GetActiveCamera());
	}
	if(planeObj_){
		planeObj_->Update();
	}
	if(ringEmitter_){
		ringEmitter_->Update();
	}
	if(circleEmitter_){
		circleEmitter_->Update();
	}

	if(cylinderEmitter_){
		cylinderEmitter_->Update();
	}

	// パーティクルマネージャーの更新
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	if(activeCamera){
		ParticleManager::GetInstance()->Update(activeCamera);
	}

#pragma region
#ifdef USE_IMGUI
	if(activeCamera){
		ImGui::Begin("GameScene Debug");

		// カメラの調整
		Vector3 camPos = activeCamera->GetTranslate();
		if(ImGui::DragFloat3("Camera Pos",&camPos.x,0.1f)){
			activeCamera->SetTranslate(camPos);
		}

		Vector3 camRot = activeCamera->GetRotate();
		if(ImGui::DragFloat3("Camera Rotate",&camRot.x,0.01f)){
			activeCamera->SetRotate(camRot);
		}

		// オブジェクトの調整
		if(planeObj_){
			Vector3 pPos = planeObj_->GetTranslate();
			ImGui::DragFloat3("Parent(Plane) Pos",&pPos.x,0.1f);
			planeObj_->SetTranslate(pPos);
		}

		// ポイントライトの調整
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

		// スポットライトの調整
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

		// シンプルスキンオブジェクトの調整
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

		ModelManager::GetInstance()->UpdateLightGui();
		ImGui::End();
	}
#endif
#pragma endregion
}

void GameScene::Draw(){
	// 3Dオブジェクトの描画
	object3dCommon_->Draw();

	if(terrainObj_){
		terrainObj_->Draw();

	}

	// パーティクルの描画
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	if(activeCamera){
		Matrix4x4 viewProj = Multiply(activeCamera->GetViewMatrix(),activeCamera->GetProjectionMatrix());
		ParticleManager::GetInstance()->Draw(viewProj);
	}
}
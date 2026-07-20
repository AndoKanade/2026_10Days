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
#include "LevelManager.h"

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
	const std::string kModelHuman = "human/sneakWalk.gltf";
}

GameScene::GameScene() = default;
GameScene::~GameScene() = default;

// --- 初期化 ---
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
	humanObj_->LoadAnimation("resource/human/","sneakWalk.gltf");

	// パーティクルの設定
	ParticleManager::GetInstance()->CreateParticleGroup("Shockwave",kTexturegradationLine,true,false,true);
	ParticleManager::GetInstance()->CreateParticleGroup("Spark",kTextureCircle2,false,false,false,true);
	ParticleManager::GetInstance()->CreateParticleGroup("Smoke",kTextureCircle2,false,false,false,false,true);
	ParticleManager::GetInstance()->CreateParticleGroup("Charge",kTexturegradationLine,false,false,false,false,false,true);
	ParticleManager::GetInstance()->CreateParticleGroup("Aura",kTextureCircle2,false,false,false,false,false,false,true);
	ParticleManager::GetInstance()->CreateParticleGroup("Warp",kTexturegradationLine,false,true,false,false,false,false,false,true);

	LevelManager levelManager;
	levelManager.LoadJSON("level.json");

	for(const auto& objData : levelManager.GetObjects()){
		if(objData.type == "MESH"){
			// 新しいObj3Dインスタンスを生成・初期化
			auto newObj = std::make_shared<Obj3D>();
			newObj->Initialize(object3dCommon_);

			// ファイル名(モデル名)が指定されていればモデルをセット
			if(!objData.fileName.empty()){
				ModelManager::GetInstance()->LoadModel(objData.fileName);
				newObj->SetModel(objData.fileName);
			}

			// トランスフォームの適用
			newObj->SetTranslate(objData.translation);
			newObj->SetScale(objData.scaling);

			// JSONに保存されている回転角(度数法)をラジアンに変換
			float radX = objData.rotation.x * (3.14159265f / 180.0f);
			float radY = objData.rotation.y * (3.14159265f / 180.0f);
			float radZ = objData.rotation.z * (3.14159265f / 180.0f);

			// MyMathの関数を使ってオイラー角からクォータニオンに変換してセット
			newObj->SetQuaternion(MakeQuaternionFromEuler(radX,radY,radZ));

			// ※必要であればコライダーの初期化もここで行う
			/*
			if (objData.colliderType == "BOX") {
				newObj->SetCollider(objData.colliderCenter, objData.colliderSize);
			}
			*/

			// 管理用配列に追加
			levelObjects_.push_back(newObj);
		}
	}
}

void GameScene::Finalize(){}

// --- 更新処理 ---
void GameScene::Update(){
	// オブジェクトの更新
	if(sphereObj_) sphereObj_->Update();
	if(terrainObj_) terrainObj_->Update();
	if(simpleSkinObj_) simpleSkinObj_->Update();
	if(skybox_) skybox_->Update(*CameraManager::GetInstance()->GetActiveCamera());
	if(planeObj_) planeObj_->Update();

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

	for(auto& obj : levelObjects_){
		obj->Update();
	}

	// --- デバッグUIの表示 ---
#ifdef USE_IMGUI
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		// メインデバッグウィンドウ
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
					if(isManualControl) skeleton.Update();
				}
			}
		}

		ModelManager::GetInstance()->UpdateLightGui();
		ImGui::End();

		// パーティクル制御ウィンドウ
		ImGui::Begin("Particle Control");

		auto* particleManager = ParticleManager::GetInstance();

		if(ImGui::Button("Emit Shockwave")) particleManager->EmitShockwave({0.0f, 0.0f, 0.0f});
		ImGui::SameLine();
		if(ImGui::Button("Emit Spark")) particleManager->EmitSpark({0.0f, 0.0f, 0.0f});

		ImGui::SameLine();
		if(ImGui::Button("Emit Smoke")) particleManager->EmitSmoke({0.0f, 0.0f, 0.0f});

		if(ImGui::Button("Emit Charge")) particleManager->EmitCharge({0.0f, 0.0f, 0.0f});
		ImGui::SameLine();
		if(ImGui::Button("Emit Aura")) particleManager->EmitAura({0.0f, 0.0f, 0.0f});

		if(ImGui::Button("Emit Warp")) particleManager->EmitWarp();

		ImGui::End();

		Application::GetInstance()->ShowPostProcessUI();
	}
#endif
}

// --- 描画処理 ---
void GameScene::Draw(){
	object3dCommon_->Draw();
	if(humanObj_) humanObj_->Draw();

	for(const auto& obj : levelObjects_){
		obj->Draw();
	}

	if(Camera* cam = CameraManager::GetInstance()->GetActiveCamera()){
		Matrix4x4 viewProj = Multiply(cam->GetViewMatrix(),cam->GetProjectionMatrix());
		ParticleManager::GetInstance()->Draw(viewProj);
	}
}
#include "TitleScene.h"

// エンジン/システム関連
#include "Input.h"
#include "ModelManager.h"
#include "SceneManager.h"
#include "Obj3D.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "Obj3dCommon.h"  
#include "DXCommon.h"     
#include "CameraManager.h" 
#include "Model.h"

// ImGui (マクロ定義がある場合のみ)
#ifdef USE_IMGUI
#include "imguiManager.h"
#endif

// 定数定義 (ファイルパスやパラメータ)
namespace{
	const std::string kModelName = "Fence/fence.obj";
	const std::string kTextureName = "resource/uvChecker.png";
	const std::string kSkyboxTexture = "resource/Skybox/rostock_laage_airport_4k.dds";
	const float kSpriteSize = 300.0f;
}

// コンストラクタ
TitleScene::TitleScene() = default;

// デストラクタ
TitleScene::~TitleScene() = default;

// 初期化処理
void TitleScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	// メンバ変数の保持
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// --- リソースのロード ---
	ModelManager::GetInstance()->LoadModel(kModelName);
	TextureManager::GetInstance()->LoadTexture(kTextureName);
	TextureManager::GetInstance()->LoadTexture(kSkyboxTexture);

	auto* dxCommon = object3dCommon_->GetDxCommon();
	CameraManager::GetInstance()->CreateCamera("TitleCamera",dxCommon->GetDevice());

	// 2. 作ったカメラをアクティブにする
	CameraManager::GetInstance()->SetActiveCamera("TitleCamera");

	// 3. 必要なら座標を調整
	auto* camera = CameraManager::GetInstance()->GetActiveCamera();
	camera->SetTranslate({0.0f, 0.0f, -10.0f});

	// --- スプライト生成と設定 ---
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(spriteCommon_,kTextureName);
	sprite_->SetPosition({0.0f, 0.0f});         // 左上
	sprite_->SetSize({kSpriteSize, kSpriteSize});
	sprite_->SetColor({1.0f, 1.0f, 1.0f, 1.0f}); // 白（不透明）

	// --- 3Dオブジェクト生成と設定 ---
	titleObject_ = std::make_unique<Obj3D>();
	titleObject_->Initialize(object3dCommon_);
	titleObject_->SetModel(kModelName);
	titleObject_->SetTranslate({0.0f, 0.0f, 0.0f});
	titleObject_->SetScale({0.5f, 0.5f, 0.5f});

	auto* material = titleObject_->GetMaterial();
	if(material){
		material->environmentCoefficient = 0.0f;
	}
}

// 終了処理
void TitleScene::Finalize(){
	// unique_ptrにより自動解放されるため処理なし
}

// 更新処理
void TitleScene::Update(){

	// 1. ImGuiの設定更新
#ifdef USE_IMGUI
	ImGui::Begin("Sprite Settings");
	ImGui::ColorEdit4("Color & Alpha",&spriteColor_.x); // 色と透明度の調整
	ImGui::End();
#endif

	// 2. オブジェクトの更新
	if(titleObject_){
		titleObject_->Update();
	}

	// 3. スプライトの更新
	if(sprite_){
		// ImGuiが無効な場合、spriteColor_ は初期値が適用されます
		sprite_->SetColor(spriteColor_);
		sprite_->Update();
	}

	// 4. シーン遷移 (スペースキー)
	if(input_->TriggerKey(DIK_SPACE)){
		SceneManager::GetInstance()->ChangeScene("GAME");
	}
}

// 描画処理
void TitleScene::Draw(){
	// 3Dオブジェクト描画
	if(titleObject_){
		titleObject_->Draw();
	}

	// 2Dスプライト描画
	if(spriteCommon_ && sprite_){
		spriteCommon_->Draw(); // 描画前処理
		sprite_->Draw();       // スプライト本体
	}
}
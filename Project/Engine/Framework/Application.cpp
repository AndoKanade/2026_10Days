#include "Application.h"
#include "SceneFactory.h"
#include "SceneManager.h"
#include "ImGuiManager.h"
#include "SrvManager.h"
#include "TextureManager.h"

Application* Application::instance_ = nullptr;

// -------------------------------------------------
// コンストラクタ・デストラクタ
// -------------------------------------------------
Application::Application(){
	instance_ = this;
}

Application::~Application() = default;

// -------------------------------------------------
// 初期化処理
// -------------------------------------------------
void Application::Initialize(){
	// 1. 基底クラス(Framework)の初期化
	// ウィンドウ生成、DirectX初期化、Input、SpriteCommon等の生成が行われます
	Framework::Initialize();
	dxCommon_->InitDepthShaderResourceView();

	// 2. ポストプロセス関連のリソース初期化
	postProcess_ = std::make_unique<PostProcess>();
	postProcess_->Initialize(dxCommon_.get());

	renderTexture_ = std::make_unique<RenderTexture>();

	// RTVハンドルの取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += (size_t)dxCommon_->descriptorSizeRTV * 2;

	// SRVハンドルの取得
	uint32_t srvIndex = SrvManager::GetInstance()->Allocate();
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCpu = SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGpu = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);

	// オフスクリーン用テクスチャの生成
	renderTexture_->Create(
		dxCommon_->GetDevice(),
		WinAPI::kClientWidth,
		WinAPI::kCliantHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		{0.1f, 0.25f, 0.5f, 1.0f},
		rtvHandle,
		srvHandleCpu,
		srvHandleGpu
	);

	// 3. シーン工場の生成
	// このアプリケーション専用のシーン生成工場を作成します
	sceneFactory_ = std::make_unique<SceneFactory>();

	// 4. シーンマネージャのセットアップ
	SceneManager* sceneManager = SceneManager::GetInstance();

	// シーン生成工場をセット (AbstractSceneFactoryインターフェースとして渡す)
	sceneManager->SetFactory(sceneFactory_.get());

	// 各シーンで利用する共通システムへのポインタをセット
	// (Frameworkが保持している unique_ptr から生ポインタを取り出して渡す)
	sceneManager->SetCommonPtr(object3dCommon_.get(),input_.get(),spriteCommon_.get());

	// 5. 最初のシーンを開始
	// ここで指定したシーンからゲームが始まります
	sceneManager->ChangeScene("TITLE");

	// 6. ポストプロセス用マスク画像のロードと初期化
	TextureManager::GetInstance()->LoadTexture("resource/noise0.png");
	TextureManager::GetInstance()->LoadTexture("resource/noise1.png");
	currentMaskPath_ = "resource/noise0.png";
}

// -------------------------------------------------
// 終了処理
// -------------------------------------------------
void Application::Finalize(){
	// 1. 基底クラスの終了処理
	Framework::Finalize();

	// ※ sceneFactory_ 等のメンバ変数は unique_ptr なので、
	// デストラクタで自動的にメモリ解放されます。明示的な delete は不要です。
}

// -------------------------------------------------
// 更新処理
// -------------------------------------------------
void Application::Update(){
	// 1. 基底クラスの更新
	// ウィンドウメッセージ処理、入力更新、シーンマネージャの更新などはここで行われます
	Framework::Update();

	// 2. Dissolveアニメーション処理
	if(isDissolving_){
		// 1フレームの時間を加算
		dissolveTimer_ += 1.0f / 60.0f;

		// 0.0 ～ 1.0 に正規化
		float threshold = dissolveTimer_ / kDissolveDuration;

		if(threshold >= 1.0f){
			threshold = 1.0f;
			isDissolving_ = false; // 終了
		}

		// ポストプロセスに値を送る
		postProcess_->SetDissolveThreshold(threshold);
	}

	if(isGlitchActive_){
		glitchTimer_ -= 1.0f / 60.0f;
		if(glitchTimer_ <= 0.0f){
			isGlitchActive_ = false;
			currentPPType_ = PostProcess::Type::PostProcess; // 演出時間が終わったら標準に戻す
		}
	}

	// 3. Random用時間更新処理
	static float time = 0.0f;
	time += 1.0f / 60.0f;
	postProcess_->SetRandomTime(time);
}

// -------------------------------------------------
// 描画処理
// -------------------------------------------------
void Application::Draw(){
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// 1. RenderTextureへの描画 (パス1：オフスクリーン)
	dxCommon_->PreDraw(renderTexture_.get());
	SrvManager::GetInstance()->PreDraw();

	// 2. 3Dオブジェクト描画の共通設定
	if(object3dCommon_){
		object3dCommon_->Draw();
	}

	// 3. 現在のシーンの描画
	SceneManager::GetInstance()->Draw();

	// 4. Swapchainへの描画 (パス2：ポストプロセス適用)
	dxCommon_->PreDraw(nullptr);

	// リソースバリアの設定：カラーと深度を読み取り状態へ遷移
	D3D12_RESOURCE_BARRIER barriers[2] = {};

	// [0] カラーバッファの遷移
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Transition.pResource = renderTexture_->GetResource();
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	// [1] 深度バッファの遷移
	barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[1].Transition.pResource = dxCommon_->depthStencilResource.Get();
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	commandList->ResourceBarrier(2,barriers);

	// --- マスクハンドルの決定 (Dissolve対応) ---
	D3D12_GPU_DESCRIPTOR_HANDLE secondarySRV = dxCommon_->GetDepthSrvHandleGpu();

	// 現在のポストプロセスがDissolveの場合のみ、マスク用テクスチャに差し替える
	if(currentPPType_ == PostProcess::Type::Dissolve){
		secondarySRV = TextureManager::GetInstance()->GetSrvHandleGPU(currentMaskPath_);
	}

	// ポストプロセスの実行
	postProcess_->Draw(commandList,renderTexture_->GetSrvHandleGpu(),secondarySRV,currentPPType_);

	// 次フレームのために状態を元に戻す
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;

	commandList->ResourceBarrier(2,barriers);

	// 5. UI描画 (ImGui)
#ifdef _DEBUG
	ImGuiManager::GetInstance()->Draw();
#endif

	// 6. 描画後処理 (画面のフリップなど)
	dxCommon_->PostDraw();
}

// -------------------------------------------------
// デバッグUI表示
// -------------------------------------------------
void Application::ShowPostProcessUI(){
#ifdef _DEBUG
	ImGui::Begin("PostProcess Settings");

	const char* typeNames[] = {"Default", "BoxFilter", "Grayscale", "Vignette", "GaussianBlur", "LuminanceOutline", "DepthOutline", "RadialBlur", "Dissolve", "Random", "Glitch"};
	int currentIdx = static_cast<int>(currentPPType_);

	if(ImGui::Combo("Filter Type",&currentIdx,typeNames,IM_ARRAYSIZE(typeNames))){
		currentPPType_ = static_cast<PostProcess::Type>(currentIdx);
	}

	ImGui::Separator();

	if(currentPPType_ == PostProcess::Type::Vignette){
		ImGui::Text("Vignette Settings");
		static float intensity = 0.5f;
		if(ImGui::DragFloat("Intensity",&intensity,0.01f,0.0f,1.0f)){
			postProcess_->SetVignetteIntensity(intensity);
		}
		static float scale = 0.8f;
		if(ImGui::DragFloat("Scale",&scale,0.01f,0.0f,2.0f)){
			postProcess_->SetVignetteScale(scale);
		}
	} else if(currentPPType_ == PostProcess::Type::BoxFilter || currentPPType_ == PostProcess::Type::GaussianBlur){
		static int k = 1;
		const char* label = (currentPPType_ == PostProcess::Type::BoxFilter)?"Kernel Size":"Blur Strength";
		if(ImGui::SliderInt(label,&k,0,10)){
			postProcess_->SetKernelSize(k);
		}
	} else if(currentPPType_ == PostProcess::Type::LuminanceOutline || currentPPType_ == PostProcess::Type::DepthOutline){
		ImGui::Text(currentPPType_ == PostProcess::Type::DepthOutline?"Depth Edge Settings":"Luminance Edge Settings");
		ImGui::Text("Edge detection active.");
	} else if(currentPPType_ == PostProcess::Type::RadialBlur){
		ImGui::Text("Radial Blur Settings");

		static float center[2] = {0.5f, 0.5f};
		if(ImGui::DragFloat2("Center",center,0.01f,0.0f,1.0f)){
			postProcess_->SetRadialBlurCenter({center[0], center[1]});
		}

		static float width = 0.01f;
		if(ImGui::DragFloat("Width",&width,0.001f,0.0f,0.1f)){
			postProcess_->SetRadialBlurWidth(width);
		}
	} else if(currentPPType_ == PostProcess::Type::Dissolve){
		ImGui::Text("Dissolve Settings");
		static float threshold = 0.0f;

		if(isDissolving_){
			threshold = dissolveTimer_ / kDissolveDuration;
		}

		if(ImGui::SliderFloat("Threshold",&threshold,0.0f,1.0f)){
			postProcess_->SetDissolveThreshold(threshold);
		}

		if(ImGui::Button("Start Animation")){
			isDissolving_ = true;
			dissolveTimer_ = 0.0f;
		}
		ImGui::SameLine();
		if(ImGui::Button("Reset")){
			isDissolving_ = false;
			dissolveTimer_ = 0.0f;
			threshold = 0.0f;
			postProcess_->SetDissolveThreshold(0.0f);
		}

		ImGui::Separator();

		static float edgeWidth = 0.03f;
		if(ImGui::SliderFloat("Edge Width",&edgeWidth,0.0f,0.2f)){
			postProcess_->SetDissolveEdgeWidth(edgeWidth);
		}

		static float edgeColor[3] = {1.0f, 0.4f, 0.3f};
		if(ImGui::ColorEdit3("Edge Color",edgeColor)){
			postProcess_->SetDissolveEdgeColor({edgeColor[0], edgeColor[1], edgeColor[2]});
		}

		ImGui::Separator();
		ImGui::Text("Mask Texture");

		const char* masks[] = {"resource/noise0.png", "resource/noise1.png"};
		static int currentMaskIndex = 0;

		if(ImGui::Combo("Select Mask",&currentMaskIndex,masks,IM_ARRAYSIZE(masks))){
			currentMaskPath_ = masks[currentMaskIndex];
		}
	} else if(currentPPType_ == PostProcess::Type::Random){
		ImGui::Text("Random Noise Settings");
	} else if(currentPPType_ == PostProcess::Type::Glitch){
		ImGui::Text("Glitch Settings");
		static float intensity = 0.5f;
		if(ImGui::DragFloat("Intensity",&intensity,0.01f,0.0f,1.0f)){
			postProcess_->SetRandomIntensity(intensity);
		}
	}
	ImGui::End();
#endif
}

void Application::TriggerGlitch(){
	currentPPType_ = PostProcess::Type::Glitch; // フィルターをグリッチに変更
	isGlitchActive_ = true;                     // カウントダウン開始
	glitchTimer_ = kGlitchDuration;             // タイマーを 0.15秒 に設定
}
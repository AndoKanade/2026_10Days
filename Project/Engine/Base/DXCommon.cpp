#include "DXCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include "RenderTexture.h"
#include "SrvManager.h"

#include <d3d12.h>
#include <thread>
#include <cassert>
#include <format>

#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include "externals/imgui/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui/imgui_impl_win32.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Microsoft::WRL;
using namespace StringUtility;

//=============================================================================
// 初期化処理
//=============================================================================

// 追加 バックバッファの枚数
static constexpr uint32_t kNumBackBuffers = 2;

// 追加 オフスクリーン描画のクリア色（背景色）
static constexpr float kSceneClearColor[4] = {0.1f, 0.25f, 0.5f, 1.0f};

// 追加 スワップチェーンのクリア色（レターボックスの余白になるため黒）
static constexpr float kBackBufferClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

void DXCommon::Initialize(WinAPI* winApi){
	assert(winApi);
	this->winApi_ = winApi;

	// 追加 起動時のクライアントサイズから解像度を算出する
	RECT clientRect{};
	GetClientRect(winApi_->GetHwnd(),&clientRect);
	CalcResolution(clientRect.right - clientRect.left,clientRect.bottom - clientRect.top);

	// 各種初期化関数の呼び出し
	InitDevice();
	InitCommand();
	CreateSwapChain();
	CreateDepthBuffer();
	CreateDescriptorHeaps();
	InitRenderTargetView();
	InitDepthStancilView();
	InitFence();
	InitViewportRect();
	InitScissorRect();
	CreateDXCCompiler();
}

void DXCommon::InitDevice(){
	HRESULT hr;

	// デバッグレイヤーの有効化
	ComPtr<ID3D12Debug1> debugController = nullptr;
	if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))){
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}

	// DXGIファクトリーの生成
	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	// 高パフォーマンスなハードウェアアダプターの選定
	ComPtr<IDXGIAdapter4> useAdapter = nullptr;
	for(UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; i++){
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));

		if(!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)){
			Logger::Log(std::format("Use Adapter : {}\n",ConvertString(adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr;
	}
	assert(useAdapter != nullptr);

	// Direct3D12デバイスの生成
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = {"12.2", "12.1", "12.0"};

	for(size_t i = 0; i < _countof(featureLevels); i++){
		hr = D3D12CreateDevice(useAdapter.Get(),featureLevels[i],IID_PPV_ARGS(&device));
		if(SUCCEEDED(hr)){
			Logger::Log(std::format("FeatureLevels : {}\n",featureLevelStrings[i]));
			break;
		}
	}
	assert(SUCCEEDED(hr));
	Logger::Log("Complete create D3D12Device!!\n");

#ifdef _DEBUG
	// 情報キューの設定
	ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if(SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))){
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION,true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING,true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR,true);

		// 特定の警告の抑制
		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;

		infoQueue->PushStorageFilter(&filter);
	}
#endif
}

void DXCommon::InitCommand(){
	HRESULT hr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc,IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(hr));

	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(hr));

	hr = device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,commandAllocator.Get(),nullptr,IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(hr));
}

void DXCommon::CreateSwapChain(){
	HRESULT hr;
	// 変更 固定値ではなく実際のウィンドウサイズで生成する
	swapChainDesc.Width = backBufferWidth_;
	swapChainDesc.Height = backBufferHeight_;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = kNumBackBuffers;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	hr = dxgiFactory->CreateSwapChainForHwnd(
		commandQueue.Get(),winApi_->GetHwnd(),&swapChainDesc,nullptr,nullptr,
		reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// 追加 DXGI による Alt + Enter の自動フルスクリーン切り替えを無効化する
	// 切り替えは WinAPI のボーダーレスフルスクリーンで行うため
	dxgiFactory->MakeWindowAssociation(winApi_->GetHwnd(),DXGI_MWA_NO_ALT_ENTER);
}

void DXCommon::CreateDepthBuffer(){
	D3D12_RESOURCE_DESC resourceDesc{};
	// 変更 オフスクリーン描画のサイズに合わせる
	resourceDesc.Width = sceneWidth_;
	resourceDesc.Height = sceneHeight_;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,D3D12_HEAP_FLAG_NONE,&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,&depthClearValue,
		IID_PPV_ARGS(&depthStencilResource));
	assert(SUCCEEDED(hr));
}

void DXCommon::CreateDescriptorHeaps(){
	descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	rtvDescriptorHeap = CreateDiscriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV,50,false);
	dsvDescriptorHeap = CreateDiscriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false);
}

void DXCommon::InitRenderTargetView(){
	HRESULT hr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	// ヒープの先頭からバックバッファの枚数分のスロットを使う
	// リサイズ時も同じスロットへ上書きするので、ディスクリプタは増えない
	for(uint32_t i = 0; i < kNumBackBuffers; ++i){
		hr = swapChain->GetBuffer(i,IID_PPV_ARGS(&swapChainResources[i]));
		assert(SUCCEEDED(hr));

		rtvHandles[i] = rtvHandle;
		device->CreateRenderTargetView(swapChainResources[i].Get(),&rtvDesc,rtvHandle);
		rtvHandle.ptr += descriptorSizeRTV;
	}

	// 変更 リサイズ時に呼び直しても払い出し済みのインデックスを巻き戻さない
	if(currentRtvIndex_ < kNumBackBuffers){
		currentRtvIndex_ = kNumBackBuffers;
	}
}

void DXCommon::InitDepthStancilView(){
	dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	device->CreateDepthStencilView(depthStencilResource.Get(),&dsvDesc,dsvHandle);
}

void DXCommon::InitDepthShaderResourceView(){
	// 変更 ディスクリプタは初回だけ確保し、リサイズ時は同じ位置へビューを上書きする
	// 毎回確保するとリサイズのたびにディスクリプタを消費してしまう
	if(depthSrvIndex_ == kInvalidDescriptorIndex){
		depthSrvIndex_ = SrvManager::GetInstance()->Allocate();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCpu = SrvManager::GetInstance()->GetCPUDescriptorHandle(depthSrvIndex_);

	depthSrvHandleGpu_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(depthSrvIndex_);

	D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{};
	depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	depthSrvDesc.Texture2D.MipLevels = 1;

	device->CreateShaderResourceView(depthStencilResource.Get(),&depthSrvDesc,srvHandleCpu);
}

void DXCommon::InitFence(){
	HRESULT hr;
	hr = device->CreateFence(fenceValue,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	fenceEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
	assert(fenceEvent != nullptr);
}

void DXCommon::InitViewportRect(){
	// 変更 オフスクリーン描画用はレンダーテクスチャ全体を使う
	sceneViewport_.Width = (float)sceneWidth_;
	sceneViewport_.Height = (float)sceneHeight_;
	sceneViewport_.TopLeftX = 0;
	sceneViewport_.TopLeftY = 0;
	sceneViewport_.MinDepth = 0.0f;
	sceneViewport_.MaxDepth = 1.0f;

	// 変更 スワップチェーン用は縦横比を保つため中央寄せのレターボックス矩形にする
	viewport.Width = (float)sceneWidth_;
	viewport.Height = (float)sceneHeight_;
	viewport.TopLeftX = (float)((backBufferWidth_ - sceneWidth_) / 2);
	viewport.TopLeftY = (float)((backBufferHeight_ - sceneHeight_) / 2);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

void DXCommon::InitScissorRect(){
	// 変更 オフスクリーン描画用はレンダーテクスチャ全体を使う
	sceneScissor_.left = 0;
	sceneScissor_.right = (LONG)sceneWidth_;
	sceneScissor_.top = 0;
	sceneScissor_.bottom = (LONG)sceneHeight_;

	// 変更 スワップチェーン用はビューポートと同じレターボックス矩形にする
	scissorRect.left = (LONG)viewport.TopLeftX;
	scissorRect.right = scissorRect.left + (LONG)sceneWidth_;
	scissorRect.top = (LONG)viewport.TopLeftY;
	scissorRect.bottom = scissorRect.top + (LONG)sceneHeight_;
}

// 追加 ウィンドウサイズから各解像度とレターボックス矩形を算出する
void DXCommon::CalcResolution(uint32_t windowWidth,uint32_t windowHeight){
	// スワップチェーンはウィンドウ全体を覆う
	backBufferWidth_ = windowWidth;
	backBufferHeight_ = windowHeight;

	// 論理解像度の縦横比を保ったまま、ウィンドウに収まる最大の矩形を求める
	// 幅を基準にした高さがウィンドウに収まるなら幅いっぱい、収まらないなら高さいっぱいにする
	uint32_t heightFromWidth = windowWidth * WinAPI::kClientHeight / WinAPI::kClientWidth;

	if(heightFromWidth <= windowHeight){
		// ウィンドウが縦に余っている場合（上下に帯が出る）
		sceneWidth_ = windowWidth;
		sceneHeight_ = heightFromWidth;
	} else{
		// ウィンドウが横に余っている場合（左右に帯が出る）
		sceneWidth_ = windowHeight * WinAPI::kClientWidth / WinAPI::kClientHeight;
		sceneHeight_ = windowHeight;
	}

	// 計算誤差でゼロになるとリソースを作れないので最低1ピクセルを保証する
	if(sceneWidth_ == 0){
		sceneWidth_ = 1;
	}
	if(sceneHeight_ == 0){
		sceneHeight_ = 1;
	}
}

// 追加 GPUの処理完了を待つ
void DXCommon::WaitForGPU(){
	fenceValue++;
	commandQueue->Signal(fence.Get(),fenceValue);

	if(fence->GetCompletedValue() < fenceValue){
		fence->SetEventOnCompletion(fenceValue,fenceEvent);
		WaitForSingleObject(fenceEvent,INFINITE);
	}
}

// 追加 解像度に依存する描画リソースを作り直す
bool DXCommon::Resize(uint32_t windowWidth,uint32_t windowHeight){
	// サイズが変わっていなければ何もしない
	if(backBufferWidth_ == windowWidth && backBufferHeight_ == windowHeight){
		return false;
	}

	// 使用中のリソースを解放するためGPUの処理完了を待つ
	WaitForGPU();

	// ResizeBuffersはバックバッファへの参照が全て消えていないと失敗する
	for(uint32_t i = 0; i < kNumBackBuffers; ++i){
		swapChainResources[i].Reset();
	}

	// 新しい解像度を算出する
	CalcResolution(windowWidth,windowHeight);

	// スワップチェーンのバッファを作り直す
	HRESULT hr = swapChain->ResizeBuffers(
		kNumBackBuffers,backBufferWidth_,backBufferHeight_,swapChainDesc.Format,0);
	assert(SUCCEEDED(hr));

	// スワップチェーン設定にも新しいサイズを反映しておく
	swapChainDesc.Width = backBufferWidth_;
	swapChainDesc.Height = backBufferHeight_;

	// レンダーターゲットビューを同じディスクリプタスロットへ作り直す
	InitRenderTargetView();

	// 深度バッファと関連するビューを作り直す
	depthStencilResource.Reset();
	CreateDepthBuffer();
	InitDepthStancilView();
	InitDepthShaderResourceView();

	// ビューポートとシザー矩形を新しい解像度に合わせる
	InitViewportRect();
	InitScissorRect();

	return true;
}

void DXCommon::CreateDXCCompiler(){
	HRESULT hr;
	hr = DxcCreateInstance(CLSID_DxcUtils,IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));

	hr = DxcCreateInstance(CLSID_DxcCompiler,IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));
}

//=============================================================================
// 描画処理
//=============================================================================

void DXCommon::PreDraw(RenderTexture* renderTexture){
	D3D12_CPU_DESCRIPTOR_HANDLE targetRtv;
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	// オフスクリーン描画ターゲットの判定とリソースバリア
	if(renderTexture){
		targetRtv = renderTexture->GetRtvHandle();

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(swapChainResources[backBufferIndex].Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->ResourceBarrier(1,&barrier);
	} else{
		targetRtv = rtvHandles[backBufferIndex];
	}

	// レンダーターゲットの設定
	commandList->OMSetRenderTargets(1,&targetRtv,false,renderTexture?&dsvHandle:nullptr);

	// 変更 描画先ごとにクリア色とビューポートを切り替える
	if(renderTexture){
		// オフスクリーン描画はレンダーテクスチャ全体を使う
		commandList->ClearRenderTargetView(targetRtv,kSceneClearColor,0,nullptr);
		commandList->ClearDepthStencilView(dsvHandle,D3D12_CLEAR_FLAG_DEPTH,1.0f,0,0,nullptr);

		commandList->RSSetViewports(1,&sceneViewport_);
		commandList->RSSetScissorRects(1,&sceneScissor_);
	} else{
		// スワップチェーンへの描画は中央のレターボックス矩形だけを使う
		// クリアはバックバッファ全体にかかるため、余白は黒で塗られる
		commandList->ClearRenderTargetView(targetRtv,kBackBufferClearColor,0,nullptr);

		commandList->RSSetViewports(1,&viewport);
		commandList->RSSetScissorRects(1,&scissorRect);
	}
}

void DXCommon::PostDraw(){
	HRESULT hr;
	UINT bbIndex = swapChain->GetCurrentBackBufferIndex();

	// PRESENT状態へ戻すためのリソースバリア
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(swapChainResources[bbIndex].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);
	commandList->ResourceBarrier(1,&barrier);

	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	// コマンドリストの実行
	ID3D12CommandList* commandLists[] = {commandList.Get()};
	commandQueue->ExecuteCommandLists(1,commandLists);

	// 画面への表示
	swapChain->Present(1,0);

	// 変更 フェンスによるGPU同期は共通処理にまとめた
	WaitForGPU();

	// 次フレームに向けたリセット
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));

	hr = commandList->Reset(commandAllocator.Get(),nullptr);
	assert(SUCCEEDED(hr));
}

//=============================================================================
// ユーティリティ関数
//=============================================================================

ComPtr<ID3D12DescriptorHeap> DXCommon::CreateDiscriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType,UINT numDescriptors,bool shaderVisible){
	ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible?D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE:D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc,IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

ComPtr<IDxcBlob> DXCommon::CompileShader(const std::wstring& filePath,const wchar_t* profile){
	Logger::Log(std::format("Begin CompileShader: {}\n",ConvertString(filePath)));

	ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
	assert(SUCCEEDED(dxcUtils->LoadFile(filePath.c_str(),nullptr,&shaderSource)));

	DxcBuffer buffer;
	buffer.Ptr = shaderSource->GetBufferPointer();
	buffer.Size = shaderSource->GetBufferSize();
	buffer.Encoding = DXC_CP_UTF8;

	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E", L"main",
		L"-T", profile,
		L"-Zi", L"-Qembed_debug",
		L"-Od",
		L"-Zpr",
	};

	ComPtr<IDxcResult> shaderResult = nullptr;
	dxcCompiler->Compile(&buffer,arguments,_countof(arguments),includeHandler,IID_PPV_ARGS(&shaderResult));

	// コンパイルエラーのチェック
	ComPtr<IDxcBlobUtf8> shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS,IID_PPV_ARGS(&shaderError),nullptr);
	if(shaderError != nullptr && shaderError->GetStringLength() != 0){
		Logger::Log(shaderError->GetStringPointer());
		assert(false);
	}

	ComPtr<IDxcBlob> shaderBlob = nullptr;
	assert(SUCCEEDED(shaderResult->GetOutput(DXC_OUT_OBJECT,IID_PPV_ARGS(&shaderBlob),nullptr)));

	Logger::Log(std::format("Compile Succeeded: {}\n",ConvertString(filePath)));
	return shaderBlob;
}

ComPtr<ID3D12Resource> DXCommon::CreateBufferResource(size_t sizeInBytes){
	D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	// 256バイト境界にアライメント
	size_t alignedSize = (sizeInBytes + 0xff) & ~0xff;

	D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);

	ComPtr<ID3D12Resource> buffer;
	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,D3D12_HEAP_FLAG_NONE,&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&buffer));
	assert(SUCCEEDED(hr));

	return buffer;
}

ComPtr<ID3D12Resource> DXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata){
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Width = UINT(metadata.width);
	resourceDesc.Height = UINT(metadata.height);
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,D3D12_HEAP_FLAG_NONE,&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	return resource;
}

ComPtr<ID3D12Resource> DXCommon::UploadTextureData(const ComPtr<ID3D12Resource>& texture,const DirectX::ScratchImage& mipImages){
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device.Get(),mipImages.GetImages(),mipImages.GetImageCount(),mipImages.GetMetadata(),subresources);

	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(),0,UINT(subresources.size()));
	ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(intermediateSize);

	// 転送先へ遷移
	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(),D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1,&barrier);

	// データ転送
	UpdateSubresources(commandList.Get(),texture.Get(),intermediateResource.Get(),0,0,UINT(subresources.size()),subresources.data());

	// 読み込み可能状態へ遷移
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_GENERIC_READ);
	commandList->ResourceBarrier(1,&barrier);

	return intermediateResource;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXCommon::AllocateRtvDescriptor(){
	// 現在のインデックスの位置のハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += (size_t)descriptorSizeRTV * currentRtvIndex_;

	// 次回呼ばれた時のためにインデックスを1進める
	currentRtvIndex_++;

	return handle;
}
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

void DXCommon::Initialize(WinAPI* winApi){
	InitializeFixFPS();

	assert(winApi);
	this->winApi_ = winApi;

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

	// Direct3D12デバイスの生成 (対応レベルの高い順に試行)
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
	// エラー・警告発生時にブレークする情報キューの設定
	ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if(SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))){
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION,true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING,true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR,true);

		// 特定の警告メッセージの出力を抑制
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
	swapChainDesc.Width = winApi_->kClientWidth;
	swapChainDesc.Height = winApi_->kCliantHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	hr = dxgiFactory->CreateSwapChainForHwnd(
		commandQueue.Get(),winApi_->GetHwnd(),&swapChainDesc,nullptr,nullptr,
		reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));
}

void DXCommon::CreateDepthBuffer(){
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = winApi_->kClientWidth;
	resourceDesc.Height = winApi_->kCliantHeight;
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

	// ポストプロセス用のオフスクリーン描画先(RenderTexture)確保のため、RTV数を 2 から 3 へ拡張
	rtvDescriptorHeap = CreateDiscriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV,3,false);
	dsvDescriptorHeap = CreateDiscriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false);
}

void DXCommon::InitRenderTargetView(){
	HRESULT hr;
	const UINT kNumBackBuffers = 2;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for(uint32_t i = 0; i < kNumBackBuffers; ++i){
		hr = swapChain->GetBuffer(i,IID_PPV_ARGS(&swapChainResources[i]));
		assert(SUCCEEDED(hr));

		rtvHandles[i] = rtvHandle;
		device->CreateRenderTargetView(swapChainResources[i].Get(),&rtvDesc,rtvHandle);
		rtvHandle.ptr += descriptorSizeRTV;
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
	uint32_t index = SrvManager::GetInstance()->Allocate();
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCpu = SrvManager::GetInstance()->GetCPUDescriptorHandle(index);

	// ポストプロセスのシェーダー読み込み用GPUハンドルを取得・保存
	depthSrvHandleGpu_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(index);

	// 深度バッファをシェーダーリソースとして扱うためのSRVビュー設定
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
	viewport.Width = (float)WinAPI::kClientWidth;
	viewport.Height = (float)WinAPI::kCliantHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

void DXCommon::InitScissorRect(){
	scissorRect.left = 0;
	scissorRect.right = (LONG)WinAPI::kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = (LONG)WinAPI::kCliantHeight;
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

	// オフスクリーンターゲットが渡された場合のバリア・描画先変更
	if(renderTexture){
		targetRtv = renderTexture->GetRtvHandle();

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(swapChainResources[backBufferIndex].Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->ResourceBarrier(1,&barrier);
	} else{
		targetRtv = rtvHandles[backBufferIndex];
	}

	// レンダーターゲットの設定
	commandList->OMSetRenderTargets(1,&targetRtv,false,renderTexture?&dsvHandle:nullptr);

	float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
	commandList->ClearRenderTargetView(targetRtv,clearColor,0,nullptr);

	if(renderTexture){
		commandList->ClearDepthStencilView(dsvHandle,D3D12_CLEAR_FLAG_DEPTH,1.0f,0,0,nullptr);
	}

	commandList->RSSetViewports(1,&viewport);
	commandList->RSSetScissorRects(1,&scissorRect);
}

void DXCommon::PostDraw(){
	HRESULT hr;
	UINT bbIndex = swapChain->GetCurrentBackBufferIndex();

	// レンダーターゲットから画面表示状態(PRESENT)へのバリア遷移
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(swapChainResources[bbIndex].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);
	commandList->ResourceBarrier(1,&barrier);

	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	// コマンドリストの実行
	ID3D12CommandList* commandLists[] = {commandList.Get()};
	commandQueue->ExecuteCommandLists(1,commandLists);

	UpdateFixFPS();
	swapChain->Present(1,0);

	// フェンスによる同期処理
	fenceValue++;
	commandQueue->Signal(fence.Get(),fenceValue);

	if(fence->GetCompletedValue() < fenceValue){
		fence->SetEventOnCompletion(fenceValue,fenceEvent);
		WaitForSingleObject(fenceEvent,INFINITE);
	}

	// 次フレーム用コマンド割り当て群のリセット
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));

	hr = commandList->Reset(commandAllocator.Get(),nullptr);
	assert(SUCCEEDED(hr));
}

void DXCommon::InitializeFixFPS(){
	reference_ = std::chrono::steady_clock::now();
}

void DXCommon::UpdateFixFPS(){
	const std::chrono::microseconds kMinTime(16666); // 1/60sec
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

	if(elapsed < kMinTime){
		std::this_thread::sleep_for(kMinTime - elapsed);
	}
	reference_ = std::chrono::steady_clock::now();
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

	// バッファを一度変数として定義してからアドレスを渡す
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

	// コンパイルエラーの確認
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

	// 定数バッファのアライメント制約(256バイトの倍数)に切り上げ
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

	// テクスチャへのデータ転送前に、転送先リソースを COPY_DEST 状態へ遷移させるバリア
	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(),D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1,&barrier);

	UpdateSubresources(commandList.Get(),texture.Get(),intermediateResource.Get(),0,0,UINT(subresources.size()),subresources.data());

	// サンプリング可能にするため、状態を GENERIC_READ へ戻すバリア
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_GENERIC_READ);
	commandList->ResourceBarrier(1,&barrier);

	return intermediateResource;
}
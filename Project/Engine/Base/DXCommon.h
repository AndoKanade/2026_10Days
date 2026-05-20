#pragma once
#include "WinAPI.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <DirectXTex.h>
#include <wrl.h>
#include <array>
#include <chrono>
#include <string>
#include <cstdint>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

// 前方宣言
class RenderTexture;

class DXCommon{
public:
	// 名前空間の省略 (メンバ変数定義用)
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

#pragma region メンバ変数
public:
	// --- Direct3D Core ---
	ComPtr<IDXGIFactory7> dxgiFactory;
	ComPtr<ID3D12Device> device;

	// --- Command Objects ---
	ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;
	ComPtr<ID3D12CommandQueue> commandQueue = nullptr;

	// --- SwapChain & RenderTargets ---
	ComPtr<IDXGISwapChain4> swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	std::array<ComPtr<ID3D12Resource>,2> swapChainResources;
	std::array<ComPtr<ID3D12Resource>,2> backBuffers;

	// --- RTV Descriptor Heap ---
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = nullptr;
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	uint32_t descriptorSizeRTV = 0;

	// --- Depth Stencil ---
	ComPtr<ID3D12Resource> depthStencilResource = nullptr;
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
	uint32_t descriptorSizeDSV = 0;

	// --- ポストプロセス用深度SRV ---
	D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandleGpu_;

	// --- Synchronization (Fence) ---
	ComPtr<ID3D12Fence> fence = nullptr;
	uint64_t fenceValue = 0;
	HANDLE fenceEvent = nullptr;
	D3D12_RESOURCE_BARRIER barrier{};

	// --- Viewport & Scissor ---
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};

	// --- DXC (Shader Compiler) ---
	ComPtr<IDxcUtils> dxcUtils = nullptr;
	ComPtr<IDxcCompiler3> dxcCompiler = nullptr;
	IDxcIncludeHandler* includeHandler = nullptr;
#pragma endregion

#pragma region メンバ関数
public:
	// --- 初期化フロー ---
	void Initialize(WinAPI* winApi);

	void InitDevice();
	void InitCommand();
	void CreateSwapChain();
	void CreateDepthBuffer();
	void CreateDescriptorHeaps();
	void InitRenderTargetView();
	void InitDepthStancilView();
	void InitDepthShaderResourceView();
	void InitFence();
	void InitViewportRect();
	void InitScissorRect();
	void CreateDXCCompiler();

	// --- 描画フロー ---
	void PreDraw(RenderTexture* renderTexture = nullptr);
	void PostDraw();
#pragma endregion

#pragma region ユーティリティ関数
	ComPtr<ID3D12DescriptorHeap> CreateDiscriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE heapType,
		UINT numDescriptors,
		bool shaderVisible
	);

	ComPtr<IDxcBlob> CompileShader(
		const std::wstring& filePath,
		const wchar_t* profile
	);

	ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

	ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

	ComPtr<ID3D12Resource> UploadTextureData(
		const ComPtr<ID3D12Resource>& texture,
		const DirectX::ScratchImage& mipImages
	);
#pragma endregion

	// --- アクセッサ ---
	ID3D12Device* GetDevice() const{ return device.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const{ return commandList.Get(); }
	ID3D12CommandQueue* GetCommandQueue() const{ return commandQueue.Get(); }
	size_t GetSwapChainResourcesNum() const{ return swapChainResources.size(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const{ return dsvHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetDepthSrvHandleGpu() const{ return depthSrvHandleGpu_; }

	void SetDepthSrvHandleGpu(D3D12_GPU_DESCRIPTOR_HANDLE handle){ depthSrvHandleGpu_ = handle; }

private:
	WinAPI* winApi_ = nullptr;

	// --- FPS制御 ---
	std::chrono::steady_clock::time_point reference_;
	void InitializeFixFPS();
	void UpdateFixFPS();

	// --- ヘルパー (static) ---
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
		const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
		uint32_t descriptorSize,
		uint32_t index){
		D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		handleCPU.ptr += (size_t)descriptorSize * index;
		return handleCPU;
	}

	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDscriptorHandle(
		const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
		uint32_t descriptorSize,
		uint32_t index){
		D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
		handleGPU.ptr += (size_t)descriptorSize * index;
		return handleGPU;
	}
};
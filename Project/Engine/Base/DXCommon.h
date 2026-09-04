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

// DirectX 12関連のライブラリリンク
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

// 前方宣言
class RenderTexture;

class DXCommon{
public:
	// Microsoft::WRL::ComPtrの省略定義
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	// 追加 ディスクリプタ未確保をあらわす番号
	static constexpr uint32_t kInvalidDescriptorIndex = UINT32_MAX;

#pragma region メンバ変数
private:
	// --- Direct3D Core ---
	ComPtr<IDXGIFactory7> dxgiFactory; // DXGIファクトリ
	ComPtr<ID3D12Device> device;       // D3D12デバイス

	// --- Command Objects ---
	ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;    // コマンドアロケータ
	ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;      // グラフィックスコマンドリスト
	ComPtr<ID3D12CommandQueue> commandQueue = nullptr;            // コマンドキュー

	// --- SwapChain & RenderTargets ---
	ComPtr<IDXGISwapChain4> swapChain = nullptr;                  // スワップチェーン
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};                        // スワップチェーン設定
	std::array<ComPtr<ID3D12Resource>,2> swapChainResources;     // スワップチェーンリソース
	std::array<ComPtr<ID3D12Resource>,2> backBuffers;            // バックバッファ

	// --- RTV Descriptor Heap ---
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = nullptr;     // RTV用ディスクリプタヒープ
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};                      // RTV設定
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];                    // RTVハンドル
	uint32_t descriptorSizeRTV = 0;                               // RTVディスクリプタサイズ

	// --- Depth Stencil ---
	ComPtr<ID3D12Resource> depthStencilResource = nullptr;        // 深度バッファリソース
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = nullptr;     // DSV用ディスクリプタヒープ
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;                        // DSVハンドル
	uint32_t descriptorSizeDSV = 0;                               // DSVディスクリプタサイズ

	// --- ポストプロセス用深度SRV ---
	D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandleGpu_;               // 深度SRV用GPUハンドル
	uint32_t depthSrvIndex_ = kInvalidDescriptorIndex;            // 追加 深度SRVのディスクリプタ番号（リサイズ時に同じ位置へ作り直すため）

	// --- Synchronization (Fence) ---
	ComPtr<ID3D12Fence> fence = nullptr;                          // フェンス
	uint64_t fenceValue = 0;                                      // フェンス値
	HANDLE fenceEvent = nullptr;                                  // フェンス用イベントハンドル
	D3D12_RESOURCE_BARRIER barrier{};                             // リソースバリア

	// --- Viewport & Scissor ---
	D3D12_VIEWPORT viewport{};                                    // ビューポート矩形（スワップチェーン用のレターボックス矩形）
	D3D12_RECT scissorRect{};                                     // シザー矩形（スワップチェーン用）

	// --- 追加 解像度情報 ---
	uint32_t backBufferWidth_ = 0;                                // スワップチェーンの実サイズ（ウィンドウサイズと同じ）
	uint32_t backBufferHeight_ = 0;
	uint32_t sceneWidth_ = 0;                                     // シーン描画の実サイズ（ウィンドウに収まる最大の16対9矩形）
	uint32_t sceneHeight_ = 0;

	// --- 追加 シーン描画用のViewport & Scissor ---
	D3D12_VIEWPORT sceneViewport_{};                              // オフスクリーン描画用のビューポート矩形
	D3D12_RECT sceneScissor_{};                                   // オフスクリーン描画用のシザー矩形

	// --- DXC (Shader Compiler) ---
	ComPtr<IDxcUtils> dxcUtils = nullptr;                         // DXCユーティリティ
	ComPtr<IDxcCompiler3> dxcCompiler = nullptr;                  // DXCコンパイラ
	IDxcIncludeHandler* includeHandler = nullptr;                 // インクルードハンドラ
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

	D3D12_CPU_DESCRIPTOR_HANDLE AllocateRtvDescriptor();

	// --- 追加 解像度の変更フロー ---
	// ウィンドウサイズから各解像度とレターボックス矩形を算出する
	void CalcResolution(uint32_t windowWidth,uint32_t windowHeight);

	// GPUの処理完了を待つ
	void WaitForGPU();

	// 解像度に依存する描画リソースを作り直す
	// 実際に作り直したときだけ true を返す
	bool Resize(uint32_t windowWidth,uint32_t windowHeight);

	// --- 描画フロー ---
	void PreDraw(RenderTexture* renderTexture = nullptr); // 描画前処理
	void PostDraw();                                      // 描画後処理
#pragma endregion

#pragma region ユーティリティ関数
public:
	// ディスクリプタヒープの作成
	ComPtr<ID3D12DescriptorHeap> CreateDiscriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE heapType,
		UINT numDescriptors,
		bool shaderVisible
	);

	// シェーダーのコンパイル
	ComPtr<IDxcBlob> CompileShader(
		const std::wstring& filePath,
		const wchar_t* profile
	);

	// バッファリソースの作成
	ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

	// テクスチャリソースの作成
	ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

	// テクスチャデータのアップロード
	ComPtr<ID3D12Resource> UploadTextureData(
		const ComPtr<ID3D12Resource>& texture,
		const DirectX::ScratchImage& mipImages
	);
#pragma endregion

public:
	// --- アクセッサ ---
	ID3D12Device* GetDevice() const{ return device.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const{ return commandList.Get(); }
	ID3D12CommandQueue* GetCommandQueue() const{ return commandQueue.Get(); }
	size_t GetSwapChainResourcesNum() const{ return swapChainResources.size(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const{ return dsvHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetDepthSrvHandleGpu() const{ return depthSrvHandleGpu_; }
	ID3D12DescriptorHeap* GetRtvDescriptorHeap() const{ return rtvDescriptorHeap.Get(); }
	uint32_t GetDescriptorSizeRTV() const{ return descriptorSizeRTV; }
	ID3D12Resource* GetDepthStencilResource() const{ return depthStencilResource.Get(); }

	// 追加 シーン描画の実サイズ（RenderTextureの生成に使用する）
	uint32_t GetSceneWidth() const{ return sceneWidth_; }
	uint32_t GetSceneHeight() const{ return sceneHeight_; }

	void SetDepthSrvHandleGpu(D3D12_GPU_DESCRIPTOR_HANDLE handle){ depthSrvHandleGpu_ = handle; }

private:
	WinAPI* winApi_ = nullptr; // Windows API管理クラスへのポインタ
	uint32_t currentRtvIndex_ = 0;

	// --- ヘルパー (static) ---
	// CPUディスクリプタハンドルの取得
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
		const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
		uint32_t descriptorSize,
		uint32_t index){
		D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		handleCPU.ptr += (size_t)descriptorSize * index;
		return handleCPU;
	}

	// GPUディスクリプタハンドルの取得
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDscriptorHandle(
		const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
		uint32_t descriptorSize,
		uint32_t index){
		D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
		handleGPU.ptr += (size_t)descriptorSize * index;
		return handleGPU;
	}
};
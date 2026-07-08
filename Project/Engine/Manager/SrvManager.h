#pragma once
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>

// 前方宣言
class DXCommon;

class SrvManager{
public: // --- 定数 ---

	// SRVデスクリプタの最大数
	static const uint32_t kMaxSRVCount;

public: // --- シングルトン管理 ---

	// インスタンス取得
	static SrvManager* GetInstance();

	// 終了処理
	void Finalize();

public: // --- システム関数 ---

	// 初期化
	void Initialize(DXCommon* dxCommon);

	// 描画前処理 (SRVヒープをコマンドリストに積む)
	void PreDraw();

public: // --- デスクリプタ管理 ---

	// SRV用のインデックスを1つ確保する
	uint32_t Allocate();

	// 確保可能かチェック
	bool CanAllocate() const;

	// 指定したインデックスのSRVをルートパラメータにセット
	void SetGraphicsRootDescriptorTable(UINT RootParameterIndex,uint32_t srvIndex);

	// テクスチャ用SRV生成 (Texture)
	void CreateSRVTexture(uint32_t srvIndex,ID3D12Resource* pResource,DXGI_FORMAT Format,UINT MipLevels);

	// 構造化バッファ用SRV生成 (StructuredBuffer)
	void CreateSRVforStructuredBuffer(uint32_t srvIndex,ID3D12Resource* pResource,UINT numElements,UINT structureByteStride);

	// 構造化バッファ用UAV生成
	void CreateUAVForStructuredBuffer(uint32_t srvIndex,ID3D12Resource* pResource,UINT numElements,UINT structureByteStride);

public: // --- ハンドル取得 ---

	ID3D12DescriptorHeap* GetDescriptorHeap() const{ return descriptorHeap.Get(); }

	// CPUハンドル取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

	// GPUハンドル取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

private: // --- コンストラクタ等 (シングルトン) ---

	SrvManager() = default;
	~SrvManager() = default;
	SrvManager(const SrvManager&) = delete;
	SrvManager& operator=(const SrvManager&) = delete;

private: // --- メンバ変数 ---

	DXCommon* dxCommon_ = nullptr;

	// SRV用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	// デスクリプタ1個分のサイズ
	uint32_t descriptorSize = 0;

	// 次に使用するインデックス
	uint32_t useIndex = 0;
};
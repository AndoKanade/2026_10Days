#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "MyMath.h"

class RenderTexture{
public:
	// 生成・初期化
	void Create(
		Microsoft::WRL::ComPtr<ID3D12Device> device,
		uint32_t width,
		uint32_t height,
		DXGI_FORMAT format,
		const Vector4& clearColor,
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCpu,
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGpu
	);

	// --- リソース取得用 ---
	ID3D12Resource* GetResource() const{ return resource_.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle() const{ return rtvHandle_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGpu() const{ return srvHandleGpu_; }

private:
	// --- メンバ変数 ---
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_;
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCpu_;
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGpu_;
};
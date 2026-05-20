#include "RenderTexture.h"
#include <cassert>

void RenderTexture::Create(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	uint32_t width,
	uint32_t height,
	DXGI_FORMAT format,
	const Vector4& clearColor,
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCpu,
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGpu
){
	// リソース設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	// ヒープ設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// クリア値設定
	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = format;
	clearValue.Color[0] = clearColor.x;
	clearValue.Color[1] = clearColor.y;
	clearValue.Color[2] = clearColor.z;
	clearValue.Color[3] = clearColor.w;

	// リソース生成
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&resource_)
	);
	assert(SUCCEEDED(hr));

	// RTV作成
	rtvHandle_ = rtvHandle;
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	device->CreateRenderTargetView(resource_.Get(),&rtvDesc,rtvHandle_);

	// SRV作成
	srvHandleCpu_ = srvHandleCpu;
	srvHandleGpu_ = srvHandleGpu;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	device->CreateShaderResourceView(resource_.Get(),&srvDesc,srvHandleCpu_);
}
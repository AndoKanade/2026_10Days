#include "SrvManager.h"
#include "DXCommon.h"
#include <cassert>

// SRVの最大数
const uint32_t SrvManager::kMaxSRVCount = 512;

// インスタンス取得
SrvManager* SrvManager::GetInstance(){
	static SrvManager instance;
	return &instance;
}

// 初期化
void SrvManager::Initialize(DXCommon* dxCommon){
	assert(dxCommon);
	this->dxCommon_ = dxCommon;

	// SRV用ヒープの生成
	descriptorHeap = dxCommon_->CreateDiscriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,kMaxSRVCount,true);

	// デスクリプタ1個分のサイズを取得
	descriptorSize = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	useIndex = 0;
}

// 終了処理
void SrvManager::Finalize(){
	descriptorHeap.Reset();
	useIndex = 0;
	dxCommon_ = nullptr;
}

// 描画前処理
void SrvManager::PreDraw(){
	ID3D12DescriptorHeap* descriptorHeaps[] = {descriptorHeap.Get()};
	dxCommon_->GetCommandList()->SetDescriptorHeaps(1,descriptorHeaps);
}

// 空きインデックスの確保
uint32_t SrvManager::Allocate(){
	assert(CanAllocate());
	int index = useIndex;
	useIndex++;
	return index;
}

// 確保可能かチェック
bool SrvManager::CanAllocate() const{
	return useIndex < kMaxSRVCount;
}

// CPUハンドルの取得
D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index){
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

// GPUハンドルの取得
D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index){
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

// コマンドリストにSRVをセット
void SrvManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex,uint32_t srvIndex){
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex,GetGPUDescriptorHandle(srvIndex));
}

// テクスチャ用SRVの生成 (2D/CubeMap自動判別)
void SrvManager::CreateSRVTexture(uint32_t srvIndex,ID3D12Resource* pResource,DXGI_FORMAT Format,UINT MipLevels){
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();

	// CubeMap判定 (Texture2Dかつ配列数が6)
	if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && resDesc.DepthOrArraySize == 6){
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = MipLevels;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	} else{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = MipLevels;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	}

	dxCommon_->GetDevice()->CreateShaderResourceView(pResource,&srvDesc,GetCPUDescriptorHandle(srvIndex));
}

// StructuredBuffer用SRVの生成
void SrvManager::CreateSRVforStructuredBuffer(uint32_t srvIndex,ID3D12Resource* pResource,UINT numElements,UINT structureByteStride){
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = numElements;
	srvDesc.Buffer.StructureByteStride = structureByteStride;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	dxCommon_->GetDevice()->CreateShaderResourceView(pResource,&srvDesc,GetCPUDescriptorHandle(srvIndex));
}

void SrvManager::CreateUAVForStructuredBuffer(uint32_t srvIndex,ID3D12Resource* pResource,UINT numElements,UINT structureByteStride){
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.NumElements = numElements;
	uavDesc.Buffer.StructureByteStride = structureByteStride;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	dxCommon_->GetDevice()->CreateUnorderedAccessView(
		pResource,nullptr,&uavDesc,GetCPUDescriptorHandle(srvIndex));
}
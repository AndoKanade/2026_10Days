#include "ParticleManager.h"
#include "TextureManager.h"
#include "Camera.h"
#include "Logger.h"
#include <cassert>
#include <random>
#include <numbers>

using namespace Microsoft::WRL;

// --- シングルトン管理 ---
ParticleManager* ParticleManager::GetInstance(){
	static ParticleManager instance;
	return &instance;
}

// -------------------------------------------------
// ライフサイクル
// -------------------------------------------------

void ParticleManager::Initialize(DXCommon* dxCommon,SrvManager* srvManager){
	assert(dxCommon);
	assert(srvManager);

	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	// 乱数生成器の初期化
	std::random_device seedGenerator;
	randomEngine_.seed(seedGenerator());

	// 定数バッファ用リソース生成
	size_t sizeInBytes = (sizeof(PerView) + 0xff) & ~0xff;
	perViewResource_ = dxCommon_->CreateBufferResource(sizeInBytes);
	perViewResource_->Map(0,nullptr,reinterpret_cast<void**>(&perViewData_));
	*perViewData_ = {MakeIdentity4x4(), MakeIdentity4x4()};

	size_t sizePerFrame = (sizeof(PerFrame) + 0xff) & ~0xff;
	perFrameResource_ = dxCommon_->CreateBufferResource(sizePerFrame);
	perFrameResource_->Map(0,nullptr,reinterpret_cast<void**>(&perFrameData_));
	perFrameData_->time = 0.0f;
	perFrameData_->deltaTime = 1.0f / 60.0f;

	// 各種パイプラインおよびモデル生成
	CreateGraphicsPipeline();
	CreateModel();
	CreateRingModel();
	CreateCylinderModel();
	CreateComputePipeline();
}

void ParticleManager::Finalize(){
	// グループのリソース解放
	for(auto& [name,group] : particleGroups_){
		group->instancingResource.Reset();
	}
	particleGroups_.clear();

	// 共有リソースの解放
	rootSignature_.Reset();
	graphicsPipelineState_.Reset();
	computeRootSignature_.Reset();
	computePipelineState_.Reset();
	emitComputeRootSignature_.Reset();
	emitComputePipelineState_.Reset();

	vertexBuffer_.Reset();
	ringVertexBuffer_.Reset();
	cylinderVertexBuffer_.Reset();
	vertexResource_.Reset();

	// 定数バッファ解放
	perViewResource_.Reset();
	perFrameResource_.Reset();
}

// -------------------------------------------------
// パーティクル操作
// -------------------------------------------------

void ParticleManager::CreateParticleGroup(const std::string& name,const std::string& textureFilePath,bool isRing,bool isCylinder,bool isShockwave,bool isSpark,bool isSmoke,bool isCharge,bool isAura,bool isWarp){
	if(particleGroups_.contains(name)){
		return;
	}

	std::unique_ptr<ParticleGroup> group = std::make_unique<ParticleGroup>();

	// 各種設定
	group->isRing = isRing;
	group->isCylinder = isCylinder;
	group->isShockwave = isShockwave;
	group->isSpark = isSpark;
	group->isSmoke = isSmoke;
	group->isCharge = isCharge;
	group->isAura = isAura;
	group->isWarp = isWarp;

	// テクスチャ設定
	group->textureFilePath = textureFilePath;
	TextureManager::GetInstance()->LoadTexture(textureFilePath);
	group->textureSrvIndex = TextureManager::GetInstance()->GetSrvIndex(textureFilePath);

	// インスタンシング用リソース生成 (DEFAULTヒープ)
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(ParticleForGPU) * group->kNumMaxInstance;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&group->instancingResource)
	);
	assert(SUCCEEDED(hr));

	// SRV生成
	group->instancingSrvIndex = srvManager_->Allocate();
	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.NumElements = group->kNumMaxInstance;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	srvManager_->CreateSRVforStructuredBuffer(
		group->instancingSrvIndex,
		group->instancingResource.Get(),
		instancingSrvDesc.Buffer.NumElements,
		instancingSrvDesc.Buffer.StructureByteStride
	);

	// UAV生成
	group->instancingUavIndex = srvManager_->Allocate();
	group->freeCounterUavIndex = srvManager_->Allocate();

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = group->kNumMaxInstance;
	uavDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

	D3D12_UNORDERED_ACCESS_VIEW_DESC counterUavDesc{};
	counterUavDesc.Format = DXGI_FORMAT_UNKNOWN;
	counterUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	counterUavDesc.Buffer.NumElements = 1;
	counterUavDesc.Buffer.StructureByteStride = sizeof(int32_t);
	dxCommon_->GetDevice()->CreateUnorderedAccessView(group->freeCounterResource.Get(),nullptr,&counterUavDesc,srvManager_->GetCPUDescriptorHandle(group->freeCounterUavIndex));

	dxCommon_->GetDevice()->CreateUnorderedAccessView(
		group->instancingResource.Get(),
		nullptr,
		&uavDesc,
		srvManager_->GetCPUDescriptorHandle(group->instancingUavIndex)
	);

	D3D12_RESOURCE_DESC counterResourceDesc{};
	counterResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	counterResourceDesc.Width = sizeof(int32_t);
	counterResourceDesc.Height = 1;
	counterResourceDesc.DepthOrArraySize = 1;
	counterResourceDesc.MipLevels = 1;
	counterResourceDesc.SampleDesc.Count = 1;
	counterResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	counterResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&counterResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&group->freeCounterResource)
	);

	assert(SUCCEEDED(hr));
	size_t emitterSize = (sizeof(EmitterSphere) + 0xff) & ~0xff;
	group->emitterResource = dxCommon_->CreateBufferResource(emitterSize);
	group->emitterResource->Map(0,nullptr,reinterpret_cast<void**>(&group->emitterData));

	// エミッターの初期値設定
	group->emitterData->count = 10;
	group->emitterData->frequency = 0.5f;
	group->emitterData->frequencyTime = 0.0f;
	group->emitterData->translate = {0.0f, 0.0f, 0.0f};
	group->emitterData->radius = 1.0f;
	group->emitterData->emit = 0;

	group->currentState = D3D12_RESOURCE_STATE_COMMON;

	particleGroups_.insert(std::make_pair(name,std::move(group)));
}

void ParticleManager::Update(Camera* camera){
	assert(camera);
	const float kDeltaTime = 1.0f / 60.0f;

	// カメラ行列等の計算
	Matrix4x4 viewMatrix = camera->GetViewMatrix();
	Matrix4x4 projectionMatrix = camera->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix,projectionMatrix);
	Matrix4x4 billboardMatrix = camera->GetWorldMatrix();
	billboardMatrix.m[3][0] = 0.0f; billboardMatrix.m[3][1] = 0.0f; billboardMatrix.m[3][2] = 0.0f;

	// 全グループ更新
	for(auto& [name,group] : particleGroups_){
		if(group->emitterData){
			group->emitterData->emit = 0;
		}

		// エミッターの更新処理
		if(group->emitterData){
			group->emitterData->frequencyTime += kDeltaTime;
			if(group->emitterData->frequency <= group->emitterData->frequencyTime){
				group->emitterData->frequencyTime -= group->emitterData->frequency;
			}
		}

		// Compute Shaderで全パーティクルを計算しているため、描画個数は最大値にします。
		group->numInstance = group->kNumMaxInstance;
	}

	// 定数バッファの更新
	if(perViewData_){
		perViewData_->viewProjection = viewProjectionMatrix;
		perViewData_->billboardMatrix = billboardMatrix;
	}
	if(perFrameData_){
		perFrameData_->deltaTime = kDeltaTime;
		perFrameData_->time += kDeltaTime;
	}
}

void ParticleManager::Draw(const Matrix4x4& viewProjectionMatrix){
	assert(dxCommon_);
	auto commandList = dxCommon_->GetCommandList();

	for(auto& [name,group] : particleGroups_){
		if(group->numInstance == 0) continue;

		// UAV状態へ遷移
		D3D12_RESOURCE_BARRIER barrierToUAV{};
		barrierToUAV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrierToUAV.Transition.pResource = group->instancingResource.Get();
		barrierToUAV.Transition.StateBefore = group->currentState;
		barrierToUAV.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barrierToUAV.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1,&barrierToUAV);
		group->currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		// エミット用コンピュートシェーダー実行
		if(group->emitterResource && group->emitterData->emit == 1){
			commandList->SetComputeRootSignature(emitComputeRootSignature_.Get());
			commandList->SetPipelineState(emitComputePipelineState_.Get());

			commandList->SetComputeRootDescriptorTable(0,srvManager_->GetGPUDescriptorHandle(group->instancingUavIndex));
			commandList->SetComputeRootDescriptorTable(1,srvManager_->GetGPUDescriptorHandle(group->freeCounterUavIndex));

			commandList->SetComputeRootConstantBufferView(2,group->emitterResource->GetGPUVirtualAddress());
			commandList->SetComputeRootConstantBufferView(3,perFrameResource_->GetGPUVirtualAddress());
			commandList->Dispatch(1,1,1);
			D3D12_RESOURCE_BARRIER barrierUAV{};
			barrierUAV.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barrierUAV.UAV.pResource = group->instancingResource.Get();
			commandList->ResourceBarrier(1,&barrierUAV);

		}

		// 更新用コンピュートシェーダー実行
		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());
		commandList->SetComputeRootDescriptorTable(0,srvManager_->GetGPUDescriptorHandle(group->instancingUavIndex));
		commandList->SetComputeRootDescriptorTable(1,srvManager_->GetGPUDescriptorHandle(group->freeCounterUavIndex));
		uint32_t groupCount = (group->kNumMaxInstance + 255) / 256;
		commandList->Dispatch(groupCount,1,1);

		// UAVからSRVへ戻すバリア
		D3D12_RESOURCE_BARRIER barrierToSRV{};
		barrierToSRV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrierToSRV.Transition.pResource = group->instancingResource.Get();
		barrierToSRV.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barrierToSRV.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		barrierToSRV.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1,&barrierToSRV);
		group->currentState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		// FreeCounterバリア
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = group->freeCounterResource.Get();
		commandList->ResourceBarrier(1,&barrier);
	}

	// グラフィックス描画処理
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->SetGraphicsRootConstantBufferView(2,perViewResource_->GetGPUVirtualAddress());

	for(auto& [name,group] : particleGroups_){
		if(group->numInstance == 0) continue;

		uint32_t vertexCount = 4;
		if(group->isCylinder){
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->IASetVertexBuffers(0,1,&cylinderVertexBufferView_);
			vertexCount = cylinderVertexCount_;
		} else if(group->isRing){
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->IASetVertexBuffers(0,1,&ringVertexBufferView_);
			vertexCount = ringVertexCount_;
		} else{
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			commandList->IASetVertexBuffers(0,1,&vertexBufferView_);
			vertexCount = 4;
		}

		commandList->SetGraphicsRootDescriptorTable(0,srvManager_->GetGPUDescriptorHandle(group->textureSrvIndex));
		commandList->SetGraphicsRootDescriptorTable(1,srvManager_->GetGPUDescriptorHandle(group->instancingSrvIndex));

		commandList->DrawInstanced(vertexCount,group->numInstance,0,0);
	}
}

// -------------------------------------------------
// 内部ヘルパー
// -------------------------------------------------

Particle ParticleManager::MakeNewParticle(const Vector3& translate){
	Particle particle;
	std::uniform_real_distribution<float> distributionVel(-1.0f,1.0f);
	std::uniform_real_distribution<float> distColor(0.0f,1.0f);
	std::uniform_real_distribution<float> distTime(1.0f,3.0f);

	particle.translate = translate;
	particle.velocity = {distributionVel(randomEngine_), distributionVel(randomEngine_), distributionVel(randomEngine_)};
	particle.color = {distColor(randomEngine_), distColor(randomEngine_), distColor(randomEngine_), 1.0f};
	particle.lifeTime = distTime(randomEngine_);
	particle.currentTime = 0.0f;

	particle.scale = {1.0f, 1.0f, 1.0f};
	return particle;
}
void ParticleManager::Emit(const std::string& name,const Transform& emitterTransform,uint32_t count,const Vector4& color,const Vector3& velocity,float lifeTime){
	if(!particleGroups_.contains(name)) return;
	auto& group = particleGroups_[name];

	// エミッターのパラメータを更新
	group->emitterData->translate = emitterTransform.translate;
	group->emitterData->count = count;
	group->emitterData->emit = 1;

	group->emitterData->color = color;
	group->emitterData->velocity = velocity;
	group->emitterData->lifeTime = lifeTime;
}

void ParticleManager::CreateGraphicsPipeline(){
	HRESULT hr = S_OK;

	D3D12_DESCRIPTOR_RANGE descriptorRangeTexture[1] = {};
	descriptorRangeTexture[0].BaseShaderRegister = 0;
	descriptorRangeTexture[0].NumDescriptors = 1;
	descriptorRangeTexture[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeTexture[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeInstancing[1] = {};
	descriptorRangeInstancing[0].BaseShaderRegister = 0;
	descriptorRangeInstancing[0].NumDescriptors = 1;
	descriptorRangeInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangeTexture;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeTexture);

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeInstancing;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeInstancing);

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[2].Descriptor.ShaderRegister = 0;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,D3D_ROOT_SIGNATURE_VERSION_1,&signatureBlob,&errorBlob);
	if(FAILED(hr)){ Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer())); assert(false); }
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,signatureBlob->GetBufferPointer(),signatureBlob->GetBufferSize(),IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Particle/Particle.VS.hlsl",L"vs_6_0");
	ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Particle/Particle.PS.hlsl",L"ps_6_0");

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.InputLayout = {inputElementDescs, _countof(inputElementDescs)};
	psoDesc.VS = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
	psoDesc.PS = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};
	D3D12_RENDER_TARGET_BLEND_DESC& blendDesc = psoDesc.BlendState.RenderTarget[0];
	blendDesc.BlendEnable = TRUE;
	blendDesc.LogicOpEnable = FALSE;
	blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.DestBlend = D3D12_BLEND_ONE;
	blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.DepthStencilState.DepthEnable = true;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc,IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

void ParticleManager::CreateModel(){
	VertexData vertices[4] = {
		{{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
		{{-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
		{{ 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
		{{ 1.0f,  1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
	};
	vertexBuffer_ = dxCommon_->CreateBufferResource(sizeof(vertices));
	VertexData* vertexData = nullptr;
	vertexBuffer_->Map(0,nullptr,reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData,vertices,sizeof(vertices));
	vertexBuffer_->Unmap(0,nullptr);
	vertexBufferView_ = {vertexBuffer_->GetGPUVirtualAddress(), sizeof(vertices), sizeof(VertexData)};
}

void ParticleManager::CreateRingModel(){
	const uint32_t kRingDivide = 32;
	const float kOuterRadius = 1.0f,kInnerRadius = 0.2f;
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kRingDivide);

	ringVertexCount_ = kRingDivide * 6;
	std::vector<VertexData> vertices;
	vertices.reserve(ringVertexCount_);
	const Vector3 kNormal = {0.0f, 0.0f, -1.0f};

	for(uint32_t i = 0; i < kRingDivide; ++i){
		float s = std::sin(i * radianPerDivide),c = std::cos(i * radianPerDivide);
		float sN = std::sin((i + 1) * radianPerDivide),cN = std::cos((i + 1) * radianPerDivide);
		float u = float(i) / float(kRingDivide),uN = float(i + 1) / float(kRingDivide);

		vertices.push_back({{-s * kOuterRadius, c * kOuterRadius, 0.0f, 1.0f}, {u, 0.0f}, kNormal});
		vertices.push_back({{-sN * kOuterRadius, cN * kOuterRadius, 0.0f, 1.0f}, {uN, 0.0f}, kNormal});
		vertices.push_back({{-s * kInnerRadius, c * kInnerRadius, 0.0f, 1.0f}, {u, 1.0f}, kNormal});
		vertices.push_back({{-s * kInnerRadius, c * kInnerRadius, 0.0f, 1.0f}, {u, 1.0f}, kNormal});
		vertices.push_back({{-sN * kOuterRadius, cN * kOuterRadius, 0.0f, 1.0f}, {uN, 0.0f}, kNormal});
		vertices.push_back({{-sN * kInnerRadius, cN * kInnerRadius, 0.0f, 1.0f}, {uN, 1.0f}, kNormal});
	}

	size_t size = sizeof(VertexData) * vertices.size();
	ringVertexBuffer_ = dxCommon_->CreateBufferResource(size);
	VertexData* vData = nullptr;
	ringVertexBuffer_->Map(0,nullptr,reinterpret_cast<void**>(&vData));
	std::memcpy(vData,vertices.data(),size);
	ringVertexBuffer_->Unmap(0,nullptr);
	ringVertexBufferView_ = {ringVertexBuffer_->GetGPUVirtualAddress(), (UINT)size, sizeof(VertexData)};
}

void ParticleManager::CreateCylinderModel(){
	const uint32_t kDiv = 32;
	const float kR = 1.0f,kH = 3.0f;
	const float rad = 2.0f * std::numbers::pi_v<float> / float(kDiv);

	cylinderVertexCount_ = kDiv * 6;
	std::vector<VertexData> vtxs;
	vtxs.reserve(cylinderVertexCount_);

	for(uint32_t i = 0; i < kDiv; ++i){
		float s = std::sin(i * rad),c = std::cos(i * rad);
		float sN = std::sin((i + 1) * rad),cN = std::cos((i + 1) * rad);
		float u = float(i) / float(kDiv),uN = float(i + 1) / float(kDiv);

		vtxs.push_back({{-s * kR, kH, c * kR, 1.0f}, {u, 1.0f}, {-s, 0.0f, c}});
		vtxs.push_back({{-sN * kR, kH, cN * kR, 1.0f}, {uN, 1.0f}, {-sN, 0.0f, cN}});
		vtxs.push_back({{-s * kR, 0.0f, c * kR, 1.0f}, {u, 0.0f}, {-s, 0.0f, c}});
		vtxs.push_back({{-s * kR, 0.0f, c * kR, 1.0f}, {u, 0.0f}, {-s, 0.0f, c}});
		vtxs.push_back({{-sN * kR, kH, cN * kR, 1.0f}, {uN, 1.0f}, {-sN, 0.0f, cN}});
		vtxs.push_back({{-sN * kR, 0.0f, cN * kR, 1.0f}, {uN, 0.0f}, {-sN, 0.0f, cN}});
	}
	size_t size = sizeof(VertexData) * vtxs.size();
	cylinderVertexBuffer_ = dxCommon_->CreateBufferResource(size);
	VertexData* p = nullptr;
	cylinderVertexBuffer_->Map(0,nullptr,reinterpret_cast<void**>(&p));
	std::memcpy(p,vtxs.data(),size);
	cylinderVertexBuffer_->Unmap(0,nullptr);
	cylinderVertexBufferView_ = {cylinderVertexBuffer_->GetGPUVirtualAddress(), (UINT)size, sizeof(VertexData)};
}

void ParticleManager::CreateComputePipeline(){
	HRESULT hr = S_OK;
	D3D12_DESCRIPTOR_RANGE uavRange0 = {};
	uavRange0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange0.NumDescriptors = 1;
	uavRange0.BaseShaderRegister = 0;

	D3D12_DESCRIPTOR_RANGE uavRange1 = {};
	uavRange1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange1.NumDescriptors = 1;
	uavRange1.BaseShaderRegister = 1;

	// 更新用 RootSignature
	D3D12_ROOT_PARAMETER param[2] = {};
	param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param[0].DescriptorTable.NumDescriptorRanges = 1;
	param[0].DescriptorTable.pDescriptorRanges = &uavRange0;
	param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param[1].DescriptorTable.NumDescriptorRanges = 1;
	param[1].DescriptorTable.pDescriptorRanges = &uavRange1;
	param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC desc = {2, param, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
	ComPtr<ID3DBlob> sig,err;
	hr = D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&err);
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&computeRootSignature_));

	ComPtr<IDxcBlob> cs = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Particle/UpdateParticle.CS.hlsl",L"cs_6_0");
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {computeRootSignature_.Get(), {cs->GetBufferPointer(), cs->GetBufferSize()}};
	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc,IID_PPV_ARGS(&computePipelineState_));
	assert(SUCCEEDED(hr));

	// Emit用 RootSignature
	D3D12_ROOT_PARAMETER rootParametersEmit[4] = {};
	rootParametersEmit[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParametersEmit[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParametersEmit[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParametersEmit[0].DescriptorTable.pDescriptorRanges = &uavRange0;

	rootParametersEmit[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParametersEmit[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParametersEmit[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParametersEmit[1].DescriptorTable.pDescriptorRanges = &uavRange1;

	rootParametersEmit[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParametersEmit[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParametersEmit[2].Descriptor.ShaderRegister = 0;

	rootParametersEmit[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParametersEmit[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParametersEmit[3].Descriptor.ShaderRegister = 1;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignatureEmit{};
	descriptionRootSignatureEmit.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	descriptionRootSignatureEmit.pParameters = rootParametersEmit;
	descriptionRootSignatureEmit.NumParameters = _countof(rootParametersEmit);

	ComPtr<ID3DBlob> signatureBlobEmit;
	ComPtr<ID3DBlob> errorBlobEmit;
	hr = D3D12SerializeRootSignature(&descriptionRootSignatureEmit,D3D_ROOT_SIGNATURE_VERSION_1,&signatureBlobEmit,&errorBlobEmit);
	if(FAILED(hr)){
		Logger::Log(reinterpret_cast<char*>(errorBlobEmit->GetBufferPointer()));
		assert(false);
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,signatureBlobEmit->GetBufferPointer(),signatureBlobEmit->GetBufferSize(),IID_PPV_ARGS(&emitComputeRootSignature_));
	assert(SUCCEEDED(hr));

	// Emit用PSO作成
	ComPtr<IDxcBlob> emitComputeShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Particle/EmitParticle.CS.hlsl",L"cs_6_0");
	assert(emitComputeShaderBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC emitComputePipelineStateDesc{};
	emitComputePipelineStateDesc.pRootSignature = emitComputeRootSignature_.Get();
	emitComputePipelineStateDesc.CS = {emitComputeShaderBlob->GetBufferPointer(), emitComputeShaderBlob->GetBufferSize()};

	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&emitComputePipelineStateDesc,IID_PPV_ARGS(&emitComputePipelineState_));
	assert(SUCCEEDED(hr));
}

// -------------------------------------------------
// パーティクル発生処理
// -------------------------------------------------

void ParticleManager::EmitShockwave(const Vector3& pos){
	ParticleManager::GetInstance()->Emit("Shockwave",{{0.1f, 0.1f, 0.1f}, {0, 0, 0}, pos},1,{1, 1, 1, 1},{0, 0, 0},0.3f);
}

void ParticleManager::EmitSpark(const Vector3& position){
	Transform transform;
	transform.translate = position;
	transform.scale = {0.05f, 0.05f, 0.05f};
	transform.rotate = {0.0f, 0.0f, 0.0f};

	ParticleManager::GetInstance()->Emit("Spark",transform,20,{1.0f, 0.5f, 0.0f, 1.0f},{0.0f, 0.0f, 0.0f},0.5f);
}

void ParticleManager::EmitSmoke(const Vector3& position){
	Transform transform;
	transform.translate = position;
	transform.scale = {0.2f, 0.2f, 0.2f};
	transform.rotate = {0.0f, 0.0f, 0.0f};

	ParticleManager::GetInstance()->Emit("Smoke",transform,5,{0.5f, 0.5f, 0.5f, 0.8f},{0.0f, 1.0f, 0.0f},1.5f);
}

void ParticleManager::EmitCharge(const Vector3& position){
	Transform transform;
	transform.translate = position;
	transform.scale = {0.1f, 0.5f, 0.1f};
	transform.rotate = {0.0f, 0.0f, 0.0f};

	ParticleManager::GetInstance()->Emit("Charge",transform,30,{0.2f, 0.8f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f},0.6f);
}

void ParticleManager::EmitAura(const Vector3& position){
	Transform transform;
	transform.translate = position;
	transform.scale = {0.3f, 0.3f, 0.3f};
	transform.rotate = {0.0f, 0.0f, 0.0f};

	ParticleManager::GetInstance()->Emit("Aura",transform,1,{0.8f, 1.0f, 0.2f, 0.6f},{0.0f, 2.0f, 0.0f},1.0f);
}

void ParticleManager::EmitWarp(){
	Transform transform;
	transform.translate = {0.0f, 0.0f, 0.0f};
	transform.scale = {1.0f, 1.0f, 1.0f};
	transform.rotate = {0.0f, 0.0f, 0.0f};

	ParticleManager::GetInstance()->Emit("Warp",transform,100,{0.5f, 0.8f, 1.0f, 0.8f},{0.0f, 0.0f, 0.0f},1.0f);
}
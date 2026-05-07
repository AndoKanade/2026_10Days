#include "ParticleManager.h"
#include "TextureManager.h"
#include "Camera.h"
#include "Logger.h"
#include <cassert>
#include <random>
#include <numbers>

using namespace Microsoft::WRL;

// シングルトン管理
ParticleManager* ParticleManager::GetInstance(){
	static ParticleManager instance;
	return &instance;
}

void ParticleManager::Finalize(){
	// 登録されている全パーティクルグループを解放
	particleGroups_.clear();

	// 共有リソースの解放
	rootSignature_.Reset();
	graphicsPipelineState_.Reset();
	vertexBuffer_.Reset();
	vertexResource_.Reset();
}

// 初期化
void ParticleManager::Initialize(DXCommon* dxCommon,SrvManager* srvManager){
	assert(dxCommon);
	assert(srvManager);

	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	// 乱数生成器の初期化
	std::random_device seedGenerator;
	randomEngine_.seed(seedGenerator());

	// 描画に必要な共通リソースを作成
	CreateGraphicsPipeline();
	CreateModel();
}

// パーティクルグループ管理
void ParticleManager::CreateParticleGroup(const std::string& name,const std::string& textureFilePath){
	if(particleGroups_.contains(name)){
		return;
	}

	std::unique_ptr<ParticleGroup> group = std::make_unique<ParticleGroup>();

	// テクスチャ設定
	group->textureFilePath = textureFilePath;
	TextureManager::GetInstance()->LoadTexture(textureFilePath);
	group->textureSrvIndex = TextureManager::GetInstance()->GetSrvIndex(textureFilePath);

	// インスタンシング用リソース生成
	group->instancingResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * group->kNumMaxInstance);
	group->instancingResource->Map(0,nullptr,reinterpret_cast<void**>(&group->instancingData));

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

	particleGroups_.insert(std::make_pair(name,std::move(group)));
}

// 更新・描画
void ParticleManager::Update(Camera* camera){
	assert(camera);

	const float kDeltaTime = 1.0f / 60.0f;

	// カメラ行列の取得
	Matrix4x4 viewMatrix = camera->GetViewMatrix();
	Matrix4x4 projectionMatrix = camera->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix,projectionMatrix);

	// ビルボード行列の作成
	Matrix4x4 billboardMatrix = camera->GetWorldMatrix();
	billboardMatrix.m[3][0] = 0.0f;
	billboardMatrix.m[3][1] = 0.0f;
	billboardMatrix.m[3][2] = 0.0f;

	// 全グループの更新
	for(auto& [name,group] : particleGroups_){
		uint32_t numInstance = 0;

		for(auto it = group->particles.begin(); it != group->particles.end(); ){
			// 寿命切れチェック
			if(it->lifeTime <= it->currentTime){
				it = group->particles.erase(it);
				continue;
			}

			// 移動更新
			it->transform.translate += it->velocity * kDeltaTime;
			it->currentTime += kDeltaTime;

			// アルファ値計算
			float alpha = 1.0f - (it->currentTime / it->lifeTime);

			// ワールド行列計算
			Matrix4x4 scaleMatrix = MakeScaleMatrix(it->transform.scale);
			Matrix4x4 rotateMatrix = MakeRotateZMatrix(it->transform.rotate.z);
			Matrix4x4 translateMatrix = MakeTranslateMatrix(it->transform.translate);
			Matrix4x4 worldMatrix = Multiply(scaleMatrix,Multiply(rotateMatrix,Multiply(billboardMatrix,translateMatrix)));

			// WVP行列計算
			Matrix4x4 wvpMatrix = Multiply(worldMatrix,viewProjectionMatrix);

			// インスタンシングバッファへの書き込み
			if(numInstance < group->kNumMaxInstance){
				group->instancingData[numInstance].WVP = wvpMatrix;
				group->instancingData[numInstance].World = worldMatrix;
				group->instancingData[numInstance].color = it->color;
				group->instancingData[numInstance].color.w = alpha;

				++numInstance;
			}
			++it;
		}

		// 描画するインスタンス数を保存
		group->numInstance = numInstance;
	}
}

void ParticleManager::Draw(const Matrix4x4& viewProjectionMatrix){
	assert(dxCommon_);
	auto commandList = dxCommon_->GetCommandList();

	// 共通設定
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->IASetVertexBuffers(0,1,&vertexBufferView_);

	// グループごとの描画
	for(auto& [name,group] : particleGroups_){
		if(group->numInstance == 0){
			continue;
		}

		// テクスチャ SRV をセット
		D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvManager_->GetGPUDescriptorHandle(group->textureSrvIndex);
		commandList->SetGraphicsRootDescriptorTable(0,textureSrvHandleGPU);

		// インスタンシング用 SRV をセット
		D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU = srvManager_->GetGPUDescriptorHandle(group->instancingSrvIndex);
		commandList->SetGraphicsRootDescriptorTable(1,instancingSrvHandleGPU);

		// インスタンシング描画実行
		commandList->DrawInstanced(4,group->numInstance,0,0);
	}
}

// 内部ヘルパー
Particle ParticleManager::MakeNewParticle(const Vector3& translate){
	Particle particle;

	// 乱数分布の設定
	std::uniform_real_distribution<float> distributionPos(-1.0f,1.0f);
	std::uniform_real_distribution<float> distributionVel(-1.0f,1.0f);
	std::uniform_real_distribution<float> distColor(0.0f,1.0f);
	std::uniform_real_distribution<float> distTime(1.0f,3.0f);

	// 初期値設定
	Vector3 randomTranslate = {distributionPos(randomEngine_), distributionPos(randomEngine_), distributionPos(randomEngine_)};
	particle.transform.translate = translate + randomTranslate;
	particle.velocity = {distributionVel(randomEngine_), distributionVel(randomEngine_), distributionVel(randomEngine_)};
	particle.color = {distColor(randomEngine_), distColor(randomEngine_), distColor(randomEngine_), 1.0f};
	particle.lifeTime = distTime(randomEngine_);
	particle.currentTime = 0.0f;
	particle.transform.scale = {1.0f, 1.0f, 1.0f};
	particle.transform.rotate = {0.0f, 0.0f, 0.0f};

	return particle;
}

void ParticleManager::Emit(const std::string& name,const Transform& emitterTransform,uint32_t count,const Vector4& color,const Vector3& velocity,float lifeTime){
	if(particleGroups_.contains(name)){
		auto& group = particleGroups_[name];

		// ランダム分布の設定
		std::uniform_real_distribution<float> distRotate(-std::numbers::pi_v<float>,std::numbers::pi_v<float>);
		std::uniform_real_distribution<float> distScale(0.4f,1.5f);

		for(uint32_t i = 0; i < count; ++i){
			Particle newParticle = MakeNewParticle(emitterTransform.translate);

			// パラメータの上書き
			newParticle.transform.translate = emitterTransform.translate;
			newParticle.transform.scale = {
				emitterTransform.scale.x,
				emitterTransform.scale.y * distScale(randomEngine_),
				emitterTransform.scale.z
			};
			newParticle.transform.rotate = {0.0f, 0.0f, distRotate(randomEngine_)};
			newParticle.color = color;
			newParticle.velocity = velocity;
			newParticle.lifeTime = lifeTime;

			group->particles.push_back(newParticle);
		}
	}
}

// グラフィックスパイプライン生成
void ParticleManager::CreateGraphicsPipeline(){
	HRESULT hr = S_OK;

	// RootSignature の作成
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

	D3D12_ROOT_PARAMETER rootParameters[2] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangeTexture;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeTexture);

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeInstancing;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeInstancing);

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
	if(FAILED(hr)){
		Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,signatureBlob->GetBufferPointer(),signatureBlob->GetBufferSize(),IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	// Pipeline State Object (PSO) の作成
	ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Particle/Particle.VS.hlsl",L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Particle/Particle.PS.hlsl",L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
	graphicsPipelineStateDesc.PS = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

void ParticleManager::CreateModel(){
	// 板ポリゴンの頂点データ
	VertexData vertices[4] = {
		{{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
		{{-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
		{{ 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
		{{ 1.0f,  1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
	};

	// バッファリソース生成
	vertexBuffer_ = dxCommon_->CreateBufferResource(sizeof(vertices));

	// データ転送
	VertexData* vertexData = nullptr;
	vertexBuffer_->Map(0,nullptr,reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData,vertices,sizeof(vertices));
	vertexBuffer_->Unmap(0,nullptr);

	// ビュー設定
	vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(vertices);
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
}
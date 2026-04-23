#include "Obj3DCommon.h"
#include "Logger.h" // ログ機能がある場合
#include <cassert>

using namespace Microsoft::WRL;

void Obj3dCommon::Initialize(DXCommon* dxcommon){
	assert(dxcommon);
	dxCommon_ = dxcommon;

	// パイプライン生成（この中でルートシグネチャ生成も呼ばれます）
	CreateGraphicsPipelineState();
	size_t sizeInBytes = (sizeof(CameraForGPU) + 0xff) & ~0xff;

	// 2. リソース作成
	cameraResource_ = dxCommon_->CreateBufferResource(sizeInBytes);

	// 3. データを書き込めるようにアドレスを取得 (Map)
	HRESULT hr = cameraResource_->Map(0,nullptr,reinterpret_cast<void**>(&cameraData_));
	assert(SUCCEEDED(hr));

	// 4. 初期値を入れておく
	cameraData_->worldPosition = {0.0f, 0.0f, 0.0f};

}

// 描画共通設定（描画ループの前に一度だけ呼ぶ）
void Obj3dCommon::Draw(){
	// コマンドリスト取得
	// ※GetCommandList() の名前はご自身のDXCommonの実装に合わせてください
	auto commandList = dxCommon_->GetCommandList();

	// ルートシグネチャとPSOをセット
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->SetPipelineState(graphicsPipelineState.Get());

	// トポロジ設定（三角形リスト）
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	if(cameraResource_){
		// 現在のカメラ座標を更新 (Map済みデータに書き込み)
		if(defaultCamera_){
			cameraData_->worldPosition = defaultCamera_->GetTranslate();
		}

		// GPUにアドレスを渡す
		commandList->SetGraphicsRootConstantBufferView(4,cameraResource_->GetGPUVirtualAddress());
	}

}

void Obj3dCommon::CreateRootSignature(){
	HRESULT hr;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	// 1. ルートパラメータの設定
	// 一般的な3D用構成: [0]Material(CBV), [1]Transform(CBV), [2]Texture(DescTable), [3]Light(CBV), [4]Camera(CBV)
	D3D12_ROOT_PARAMETER rootParameters[5] = {};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 1;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges =
		_countof(descriptorRange);

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 2;

	// [4] Camera (b3)
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 3; // レジスタ番号 b3

	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};

	descriptorRangeForInstancing[0].BaseShaderRegister = 0;
	descriptorRangeForInstancing[0].NumDescriptors = 1;
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParametersInstancing[4] = {};

	rootParametersInstancing[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParametersInstancing[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParametersInstancing[0].Descriptor.ShaderRegister = 0;

	rootParametersInstancing[1].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParametersInstancing[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParametersInstancing[1].DescriptorTable.pDescriptorRanges =
		descriptorRangeForInstancing;
	rootParametersInstancing[1].DescriptorTable.NumDescriptorRanges =
		_countof(descriptorRangeForInstancing);

	rootParametersInstancing[2].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParametersInstancing[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParametersInstancing[2].DescriptorTable.pDescriptorRanges =
		descriptorRange;
	rootParametersInstancing[2].DescriptorTable.NumDescriptorRanges =
		_countof(descriptorRangeForInstancing);

	rootParametersInstancing[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParametersInstancing[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParametersInstancing[3].Descriptor.ShaderRegister = 2;


	// 2. サンプラーの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// 3. ルートシグネチャの設定
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズ
	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,D3D_ROOT_SIGNATURE_VERSION_1,&signatureBlob,&errorBlob);
	if(FAILED(hr)){
		Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// 生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,signatureBlob->GetBufferPointer(),signatureBlob->GetBufferSize(),IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));
}

void Obj3dCommon::CreateGraphicsPipelineState(){
	HRESULT hr;

	CreateRootSignature();

	// 1. シェーダーコンパイル
	// DXCommonにCompileShaderがある前提です
	ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Obj3D/Object3d.VS.hlsl",L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Obj3D/Object3d.PS.hlsl",L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// 2. InputLayout (3D用: POSITION, TEXCOORD, NORMAL)
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// 3. BlendState
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	// 必要に応じてブレンド有効化設定を追加してください

	// 4. RasterizerState (3Dはカリング有効: BACK)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// 5. DepthStencilState (3Dは深度有効: WriteMask_ALL)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// 6. PSO生成設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
	graphicsPipelineStateDesc.PS = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// レンダーターゲット設定
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// 生成実行
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));
}
#include "Obj3dCommon.h"
#include "Logger.h"
#include <cassert>
#include <ModelManager.h>

using namespace Microsoft::WRL;

void Obj3dCommon::Initialize(DXCommon* dxcommon){
    assert(dxcommon);
    dxCommon_ = dxcommon;

    CreateGraphicsPipelineState();

    // --- 1. カメラリソース作成 (256バイトアライメント) ---
    size_t cameraSize = (sizeof(CameraForGPU) + 0xff) & ~0xff;
    cameraResource_ = dxCommon_->CreateBufferResource(cameraSize);
    cameraResource_->Map(0,nullptr,reinterpret_cast<void**>(&cameraData_));
    cameraData_->worldPosition = {0.0f, 0.0f, 0.0f};

    // --- 2. 平行光源リソース作成 (256バイトアライメント) ---
    size_t directionalSize = (sizeof(DirectionalLight) + 0xff) & ~0xff;
    directionalLightResource_ = dxCommon_->CreateBufferResource(directionalSize);
    directionalLightResource_->Map(0,nullptr,reinterpret_cast<void**>(&directionalLightData_));
    directionalLightData_->color = {1.0f, 1.0f, 1.0f, 1.0f};
    directionalLightData_->direction = {0.0f, -1.0f, 0.0f};
    directionalLightData_->intensity = 1.0f;

    // --- 3. 点光源リソース作成 (256バイトアライメント) ---
    size_t pointSize = (sizeof(PointLight) + 0xff) & ~0xff;
    pointLightResource_ = dxCommon_->CreateBufferResource(pointSize);
    pointLightResource_->Map(0,nullptr,reinterpret_cast<void**>(&pointLightData_));
    pointLightData_->color = {1.0f, 1.0f, 1.0f, 1.0f};
    pointLightData_->position = {0.0f, 2.0f, 0.0f};
    pointLightData_->intensity = 0.0f;
    pointLightData_->radius = 10.0f;
    pointLightData_->decay = 2.0f;

    // --- 4. スポットライトリソース作成 (256バイトアライメント) ---
    size_t spotSize = (sizeof(SpotLight) + 0xff) & ~0xff;
    spotLightResource_ = dxCommon_->CreateBufferResource(spotSize);
    spotLightResource_->Map(0,nullptr,reinterpret_cast<void**>(&spotLightData_));
    spotLightData_->color = {1.0f, 1.0f, 1.0f, 1.0f};
    spotLightData_->position = {0.0f, 5.0f, 0.0f};
    spotLightData_->direction = {0.0f, -1.0f, 0.0f};
    spotLightData_->intensity = 0.0f;
    spotLightData_->distance = 20.0f;
    spotLightData_->cosAngle = cosf(30.0f * 3.141592f / 180.0f);
    spotLightData_->cosFalloffStart = cosf(20.0f * 3.141592f / 180.0f);
    spotLightData_->decay = 2.0f;
}

void Obj3dCommon::Draw(){
    auto commandList = dxCommon_->GetCommandList();

    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->SetPipelineState(graphicsPipelineState.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 平行光源設定 (b2) - ModelManager経由
    auto lightRes = ModelManager::GetInstance()->GetModelCommon()->GetLightResource();
    commandList->SetGraphicsRootConstantBufferView(3,lightRes->GetGPUVirtualAddress());

    // カメラ設定 (b3)
    if(cameraResource_){
        if(defaultCamera_){
            cameraData_->worldPosition = defaultCamera_->GetTranslate();
        }
        commandList->SetGraphicsRootConstantBufferView(4,cameraResource_->GetGPUVirtualAddress());
    }

    // 点光源設定 (b4)
    if(pointLightResource_){
        commandList->SetGraphicsRootConstantBufferView(5,pointLightResource_->GetGPUVirtualAddress());
    }

    // スポットライト設定 (b5)
    if(spotLightResource_){
        commandList->SetGraphicsRootConstantBufferView(6,spotLightResource_->GetGPUVirtualAddress());
    }
}

void Obj3dCommon::CreateRootSignature(){
    HRESULT hr;

    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[7] = {};

    // [0] Material (b0)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // [1] Transform (b1)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 1;

    // [2] Texture (t0)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    // [3] DirectionalLight (b2)
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 2;

    // [4] Camera (b3)
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[4].Descriptor.ShaderRegister = 3;

    // [5] PointLight (b4)
    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[5].Descriptor.ShaderRegister = 4;

    // [6] SpotLight (b5)
    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[6].Descriptor.ShaderRegister = 5;

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

    hr = dxCommon_->GetDevice()->CreateRootSignature(0,signatureBlob->GetBufferPointer(),signatureBlob->GetBufferSize(),IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));
}

void Obj3dCommon::CreateGraphicsPipelineState(){
    HRESULT hr;
    CreateRootSignature();

    ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Obj3D/Object3d.VS.hlsl",L"vs_6_0");
    assert(vertexShaderBlob != nullptr);
    ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Obj3D/Object3d.PS.hlsl",L"ps_6_0");
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

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
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

    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,IID_PPV_ARGS(&graphicsPipelineState));
    assert(SUCCEEDED(hr));
}
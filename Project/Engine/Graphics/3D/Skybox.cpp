#include "Skybox.h"
#include "SkyboxCommon.h"
#include "DXCommon.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include <cassert>

void Skybox::Initialize(SkyboxCommon* skyboxCommon,const std::string& texturePath){
	skyboxCommon_ = skyboxCommon;
	auto dxCommon = skyboxCommon_->GetDxCommon();

	// 定数バッファ生成
	materialResource_ = dxCommon->CreateBufferResource(sizeof(SkyboxMaterial));
	materialResource_->Map(0,nullptr,reinterpret_cast<void**>(&materialData_));
	materialData_->color = {1.0f, 1.0f, 1.0f, 1.0f};

	wvpResource_ = dxCommon->CreateBufferResource(sizeof(SkyboxTransformationMatrix));
	wvpResource_->Map(0,nullptr,reinterpret_cast<void**>(&wvpData_));

	// メッシュ生成
	CreateMesh();

	// テクスチャ読み込み
	TextureManager::GetInstance()->LoadTexture(texturePath);
	srvIndex_ = TextureManager::GetInstance()->GetSrvIndex(texturePath);
}

void Skybox::Update(const Camera& camera){
	// 変更：500倍スケーリングで巨大化したうえで、Y軸まわりに回す。
	// ピクセルシェーダのサンプリング方向は頂点のローカル座標なので、
	// ワールド行列を回すと空そのものが回って見える。
	Matrix4x4 worldMatrix = Multiply(
		MakeScaleMatrix({500.0f, 500.0f, 500.0f}),
		MakeRotateYMatrix(rotationY_));

	// カメラの平行移動を無効化
	Matrix4x4 viewMatrix = camera.GetViewMatrix();
	viewMatrix.m[3][0] = 0.0f;
	viewMatrix.m[3][1] = 0.0f;
	viewMatrix.m[3][2] = 0.0f;

	Matrix4x4 projectionMatrix = camera.GetProjectionMatrix();
	Matrix4x4 worldViewMatrix = Multiply(worldMatrix,viewMatrix);

	// 定数バッファへ書き込み
	wvpData_->WVP = Multiply(worldViewMatrix,projectionMatrix);
}

void Skybox::CreateMesh(){
	Vertex vertices[24];
	vertices[0].pos = {1.0f,  1.0f,  1.0f, 1.0f};
	vertices[1].pos = {1.0f,  1.0f, -1.0f, 1.0f};
	vertices[2].pos = {1.0f, -1.0f,  1.0f, 1.0f};
	vertices[3].pos = {1.0f, -1.0f, -1.0f, 1.0f};
	vertices[4].pos = {-1.0f,  1.0f, -1.0f, 1.0f};
	vertices[5].pos = {-1.0f,  1.0f,  1.0f, 1.0f};
	vertices[6].pos = {-1.0f, -1.0f, -1.0f, 1.0f};
	vertices[7].pos = {-1.0f, -1.0f,  1.0f, 1.0f};
	vertices[8].pos = {-1.0f,  1.0f,  1.0f, 1.0f};
	vertices[9].pos = {1.0f,  1.0f,  1.0f, 1.0f};
	vertices[10].pos = {-1.0f, -1.0f,  1.0f, 1.0f};
	vertices[11].pos = {1.0f, -1.0f,  1.0f, 1.0f};
	vertices[12].pos = {1.0f,  1.0f, -1.0f, 1.0f};
	vertices[13].pos = {-1.0f,  1.0f, -1.0f, 1.0f};
	vertices[14].pos = {1.0f, -1.0f, -1.0f, 1.0f};
	vertices[15].pos = {-1.0f, -1.0f, -1.0f, 1.0f};
	vertices[16].pos = {-1.0f,  1.0f, -1.0f, 1.0f};
	vertices[17].pos = {1.0f,  1.0f, -1.0f, 1.0f};
	vertices[18].pos = {-1.0f,  1.0f,  1.0f, 1.0f};
	vertices[19].pos = {1.0f,  1.0f,  1.0f, 1.0f};
	vertices[20].pos = {-1.0f, -1.0f,  1.0f, 1.0f};
	vertices[21].pos = {1.0f, -1.0f,  1.0f, 1.0f};
	vertices[22].pos = {-1.0f, -1.0f, -1.0f, 1.0f};
	vertices[23].pos = {1.0f, -1.0f, -1.0f, 1.0f};

	uint32_t indices[36] = {
		0, 1, 2, 2, 1, 3,
		4, 5, 6, 6, 5, 7,
		8, 9, 10, 10, 9, 11,
		12, 13, 14, 14, 13, 15,
		16, 17, 18, 18, 17, 19,
		20, 21, 22, 22, 21, 23
	};

	auto dxCommon = skyboxCommon_->GetDxCommon();

	// 頂点バッファ
	vertexBuffer_ = dxCommon->CreateBufferResource(sizeof(vertices));
	void* vertexPtr = nullptr;
	vertexBuffer_->Map(0,nullptr,&vertexPtr);
	std::memcpy(vertexPtr,vertices,sizeof(vertices));
	vertexBuffer_->Unmap(0,nullptr);

	vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(vertices);
	vertexBufferView_.StrideInBytes = sizeof(Vertex);

	// インデックスバッファ
	indexBuffer_ = dxCommon->CreateBufferResource(sizeof(indices));
	void* indexPtr = nullptr;
	indexBuffer_->Map(0,nullptr,&indexPtr);
	std::memcpy(indexPtr,indices,sizeof(indices));
	indexBuffer_->Unmap(0,nullptr);

	indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(indices);
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
}

void Skybox::Draw(){
	auto commandList = skyboxCommon_->GetDxCommon()->GetCommandList();

	// 共通設定（RootSignature/PSO）をセット
	commandList->SetGraphicsRootSignature(skyboxCommon_->GetRootSignature());
	commandList->SetPipelineState(skyboxCommon_->GetPipelineState());

	commandList->IASetVertexBuffers(0,1,&vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 定数バッファ
	commandList->SetGraphicsRootConstantBufferView(0,materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1,wvpResource_->GetGPUVirtualAddress());

	// SRV
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2,srvIndex_);

	commandList->DrawIndexedInstanced(36,1,0,0,0);
}
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include"SrvManager.h"

void Sprite::Initialize(SpriteCommon* spriteCommon,std::string textureFilePath){
	this->spriteCommon = spriteCommon;

	this->textureFilePath_ = textureFilePath;

	CreateVertexData();
	CreateMaterialData();
	CreateTransformationMatrixData();
	textureIndex = TextureManager::GetInstance()->GetSrvIndex(textureFilePath);
	AdjustTextureSize();
}

void Sprite::Update(){

	float left = 0.0f - anchorPoint.x;
	float right = 1.0f - anchorPoint.x;
	float top = 0.0f - anchorPoint.y;
	float bottom = 1.0f - anchorPoint.y;

	// 左右反転
	if(isFlipX_){
		left = -left;
		right = -right;
	}

	// 上下反転
	if(isFlipY_){
		top = -top;
		bottom = -bottom;
	}

	// ★追加: テクスチャのメタデータを取得してUV座標を計算
	const DirectX::TexMetadata& metadata =
		TextureManager::GetInstance()->GetMetaData(textureFilePath_);

	float tex_left = textureLeftTop.x / metadata.width;
	float tex_right = (textureLeftTop.x + textureSize.x) / metadata.width;
	float tex_top = textureLeftTop.y / metadata.height;
	float tex_bottom = (textureLeftTop.y + textureSize.y) / metadata.height;


	// 頂点リソースへのデータ書き込み
	vertexResource->Map(0,nullptr,reinterpret_cast<void**>(&vertexData));

	// 1. 左上 (Top-Left)
	vertexData[0].position = {left, top, 0.0f, 1.0f};
	vertexData[0].texcoord = {tex_left, tex_top}; // ★ここを変更
	vertexData[0].normal = {0.0f, 0.0f, -1.0f};

	// 2. 左下 (Bottom-Left)
	vertexData[1].position = {left, bottom, 0.0f, 1.0f};
	vertexData[1].texcoord = {tex_left, tex_bottom}; // ★ここを変更
	vertexData[1].normal = {0.0f, 0.0f, -1.0f};

	// 3. 右上 (Top-Right)
	vertexData[2].position = {right, top, 0.0f, 1.0f};
	vertexData[2].texcoord = {tex_right, tex_top}; // ★ここを変更
	vertexData[2].normal = {0.0f, 0.0f, -1.0f};

	// 4. 右下 (Bottom-Right)
	vertexData[3].position = {right, bottom, 0.0f, 1.0f};
	vertexData[3].texcoord = {tex_right, tex_bottom}; // ★ここを変更
	vertexData[3].normal = {0.0f, 0.0f, -1.0f};

	// (Mapの解除はデストラクタ等で自動で行われない場合は Unmap が必要ですが、
	//  この書き方なら書き込みっぱなしでOKな設定になっている前提です)


	// インデックスデータの更新
	indexResource->Map(0,nullptr,reinterpret_cast<void**>(&indexData));

	// 三角形1: 左上(0) → 左下(1) → 右上(2)
	indexData[0] = 0;
	indexData[1] = 1;
	indexData[2] = 2;

	// 三角形2: 左下(1) → 右下(3) → 右上(2)
	indexData[3] = 1;
	indexData[4] = 3;
	indexData[5] = 2;
	/// Sprite用のWorldViewProjectionMatrixを作る

	Transform transform{
	{1.0f, 1.0f, 1.0f},
	{0.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 0.0f},

	};

	transform.translate = {position.x,position.y,0.0f};
	transform.rotate = {0.0f,0.0f,rotation};
	transform.scale = {size.x,size.y,1.0f};


	worldMatrix =
		MakeAffineMatrix(transform.scale,transform.rotate,
			transform.translate);
	viewMatrix = MakeIdentity4x4();
	projectionMatrix =
		MakeOrthographicMatrix(0.0f,0.0f,float(WinAPI::kClientWidth),
			float(WinAPI::kClientHeight),0.0f,100.0f);

	transformationMatrixData->WVP = Multiply(worldMatrix,Multiply(viewMatrix,projectionMatrix));
	transformationMatrixData->World = worldMatrix;

}

void Sprite::Draw(){
	spriteCommon->GetDxCommon()->GetCommandList()->IASetVertexBuffers(0,1,
		&vertexBufferView);
	spriteCommon->GetDxCommon()->GetCommandList()->IASetIndexBuffer(&indexBufferView);


	spriteCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(
		0,materialResource->GetGPUVirtualAddress());
	spriteCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(
		1,transformationMatrixResource->GetGPUVirtualAddress());
	spriteCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(
		2,SrvManager::GetInstance()->GetGPUDescriptorHandle(textureIndex)
	);

	spriteCommon->GetDxCommon()->GetCommandList()->DrawIndexedInstanced(6,1,0,0,0);


}

void Sprite::CreateVertexData(){

	vertexResource =
		spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * 6);

	indexResource =
		spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * 6);

	vertexBufferView.BufferLocation =
		vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = sizeof(VertexData) * 6;
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	indexBufferView.BufferLocation =
		indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = sizeof(uint32_t) * 6;
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	vertexResource->Map(0,nullptr,reinterpret_cast<void**>(&vertexData));
	indexResource->Map(0,nullptr,reinterpret_cast<void**>(&indexData));

}

void Sprite::CreateMaterialData(){

	materialResource =
		spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(Material));
	materialResource->Map(0,nullptr,
		reinterpret_cast<void**>(&materialData));

	materialData->color = Vector4(1.0f,1.0f,1.0f,1.0f);

	materialData->enableLighting = false;
	materialData->uvTransform = MakeIdentity4x4();

}

void Sprite::CreateTransformationMatrixData(){

	transformationMatrixResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));

	transformationMatrixResource->Map(
		0,nullptr,reinterpret_cast<void**>(&transformationMatrixData));
	transformationMatrixData->WVP = MakeIdentity4x4();
	transformationMatrixData->World = MakeIdentity4x4();

}

void Sprite::AdjustTextureSize(){
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(textureFilePath_);

	textureSize.x = static_cast<float>(metadata.width);
	textureSize.y = static_cast<float>(metadata.height);
	size = textureSize;
}

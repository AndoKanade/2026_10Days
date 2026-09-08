#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include "MyMath.h"
#include "Camera.h"

class SkyboxCommon;

class Skybox{
public:
	struct Vertex{
		Vector4 pos;
	};

	struct SkyboxMaterial{
		Vector4 color;
	};

	struct SkyboxTransformationMatrix{
		Matrix4x4 WVP;
	};

	void Initialize(SkyboxCommon* skyboxCommon,const std::string& texturePath);
	void Update(const Camera& camera);
	void Draw();

	// 追加：天球をY軸まわりに回す角度（ラジアン）。
	// 背景をゆっくり流したいときに、毎フレーム少しずつ足して使う。
	void SetRotationY(float radian){ rotationY_ = radian; }
	float GetRotationY() const{ return rotationY_; }

	// 追加：天球全体に掛ける色。テクスチャの色に乗算される。
	// 明るさを脈打たせるなど、色味を触りたいときに使う。
	void SetColor(const Vector4& color){
		if(materialData_){
			materialData_->color = color;
		}
	}

private:
	void CreateMesh();

private:
	SkyboxCommon* skyboxCommon_ = nullptr;

	// メッシュ関連
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

	// テクスチャ関連
	uint32_t srvIndex_ = 0;

	// 追加：Y軸まわりの回転角（ラジアン）
	float rotationY_ = 0.0f;

	// 定数バッファ関連
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	SkyboxMaterial* materialData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
	SkyboxTransformationMatrix* wvpData_ = nullptr;
};
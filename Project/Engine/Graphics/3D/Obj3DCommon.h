#pragma once
#include "DXCommon.h"
#include <d3d12.h>
#include <wrl.h>
#include "Camera.h"
#include "MyMath.h"
#include "ModelCommon.h"

struct PointLight{
	Vector4 color;
	Vector3 position;
	float intensity;
	float radius;
	float decay;
	float padding[2];
};

struct SpotLight{
	Vector4 color;
	Vector3 position;
	float intensity;
	Vector3 direction;
	float distance;
	float cosAngle;
	float decay;
	float cosFalloffStart;
	float padding[2];
};

// --- Obj3dCommonクラス ---

class Obj3dCommon{
public: // 外部から呼び出すもの

	// 初期化
	void Initialize(DXCommon* dxCommon);

	// 描画設定
	void Draw();

	// セッター
	void SetDefaultCamera(Camera* camera){ this->defaultCamera_ = camera; }

	// ゲッター
	ID3D12RootSignature* GetRootSignature() const{ return rootSignature.Get(); }
	ID3D12PipelineState* GetGraphicsPipelineState() const{ return graphicsPipelineState.Get(); }
	DXCommon* GetDxCommon() const{ return dxCommon_; }
	Camera* GetDefaultCamera() const{ return defaultCamera_; }

	// ライトデータ取得
	DirectionalLight* GetDirectionalLightData(){ return directionalLightData_; }
	PointLight* GetPointLightData(){ return pointLightData_; }
	SpotLight* GetSpotLightData(){ return spotLightData_; }

private: // 内部関数
	void CreateRootSignature();
	void CreateGraphicsPipelineState();

private: // メンバ変数

	DXCommon* dxCommon_ = nullptr;
	Camera* defaultCamera_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState;

	// カメラリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	CameraForGPU* cameraData_ = nullptr;

	// 平行光源リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight* directionalLightData_ = nullptr;

	// 点光源リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
	PointLight* pointLightData_ = nullptr;

	// スポットライトリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
	SpotLight* spotLightData_ = nullptr;
};
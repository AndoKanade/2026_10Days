#pragma once
#include "DXCommon.h"
#include "MyMath.h" // Vector3, Vector4 等のため

struct DirectionalLight{
    Vector4 color;
    Vector3 direction;
    float intensity;
};

class ModelCommon{
public:
    void Initialize(DXCommon* dxCommon);
    DXCommon* GetDxCommon() const{ return dxCommon_; }
    ID3D12Resource* GetLightResource() const{ return lightResource.Get(); }
    DirectionalLight* GetLightData() const{ return lightData; }

private:
    DXCommon* dxCommon_;
    // ライト用のリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource;
    // Map用ポインタ
    DirectionalLight* lightData = nullptr;
};
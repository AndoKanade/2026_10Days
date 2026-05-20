#pragma once
#include "MyMath.h"
#include "Model.h"
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl.h>
#include <memory>

class Obj3dCommon;
class Model;
class Camera;

class Obj3D{
public:
	struct TransformationMatrix{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
	};

	void Initialize(Obj3dCommon* object3dCommon);
	void Update();
	void Draw();

	// Setter
	void SetParent(const std::weak_ptr<Obj3D>& parent);
	void SetModel(Model* model){ this->model = model; }
	void SetModel(const std::string& filePath);
	void SetCamera(Camera* camera){ this->camera = camera; }
	void SetTexture(const std::string& filePath){
		if(model){
			model->SetTexture(filePath);
		}
	}

	void SetScale(const Vector3& scale){ transform.scale = scale; }
	void SetRotate(const Vector3& rotate){ transform.rotate = rotate; isUseQuaternion_ = false; }	
	void SetTranslate(const Vector3& translate){ transform.translate = translate; }
	void SetQuaternion(const Quaternion& quat){ quaternion_ = quat; isUseQuaternion_ = true; }

	// Getter
	const Vector3& GetScale() const{ return transform.scale; }
	const Vector3& GetRotate() const{ return transform.rotate; }
	const Vector3& GetTranslate() const{ return transform.translate; }
	const Matrix4x4& GetWorldMatrix() const{ return transformationMatrixData->World; }
	Model::Material* GetMaterial() const{ return materialData; }

private:
	void CreateTransformationMatrixData();
	void CreateMaterialData();

	Obj3dCommon* object3dCommon = nullptr;
	Model* model = nullptr;
	Camera* camera = nullptr;
	std::weak_ptr<Obj3D> parent_;

	// 行列用
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	TransformationMatrix* transformationMatrixData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Model::Material* materialData = nullptr;

	Transform transform;
	Quaternion quaternion_ = {0.0f, 0.0f, 0.0f, 1.0f};
	bool isUseQuaternion_ = false;
};
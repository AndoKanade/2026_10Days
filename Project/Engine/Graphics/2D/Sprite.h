#pragma once
#include "MyMath.h"
#include <wrl.h>
#include <d3d12.h>
#include <string>

// 前方宣言
class SpriteCommon;

class Sprite{
public: // --- 公開する定義・関数 ---

	// 構造体定義（外部からも参照できるようにPublicのままにします）
	struct VertexData{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;

		// mapなどで使うための比較演算子
		bool operator<(const VertexData& other) const{
			if(position != other.position) return position < other.position;
			if(texcoord != other.texcoord) return texcoord < other.texcoord;
			if(normal != other.normal) return normal < other.normal;
			return false;
		}
	};

	struct Material{
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

	struct TransformationMatrix{
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	// --- メイン関数 ---
	void Initialize(SpriteCommon* spriteCommon,std::string textureFilePath);
	void Update();
	void Draw();

	// --- ゲッター / セッター ---
	const Vector2& GetPosition() const{ return position; }
	void SetPosition(const Vector2& position){ this->position = position; }

	float GetRotation() const{ return rotation; }
	void SetRotation(float rotation){ this->rotation = rotation; }

	const Vector4& GetColor() const{ return materialData->color; }
	void SetColor(const Vector4& color){ materialData->color = color; }
	// テクスチャ本来のRGBを無視し、透明度だけを文字・図形の形として着色する。
	// Spriteでは未使用だったenableLightingの領域をマスク切り替えに利用する。
	void SetUseAlphaMask(bool useAlphaMask){ materialData->enableLighting = useAlphaMask ? 1 : 0; }

	const Vector2& GetSize() const{ return size; }
	void SetSize(const Vector2& size){ this->size = size; }

	const Vector2& GetAnchorPoint() const{ return anchorPoint; }
	void SetAnchorPoint(const Vector2& anchorPoint){ this->anchorPoint = anchorPoint; }

	bool IsFlipX() const{ return isFlipX_; }
	void SetFlipX(bool isFlipX){ isFlipX_ = isFlipX; }

	bool IsFlipY() const{ return isFlipY_; }
	void SetFlipY(bool isFlipY){ isFlipY_ = isFlipY; }

	bool GetTextureLeftTop(Vector2* leftTop) const{
		if(!leftTop) return false;
		*leftTop = textureLeftTop;
		return true;
	}
	void SetTextureLeftTop(const Vector2& leftTop){
		textureLeftTop = leftTop;
	}

	bool GetTextureSize(Vector2* size) const{
		if(!size) return false;
		*size = textureSize;
		return true;
	}
	void SetTextureSize(const Vector2& size){
		textureSize = size;
	}

private: // --- 内部でのみ使用する関数・変数 ---

	// 初期化用ヘルパー関数
	void CreateVertexData();
	void CreateMaterialData();
	void CreateTransformationMatrixData();
	void AdjustTextureSize();

	// 借りてくるポインタ
	SpriteCommon* spriteCommon = nullptr;

	uint32_t textureIndex = 0;
	std::string textureFilePath_;

	// --- DirectXリソース (外部から触る必要がないのでPrivateへ) ---

	// 頂点関連
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	VertexData* vertexData = nullptr;

	// インデックス関連
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	uint32_t* indexData = nullptr;

	// マテリアル関連
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Material* materialData = nullptr;

	// 座標変換行列関連
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	TransformationMatrix* transformationMatrixData = nullptr;

	// --- 内部パラメータ ---

	// 行列計算用
	Matrix4x4 worldMatrix;
	Matrix4x4 viewMatrix;
	Matrix4x4 projectionMatrix;

	// トランスフォーム情報
	Vector2 position = {0.0f, 0.0f};
	float rotation = 0.0f;
	Vector2 size = {640.0f, 360.0f};

	// テクスチャ・見た目情報
	Vector2 anchorPoint = {0.0f, 0.0f};
	bool isFlipX_ = false;
	bool isFlipY_ = false;

	Vector2 textureLeftTop = {0.0f, 0.0f};
	Vector2 textureSize = {100.0f, 100.0f};
};

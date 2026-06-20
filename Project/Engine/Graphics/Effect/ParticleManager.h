#pragma once

// --- 標準ライブラリ ---
#include <list>
#include <random>
#include <map>
#include <unordered_map>
#include <string>
#include <memory>

// --- DirectX / Math ---
#include <d3d12.h>
#include <wrl.h>
#include "MyMath.h"

// --- エンジン内ヘッダー ---
#include "DXCommon.h"
#include "SrvManager.h"

// 前方宣言
class Camera;

/// <summary>
/// パーティクル1粒のデータ (CPU側計算用)
/// </summary>
struct Particle{
	Transform transform;  // 位置・回転・スケール
	Vector3 velocity;     // 速度
	Vector4 color;        // 色 (RGBA)
	float lifeTime;       // 生存可能時間
	float currentTime;    // 経過時間
};

/// <summary>
/// GPUへ送るインスタンシング用データ
/// </summary>
struct ParticleForGPU{
	Matrix4x4 WVP;   // World * View * Projection 行列
	Matrix4x4 World; // World 行列
	Vector4 color;   // 色
	Vector2 uvOffset; // UVオフセット (アニメーション用)
};

/// <summary>
/// パーティクルグループ
/// テクスチャごとに管理されるパーティクルの集合体
/// </summary>
struct ParticleGroup{
	// --- マテリアル情報 ---
	std::string textureFilePath;  // テクスチャのパス (識別キー)
	uint32_t textureSrvIndex;     // SRVのインデックス

	// --- パーティクル管理 ---
	std::list<Particle> particles; // 発生中のパーティクルリスト

	// --- インスタンシング描画用データ ---
	// グループごとの最大描画数
	static const uint32_t kNumMaxInstance = 1000;

	// インスタンシング用リソース (StructuredBuffer等)
	uint32_t instancingSrvIndex;
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;

	// マッピング済みポインタ (毎フレーム書き込む用)
	ParticleForGPU* instancingData = nullptr;

	// 現在描画すべきインスタンス数
	uint32_t numInstance = 0;

	bool isRing = false;
	bool isCylinder = false;
	bool isShockwave = false;
	bool isSpark = false;
	bool isSmoke = false;
	bool isCharge = false;
	bool isAura = false;
	bool isWarp = false;

};

/// <summary>
/// パーティクル管理マネージャー (シングルトン)
/// 複数のパーティクルグループを一括管理し、更新・描画を行います。
/// </summary>
class ParticleManager{
private:
	// シングルトンのためコンストラクタ等は隠蔽
	ParticleManager() = default;
	~ParticleManager() = default;

public:
	// コピー禁止
	ParticleManager(const ParticleManager&) = delete;
	ParticleManager& operator=(const ParticleManager&) = delete;

	// -------------------------------------------------
	// ライフサイクル
	// -------------------------------------------------

	/// <summary>
	/// インスタンス取得
	/// </summary>
	static ParticleManager* GetInstance();

	/// <summary>
	/// 初期化処理
	/// 共通リソース(パイプライン、モデル等)の生成を行います。
	/// </summary>
	/// <param name="dxCommon">DirectX共通クラス</param>
	/// <param name="srvManager">SRVマネージャー</param>
	void Initialize(DXCommon* dxCommon,SrvManager* srvManager);

	/// <summary>
	/// 終了処理
	/// リソースの解放を行います。
	/// </summary>
	void Finalize();

	/// <summary>
	/// 毎フレーム更新
	/// 全グループのパーティクルの寿命管理・移動処理を行います。
	/// </summary>
	/// <param name="camera">カメラ情報 (ビルボード計算等に使用)</param>
	void Update(Camera* camera);

	/// <summary>
	/// 描画処理
	/// 全グループのパーティクルを描画します。
	/// </summary>
	void Draw(const Matrix4x4& viewProjectionMatrix);

	// -------------------------------------------------
	// パーティクル操作
	// -------------------------------------------------

	/// <summary>
	/// パーティクルグループの作成
	/// 新しいテクスチャや種類のパーティクルを使う前に呼び出します。
	/// </summary>
	/// <param name="name">グループ名 (Emit時に使用)</param>
	/// <param name="textureFilePath">使用するテクスチャパス</param>
	void CreateParticleGroup(const std::string& name,const std::string& textureFilePath,bool isRing = false,bool isCylinder = false,bool isShockwave = false,bool isSpark = false,bool isSmoke = false,bool isCharge = false,bool isAura = false,bool isWarp = false);
	/// <summary>
	/// パーティクルの発生 (エミット)
	/// 指定したグループにパーティクルを追加します。
	/// </summary>
	/// <param name="name">登録済みのグループ名</param>
	/// <param name="position">発生位置</param>
	/// <param name="count">発生数</param>
	void Emit(const std::string& name,const Transform& emitterTransform,uint32_t count,const Vector4& color,const Vector3& velocity,float lifeTime);
private:
	// -------------------------------------------------
	// 内部処理・ヘルパー
	// -------------------------------------------------

	/// <summary>
	/// グラフィックスパイプラインの生成 (初期化時のみ)
	/// </summary>
	void CreateGraphicsPipeline();

	/// <summary>
	/// 共通モデル(板ポリゴン)の生成 (初期化時のみ)
	/// </summary>
	void CreateModel();

	void CreateRingModel();

	void CreateCylinderModel();

	/// <summary>
	/// 新規パーティクルデータの生成
	/// ランダムな速度や寿命を設定して返します。
	/// </summary>
	Particle MakeNewParticle(const Vector3& translate);

	// 頂点データの構造体 (内部で使用)
	struct VertexData{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// -------------------------------------------------
	// メンバ変数
	// -------------------------------------------------

	// --- 外部参照 (所有権なし) ---
	DXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// --- ユーティリティ ---
	std::mt19937 randomEngine_; // ランダム生成器

	// --- パーティクルデータ管理 ---
	// キー: グループ名, 値: パーティクルグループ(unique_ptr管理)
	std::unordered_map<std::string,std::unique_ptr<ParticleGroup>> particleGroups_;

	// --- 描画共通リソース (全グループで使い回す) ---
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// 板ポリゴン用の頂点バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_; // 一時バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;   // 実際のバッファ
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	//Ring用のメンバ変数
	Microsoft::WRL::ComPtr<ID3D12Resource> ringVertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW ringVertexBufferView_{};
	uint32_t ringVertexCount_ = 0;

	// Cylinder用のメンバ変数
	Microsoft::WRL::ComPtr<ID3D12Resource> cylinderVertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW cylinderVertexBufferView_{};
	uint32_t cylinderVertexCount_ = 0;
};
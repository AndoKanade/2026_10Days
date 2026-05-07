#pragma once
#include "MyMath.h"
#include <string>

/// <summary>
/// パーティクル発生装置 (エミッタ)
/// 指定された間隔で、特定の位置からパーティクルを自動発生させるヘルパークラスです。
/// </summary>
class ParticleEmitter{
public:
	/// <summary>
	/// コンストラクタ
	/// </summary>
	/// <param name="name">発生させるパーティクルグループ名 (Managerに登録済みの名前)</param>
	/// <param name="transform">発生位置や姿勢</param>
	/// <param name="count">1回の発生につき生成するパーティクル数</param>
	/// <param name="frequency">発生間隔 (秒) ※例: 0.5fなら0.5秒ごとに発生</param>
	ParticleEmitter(const std::string& name,const Transform& transform,uint32_t count,float frequency);

	/// <summary>
	/// 毎フレーム更新
	/// タイマーを進め、設定された間隔に達したら Emit() を呼び出します。
	/// </summary>
	void Update();

	/// <summary>
	/// 強制発生
	/// タイマーに関係なく、即座に設定数分のパーティクルを発生させます。
	/// </summary>
	void Emit();

	/// <summary>
	/// パーティクルの色を設定します。
	/// </summary>
	/// <param name="color">設定する色 (RGBA)</param>
	void SetColor(const Vector4& color){ color_ = color; }

	/// <summary>
	/// パーティクルの初速を設定します。
	/// </summary>
	/// <param name="velocity">設定する速度ベクトル</param>
	void SetVelocity(const Vector3& velocity){ velocity_ = velocity; }

	/// <summary>
	/// パーティクルの生存時間(寿命)を設定します。
	/// </summary>
	/// <param name="lifeTime">設定する生存時間 (秒)</param>
	void SetLifeTime(float lifeTime){ lifeTime_ = lifeTime; }

private:
	// 設定パラメータ
	std::string name_;       // パーティクルグループ名
	Transform transform_;    // 発生源の座標
	uint32_t count_;         // 1回の発生数
	float frequency_;        // 発生間隔 (秒)
	Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f};
	Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
	float lifeTime_ = 1.0f;

	// 内部制御用
	float timer_ = 0.0f;     // 経過時間計測タイマー
	const float kDeltaTime_ = 1.0f / 60.0f; // 時間刻み幅 (固定ステップ)
};
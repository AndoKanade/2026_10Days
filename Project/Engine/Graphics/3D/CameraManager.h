#pragma once
#include <map>
#include <string>
#include <memory>
#include "Camera.h"

/// <summary>
/// カメラ管理マネージャ (シングルトン)
/// 複数のカメラインスタンスを名前で管理し、アクティブなカメラを切り替えます。
/// </summary>
class CameraManager{
public:
	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	static CameraManager* GetInstance();

	/// <summary>
	/// 初期化処理
	/// </summary>
	void Initialize();

	/// <summary>
	/// 終了処理
	/// 登録された全カメラを破棄し、メモリを開放します。
	/// </summary>
	void Finalize();

	/// <summary>
	/// 毎フレーム更新処理
	/// 現在アクティブなカメラの行列更新などを行います。
	/// </summary>
	void Update();

	// --- カメラ操作関数 ---

	/// <summary>
	/// 新しいカメラを作成して登録します
	/// </summary>
	/// <param name="name">カメラ識別用の名前</param>
	void CreateCamera(const std::string& name,ID3D12Device* device);
	/// <summary>
	/// 使用するカメラを切り替えます
	/// </summary>
	/// <param name="name">切り替えたいカメラの名前</param>
	void SetActiveCamera(const std::string& name);

	/// <summary>
	/// 現在アクティブなカメラを取得します
	/// </summary>
	Camera* GetActiveCamera() const;

	/// <summary>
	/// 名前指定でカメラを取得します
	/// </summary>
	Camera* GetCamera(const std::string& name) const;

private:
	// シングルトンパターンのため、生成・コピーを禁止
	CameraManager() = default;
	~CameraManager() = default;
	CameraManager(const CameraManager&) = delete;
	CameraManager& operator=(const CameraManager&) = delete;

private:
	// カメラのコンテナ
	// mapが所有権(unique_ptr)を持つため、Manager破棄時に自動的にdeleteされます
	std::map<std::string,std::unique_ptr<Camera>> cameras_;

	// 現在使用中のカメラ
	// 所有権は持たず、cameras_の中身を参照するだけの「借り」ポインタ
	Camera* activeCamera_ = nullptr;
};
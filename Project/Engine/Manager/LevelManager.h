#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include "MyMath.h"

// JSONから読み込んだオブジェクトのデータを保持する構造体
struct LevelObjectData{
	std::string name;
	std::string type;
	std::string fileName;
	Vector3 translation;
	Vector3 rotation;
	Vector3 scaling;

	// コライダー用
	std::string colliderType;
	Vector3 colliderCenter;
	Vector3 colliderSize;
};

class LevelManager{
public:
	// JSONファイルのロード
	void LoadJSON(const std::string& filePath);

	// ホットリロード機能：ファイルの更新時刻を確認し、変更されていれば自動で再読み込みを行う
	// 戻り値：再読み込みを行った場合はtrueを返す
	bool CheckAndReload();

	// オブジェクトデータの取得
	const std::vector<LevelObjectData>& GetObjects() const{ return objects_; }

private:
	std::vector<LevelObjectData> objects_; // 読み込んだオブジェクトのリスト

	std::string filename_;  // ファイル名
	std::string directory_; // ディレクトリパス

	// ホットリロード用のメンバ変数
	std::filesystem::file_time_type lastWriteTime_{};       // 最後に読み込んだ時点でのファイル更新時刻
	std::chrono::steady_clock::time_point lastCheckTime_{}; // 最後にファイル更新確認を行った時刻（確認間隔の制御用）
};
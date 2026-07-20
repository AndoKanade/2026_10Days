#pragma once
#include <string>
#include <vector>
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

	// オブジェクトデータの取得
	const std::vector<LevelObjectData>& GetObjects() const{ return objects_; }

private:
	std::vector<LevelObjectData> objects_; // 読み込んだオブジェクトのリスト

	std::string filename_;  // ファイル名
	std::string directory_; // ディレクトリパス
};
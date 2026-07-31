#include "LevelManager.h"
#include "externals/json.hpp"
#include <fstream>
#include <iostream>
#include <cassert>

namespace{
	// resource/levelフォルダへの相対パス
	const std::string kLevelDirectory = "resource/level/";
	// ホットリロードのファイル更新確認を行う間隔
	constexpr std::chrono::milliseconds kReloadCheckInterval{500};
}

void LevelManager::LoadJSON(const std::string& fileName){
	// 再読み込み（ホットリロード）時にも同じファイルを参照できるようファイル名を保持
	filename_ = fileName;

	// resource/levelフォルダのパスを結合
	std::string fullPath = kLevelDirectory + fileName;

	// ファイルを開く
	std::ifstream file(fullPath);
	if(!file.is_open()){
		return;
	}

	// ホットリロード判定用に、読み込んだ時点でのファイル更新時刻を記録
	lastWriteTime_ = std::filesystem::last_write_time(fullPath);

	// 古いデータをクリア
	objects_.clear();

	// JSONデータの読み込み
	nlohmann::json deserialized;
	file >> deserialized;

	// レベルデータファイルの形式チェック
	assert(deserialized.is_object());
	assert(deserialized.contains("name"));
	assert(deserialized["name"].is_string());

	std::string name = deserialized["name"].get<std::string>();
	assert(name.compare("scene") == 0);

	// "objects"の全オブジェクトを走査
	for(nlohmann::json& object : deserialized["objects"]){
		assert(object.contains("type"));

		std::string type = object["type"].get<std::string>();

		// MESHオブジェクトの処理
		if(type.compare("MESH") == 0){
			objects_.emplace_back(LevelObjectData{});
			LevelObjectData& objectData = objects_.back();

			objectData.type = type;

			if(object.contains("name")){
				objectData.name = object["name"];
			}

			if(object.contains("file_name")){
				objectData.fileName = object["file_name"];
			}

			// ファイル名が空の場合のデフォルト設定
			//if(objectData.fileName.empty()){
			//	if(objectData.name == "Cube"){
			//		objectData.fileName = "Plane/plane.obj";
			//	} else if(objectData.name == "球"){
			//		objectData.fileName = "Sphere/sphere.obj";
			//	} else{
			//		objectData.fileName = "Plane/plane.obj";
			//	}
			//}

			// トランスフォームのパラメータ読み込み（Z-upからY-upへ変換）
			nlohmann::json& transform = object["transform"];

			objectData.translation.x = (float)transform["translation"][0];
			objectData.translation.y = (float)transform["translation"][2];
			objectData.translation.z = (float)transform["translation"][1];

			objectData.rotation.x = -(float)transform["rotation"][0];
			objectData.rotation.y = -(float)transform["rotation"][2];
			objectData.rotation.z = -(float)transform["rotation"][1];

			objectData.scaling.x = (float)transform["scaling"][0];
			objectData.scaling.y = (float)transform["scaling"][2];
			objectData.scaling.z = (float)transform["scaling"][1];

			// コライダーのパラメータ読み込み
			if(object.contains("collider")){
				nlohmann::json& collider = object["collider"];
				objectData.colliderType = collider["type"];

				objectData.colliderCenter.x = (float)collider["center"][0];
				objectData.colliderCenter.y = (float)collider["center"][2];
				objectData.colliderCenter.z = (float)collider["center"][1];

				objectData.colliderSize.x = (float)collider["size"][0];
				objectData.colliderSize.y = (float)collider["size"][2];
				objectData.colliderSize.z = (float)collider["size"][1];
			}
		}
		// 種類ごとの処理・再帰処理
	}
}

// ホットリロードの確認と実行
bool LevelManager::CheckAndReload(){
	// まだ一度もロードしていない場合は対象ファイルが無いので何もしない
	if(filename_.empty()){
		return false;
	}

	// 毎フレームファイルアクセスすると無駄が多いため一定間隔でのみ確認する
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	if(now - lastCheckTime_ < kReloadCheckInterval){
		return false;
	}
	lastCheckTime_ = now;

	std::string fullPath = kLevelDirectory + filename_;

	// ファイルが存在しない場合は何もしない
	if(!std::filesystem::exists(fullPath)){
		return false;
	}

	// 現在のファイル更新時刻を取得し、前回ロード時の更新時刻と比較する
	std::filesystem::file_time_type currentWriteTime = std::filesystem::last_write_time(fullPath);
	if(currentWriteTime == lastWriteTime_){
		// 更新されていないので再読み込み不要
		return false;
	}

	// 更新が検知されたので再読み込みする
	LoadJSON(filename_);
	return true;
}
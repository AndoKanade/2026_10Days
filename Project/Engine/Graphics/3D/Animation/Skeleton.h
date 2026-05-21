#pragma once

#include "MyMath.h"
#include "Animation.h"
#include <vector>
#include <string>
#include <optional>
#include <map>

// 階層構造を持つノード
struct Node{
	QuaternionTransform transform;
	Matrix4x4 localMatrix;
	std::string name;
	std::vector<Node> children;
};

// ジョイント
struct Joint{
	QuaternionTransform transform;
	Matrix4x4 localMatrix;
	Matrix4x4 skeletonSpaceMatrix;
	std::string name;
	std::vector<int32_t> children;
	int32_t index;
	std::optional<int32_t> parent;
};

// スケルトンクラス
class Skeleton{
public:
	// ノード階層からスケルトンを構築する
	void Create(const Node& rootNode);

	// スケルトンの姿勢を更新する
	void Update();

	// スケルトンのデバッグ描画を行う
	void DrawDebug(const Matrix4x4& worldMatrix);

	// アニメーションを適用する
	void ApplyAnimation(const Animation& animation,float animationTime);

	// メンバ変数
	int32_t root;
	std::map<std::string,int32_t> jointMap;
	std::vector<Joint> joints;

private:
	// ジョイントを再帰的に作成する
	int32_t CreateJoint(const Node& node,const std::optional<int32_t>& parent,std::vector<Joint>& joints);
};
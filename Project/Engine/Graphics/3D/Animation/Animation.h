#pragma once

#include "MyMath.h"
#include <vector>
#include <string>
#include <map>

// --------------------------------------------------
// アニメーション関連のデータ構造
// --------------------------------------------------

// キーフレーム構造体
template <typename tValue>
struct Keyframe{
	float time;
	tValue value;
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

// ノードアニメーション
struct NodeAnimation{
	std::vector<KeyframeVector3> translateKeyframes;
	std::vector<KeyframeQuaternion> rotateKeyframes;
	std::vector<KeyframeVector3> scaleKeyframes;
};

// アニメーションデータ全体
struct Animation{
	float duration; // アニメーション全体の尺
	std::map<std::string,NodeAnimation> nodeAnimations;
};

// --------------------------------------------------
// アニメーション制御クラス
// --------------------------------------------------
class AnimationController{
public:
	// 制御関数
	void Initialize();
	void Update(float deltaTime);
	void Play(){ isPlaying_ = true; }
	void Stop(){ isPlaying_ = false; }

	// アニメーションデータを更新してSRTを計算する
	void UpdateKeyframes(const Animation& animation,float deltaTime);

	// SRTのゲッター
	Vector3 GetCurrentScale() const{ return currentScale_; }
	Vector3 GetCurrentTranslate() const{ return currentTranslate_; }
	Quaternion GetCurrentRotate() const{ return currentRotate_; }

	// 補間計算関数
	static Vector3 CalculateInterpolatedScale(float time,const std::vector<KeyframeVector3>& keyframes);
	static Vector3 CalculateInterpolatedTranslate(float time,const std::vector<KeyframeVector3>& keyframes);
	static Quaternion CalculateInterpolatedRotate(float time,const std::vector<KeyframeQuaternion>& keyframes);

private:
	// ノード単位の補間計算関数
	NodeAnimation CalculateInterpolatedNode(const NodeAnimation& nodeAnim,float time);

	// メンバ変数
	std::vector<KeyframeVector3> scaleKeyframes_;
	std::vector<KeyframeVector3> translateKeyframes_;
	std::vector<KeyframeQuaternion> rotateKeyframes_;

	Vector3 currentScale_;
	Vector3 currentTranslate_;
	Quaternion currentRotate_;

	float currentTime_;
	float duration_;
	bool isPlaying_;
	float animationTime_ = 0.0f;
};

// --------------------------------------------------
// 外部関数
// --------------------------------------------------

// アニメーションファイルの読み込み
Animation LoadAnimationFile(const std::string& directoryPath,const std::string& filename);
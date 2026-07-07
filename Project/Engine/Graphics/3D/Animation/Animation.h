#pragma once

#include "MyMath.h"
#include <vector>
#include <string>
#include <map>

// 構造体定義
template <typename tValue>
struct Keyframe{
	float time;
	tValue value;
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

struct NodeAnimation{
	std::vector<KeyframeVector3> translateKeyframes;
	std::vector<KeyframeQuaternion> rotateKeyframes;
	std::vector<KeyframeVector3> scaleKeyframes;
};

struct Animation{
	float duration;
	std::map<std::string,NodeAnimation> nodeAnimations;
};

// アニメーション制御クラス
class AnimationController{
public:
	// 制御関数
	void Initialize();
	void Update(float deltaTime);
	void Play(){ isPlaying_ = true; }
	void Stop(){ isPlaying_ = false; }
	void UpdateKeyframes(const Animation& animation,float deltaTime);

	// ゲッター
	Vector3 GetCurrentScale() const{ return currentScale_; }
	Vector3 GetCurrentTranslate() const{ return currentTranslate_; }
	Quaternion GetCurrentRotate() const{ return currentRotate_; }

	// 補間計算関数
	static NodeAnimation GetInterpolatedNode(const Animation& animation,const std::string& nodeName,float time);
	static Vector3 CalculateInterpolatedScale(float time,const std::vector<KeyframeVector3>& keyframes);
	static Vector3 CalculateInterpolatedTranslate(float time,const std::vector<KeyframeVector3>& keyframes);
	static Quaternion CalculateInterpolatedRotate(float time,const std::vector<KeyframeQuaternion>& keyframes);
	static NodeAnimation CalculateInterpolatedNode(const NodeAnimation& nodeAnim,float time);

private:
	// キーフレームデータ
	std::vector<KeyframeVector3> scaleKeyframes_;
	std::vector<KeyframeVector3> translateKeyframes_;
	std::vector<KeyframeQuaternion> rotateKeyframes_;

	// 現在の状態
	Vector3 currentScale_;
	Vector3 currentTranslate_;
	Quaternion currentRotate_;

	// 再生情報
	float currentTime_;
	float duration_;
	bool isPlaying_;
	float animationTime_ = 0.0f;
};

// 外部関数
Animation LoadAnimationFile(const std::string& directoryPath,const std::string& filename);
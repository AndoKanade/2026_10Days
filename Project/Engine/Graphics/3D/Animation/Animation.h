#pragma once
#include "MyMath.h"
#include<vector>

template <typename tValue>
struct Keyframe{
	float time;
	tValue value;
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

class Animation{
public:
	void Initialize();
	void Update(float deltaTime);

	void Play(){ isPlaying_ = true; }
	void Stop(){ isPlaying_ = false; }

	void CreateTestAnimation();

	Vector3 GetCurrentScale() const{ return currentScale_; }
	Vector3 GetCurrentTranslate() const{ return currentTranslate_; }
	Quaternion GetCurrentRotate() const{ return currentRotate_; }

private:
	Vector3 CalculateInterpolatedScale(float time);
	Vector3 CalculateInterpolatedTranslate(float time);
	Quaternion CalculateInterpolatedRotate(float time);

private:
	std::vector<KeyframeVector3> scaleKeyframes_;
	std::vector<KeyframeVector3> translateKeyframes_;
	std::vector<KeyframeQuaternion> rotateKeyframes_;

	float duration_ = 0.0f;
	float currentTime_ = 0.0f;
	bool isPlaying_ = false;

	Vector3 currentScale_ = {1.0f, 1.0f, 1.0f};
	Vector3 currentTranslate_ = {0.0f, 0.0f, 0.0f};
	Quaternion currentRotate_ = {0.0f, 0.0f, 0.0f, 1.0f};
};
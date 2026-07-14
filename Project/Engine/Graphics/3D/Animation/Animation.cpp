#include "Animation.h"
#include "Obj3DCommon.h"
#include "MyMath.h"
#include <filesystem>
#include <cmath>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>

// アニメーション制御 (AnimationController)

void AnimationController::Initialize(){
	isPlaying_ = false;
	currentTime_ = 0.0f;
	duration_ = 0.0f;
}

void AnimationController::UpdateKeyframes(const Animation& animation,float deltaTime){
	if(!isPlaying_){
		return;
	}

	// アニメーションの再生時間を進める
	if(animation.duration > 0.0f){
		animationTime_ += deltaTime;
		animationTime_ = std::fmod(animationTime_,animation.duration);
	}

	// 初期値を設定
	currentScale_ = {1.0f, 1.0f, 1.0f};
	currentRotate_ = {0.0f, 0.0f, 0.0f, 1.0f};
	currentTranslate_ = {0.0f, 0.0f, 0.0f};

	// gltfから読み込んだノードのアニメーションを解析
	if(!animation.nodeAnimations.empty()){
		auto it = animation.nodeAnimations.begin();
		NodeAnimation interpolated = CalculateInterpolatedNode(it->second,animationTime_);

		if(!interpolated.scaleKeyframes.empty()){
			currentScale_ = interpolated.scaleKeyframes.front().value;
		}
		if(!interpolated.rotateKeyframes.empty()){
			currentRotate_ = interpolated.rotateKeyframes.front().value;
		}
		if(!interpolated.translateKeyframes.empty()){
			currentTranslate_ = interpolated.translateKeyframes.front().value;
		}
	}
}

NodeAnimation AnimationController::GetInterpolatedNode(const Animation& animation,const std::string& nodeName,float time){
	if(animation.nodeAnimations.find(nodeName) != animation.nodeAnimations.end()){
		return CalculateInterpolatedNode(animation.nodeAnimations.at(nodeName),time);
	}
	return {};
}

// 補間計算関数群

Vector3 AnimationController::CalculateInterpolatedTranslate(float time,const std::vector<KeyframeVector3>& keyframes){
	if(keyframes.empty()) return {0.0f, 0.0f, 0.0f};
	if(keyframes.size() == 1 || time <= keyframes.front().time) return keyframes.front().value;
	if(time >= keyframes.back().time) return keyframes.back().value;

	for(size_t i = 0; i < keyframes.size() - 1; ++i){
		const auto& key0 = keyframes[i];
		const auto& key1 = keyframes[i + 1];

		if(time >= key0.time && time <= key1.time){
			float t = (time - key0.time) / (key1.time - key0.time);
			return Lerp(key0.value,key1.value,t);
		}
	}
	return {0.0f, 0.0f, 0.0f};
}

Vector3 AnimationController::CalculateInterpolatedScale(float time,const std::vector<KeyframeVector3>& keyframes){
	if(keyframes.empty()) return {1.0f, 1.0f, 1.0f};
	if(keyframes.size() == 1 || time <= keyframes.front().time) return keyframes.front().value;
	if(time >= keyframes.back().time) return keyframes.back().value;

	for(size_t i = 0; i < keyframes.size() - 1; ++i){
		const auto& key0 = keyframes[i];
		const auto& key1 = keyframes[i + 1];

		if(time >= key0.time && time <= key1.time){
			float t = (time - key0.time) / (key1.time - key0.time);
			return Lerp(key0.value,key1.value,t);
		}
	}
	return {1.0f, 1.0f, 1.0f};
}

Quaternion AnimationController::CalculateInterpolatedRotate(float time,const std::vector<KeyframeQuaternion>& keyframes){
	if(keyframes.empty()) return {0.0f, 0.0f, 0.0f, 1.0f};
	if(keyframes.size() == 1 || time <= keyframes.front().time) return keyframes.front().value;
	if(time >= keyframes.back().time) return keyframes.back().value;

	for(size_t i = 0; i < keyframes.size() - 1; ++i){
		const auto& key0 = keyframes[i];
		const auto& key1 = keyframes[i + 1];

		if(time >= key0.time && time <= key1.time){
			// 同じ時間のキーフレームが存在した場合のゼロ除算を防ぐ
			float timeDiff = key1.time - key0.time;
			if(timeDiff <= 0.0f){
				return key0.value;
			}
			float t = (time - key0.time) / timeDiff;

			float dot = Dot(key0.value,key1.value);
			Quaternion targetQuat = key1.value;

			if(dot < 0.0f){
				targetQuat = -key1.value;
				dot = -dot;
			}

			if(dot >= 1.0f - 0.0005f){
				Quaternion result = (1.0f - t) * key0.value + t * targetQuat;
				return Normalize(result);
			}

			float theta = std::acos(dot);
			float sinTheta = std::sin(theta);
			float scale0 = std::sin((1.0f - t) * theta) / sinTheta;
			float scale1 = std::sin(t * theta) / sinTheta;

			Quaternion result = scale0 * key0.value + scale1 * targetQuat;
			return Normalize(result);
		}
	}
	return {0.0f, 0.0f, 0.0f, 1.0f};
}

NodeAnimation AnimationController::CalculateInterpolatedNode(const NodeAnimation& nodeAnim,float time){
	NodeAnimation result;

	// Scale の補間
	if(!nodeAnim.scaleKeyframes.empty()){
		Vector3 s = CalculateInterpolatedScale(time,nodeAnim.scaleKeyframes);
		result.scaleKeyframes.push_back(KeyframeVector3{time, s});
	}

	// Translate の補間
	if(!nodeAnim.translateKeyframes.empty()){
		Vector3 tr = CalculateInterpolatedTranslate(time,nodeAnim.translateKeyframes);
		result.translateKeyframes.push_back(KeyframeVector3{time, tr});
	}

	// Rotate の補間
	if(!nodeAnim.rotateKeyframes.empty()){
		Quaternion r = CalculateInterpolatedRotate(time,nodeAnim.rotateKeyframes);
		result.rotateKeyframes.push_back(KeyframeQuaternion{time, r});
	}

	return result;
}

// ファイル読み込み処理 (Assimp)

Animation LoadAnimationFile(const std::string& directoryPath,const std::string& filename){
	Animation animation;
	Assimp::Importer importer;
	std::filesystem::path path(directoryPath);
	path /= filename;

	const aiScene* scene = importer.ReadFile(path.string().c_str(),0);
	assert(scene != nullptr && scene->HasAnimations());

	aiAnimation* animationAssimp = scene->mAnimations[0];

	// ticksPerSecond が 0 の場合のゼロ除算（NaN発生）を完全に防ぐ
	double ticksPerSecond = animationAssimp->mTicksPerSecond != 0.0?animationAssimp->mTicksPerSecond:25.0;
	animation.duration = static_cast<float>(animationAssimp->mDuration / ticksPerSecond);

	for(uint32_t i = 0; i < animationAssimp->mNumChannels; ++i){
		aiNodeAnim* nodeAnimAssimp = animationAssimp->mChannels[i];
		NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimAssimp->mNodeName.C_Str()];

		// Translate
		for(uint32_t j = 0; j < nodeAnimAssimp->mNumPositionKeys; ++j){
			aiVectorKey& key = nodeAnimAssimp->mPositionKeys[j];
			KeyframeVector3 keyframe;
			keyframe.time = static_cast<float>(key.mTime / ticksPerSecond);

			// MyMath の AssimpToMatrix (X軸反転) に合わせるため、Xをマイナスにする
			Vector3 value = {-key.mValue.x, key.mValue.y, key.mValue.z};

			keyframe.value = value;
			nodeAnimation.translateKeyframes.push_back(keyframe);
		}

		// Rotate
		for(uint32_t j = 0; j < nodeAnimAssimp->mNumRotationKeys; ++j){
			aiQuatKey& key = nodeAnimAssimp->mRotationKeys[j];
			KeyframeQuaternion keyframe;
			keyframe.time = static_cast<float>(key.mTime / ticksPerSecond);

			// 回転(クォータニオン)もX軸反転に対応させる（YとZをマイナスにする）
			Quaternion rotate = {key.mValue.x, -key.mValue.y, -key.mValue.z, key.mValue.w};

			keyframe.value = rotate;
			nodeAnimation.rotateKeyframes.push_back(keyframe);
		}

		// Scale
		for(uint32_t j = 0; j < nodeAnimAssimp->mNumScalingKeys; ++j){
			aiVectorKey& key = nodeAnimAssimp->mScalingKeys[j];
			KeyframeVector3 keyframe;
			keyframe.time = static_cast<float>(key.mTime / ticksPerSecond);

			// スケールは反転させなくてOK
			Vector3 value = {key.mValue.x, key.mValue.y, key.mValue.z};

			keyframe.value = value;
			nodeAnimation.scaleKeyframes.push_back(keyframe);
		}
	}
	return animation;
}
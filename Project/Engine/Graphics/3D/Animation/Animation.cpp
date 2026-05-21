#include "Animation.h"
#include "MyMath.h"
#include <cmath>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>

// ====================================================================
// アニメーション制御 (AnimationController)
// ====================================================================

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

// ====================================================================
// 補間計算関数群
// ====================================================================

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
			float t = (time - key0.time) / (key1.time - key0.time);
			float dot = Dot(key0.value,key1.value);
			Quaternion targetQuat = key1.value;

			if(dot < 0.0f){
				targetQuat = -key1.value;
				dot = -dot;
			}

			if(dot >= 1.0f - 0.0005f){
				return (1.0f - t) * key0.value + t * targetQuat;
			}

			float theta = std::acos(dot);
			float sinTheta = std::sin(theta);
			float scale0 = std::sin((1.0f - t) * theta) / sinTheta;
			float scale1 = std::sin(t * theta) / sinTheta;

			return scale0 * key0.value + scale1 * targetQuat;
		}
	}
	return {0.0f, 0.0f, 0.0f, 1.0f};
}

NodeAnimation AnimationController::CalculateInterpolatedNode(const NodeAnimation& nodeAnim,float time){
	NodeAnimation result;

	// Scale の補間
	if(!nodeAnim.scaleKeyframes.empty()){
		if(nodeAnim.scaleKeyframes.size() == 1 || time <= nodeAnim.scaleKeyframes.front().time){
			result.scaleKeyframes.push_back(nodeAnim.scaleKeyframes.front());
		} else if(time >= nodeAnim.scaleKeyframes.back().time){
			result.scaleKeyframes.push_back(nodeAnim.scaleKeyframes.back());
		} else{
			for(size_t i = 0; i < nodeAnim.scaleKeyframes.size() - 1; ++i){
				if(time >= nodeAnim.scaleKeyframes[i].time && time <= nodeAnim.scaleKeyframes[i + 1].time){
					float t = (time - nodeAnim.scaleKeyframes[i].time) / (nodeAnim.scaleKeyframes[i + 1].time - nodeAnim.scaleKeyframes[i].time);
					Vector3 s = Lerp(nodeAnim.scaleKeyframes[i].value,nodeAnim.scaleKeyframes[i + 1].value,t);
					result.scaleKeyframes.push_back(KeyframeVector3{time, s});
					break;
				}
			}
		}
	}

	// Translate の補間
	if(!nodeAnim.translateKeyframes.empty()){
		if(nodeAnim.translateKeyframes.size() == 1 || time <= nodeAnim.translateKeyframes.front().time){
			result.translateKeyframes.push_back(nodeAnim.translateKeyframes.front());
		} else if(time >= nodeAnim.translateKeyframes.back().time){
			result.translateKeyframes.push_back(nodeAnim.translateKeyframes.back());
		} else{
			for(size_t i = 0; i < nodeAnim.translateKeyframes.size() - 1; ++i){
				if(time >= nodeAnim.translateKeyframes[i].time && time <= nodeAnim.translateKeyframes[i + 1].time){
					float t = (time - nodeAnim.translateKeyframes[i].time) / (nodeAnim.translateKeyframes[i + 1].time - nodeAnim.translateKeyframes[i].time);
					Vector3 tr = Lerp(nodeAnim.translateKeyframes[i].value,nodeAnim.translateKeyframes[i + 1].value,t);
					result.translateKeyframes.push_back(KeyframeVector3{time, tr});
					break;
				}
			}
		}
	}

	// Rotate の補間
	if(!nodeAnim.rotateKeyframes.empty()){
		if(nodeAnim.rotateKeyframes.size() == 1 || time <= nodeAnim.rotateKeyframes.front().time){
			result.rotateKeyframes.push_back(nodeAnim.rotateKeyframes.front());
		} else if(time >= nodeAnim.rotateKeyframes.back().time){
			result.rotateKeyframes.push_back(nodeAnim.rotateKeyframes.back());
		} else{
			for(size_t i = 0; i < nodeAnim.rotateKeyframes.size() - 1; ++i){
				if(time >= nodeAnim.rotateKeyframes[i].time && time <= nodeAnim.rotateKeyframes[i + 1].time){
					float t = (time - nodeAnim.rotateKeyframes[i].time) / (nodeAnim.rotateKeyframes[i + 1].time - nodeAnim.rotateKeyframes[i].time);
					float dot = Dot(nodeAnim.rotateKeyframes[i].value,nodeAnim.rotateKeyframes[i + 1].value);
					Quaternion targetQuat = nodeAnim.rotateKeyframes[i + 1].value;

					if(dot < 0.0f){
						targetQuat = -nodeAnim.rotateKeyframes[i + 1].value;
						dot = -dot;
					}

					Quaternion r;
					if(dot >= 1.0f - 0.0005f){
						r = (1.0f - t) * nodeAnim.rotateKeyframes[i].value + t * targetQuat;
					} else{
						float theta = std::acos(dot);
						float sinTheta = std::sin(theta);
						float scale0 = std::sin((1.0f - t) * theta) / sinTheta;
						float scale1 = std::sin(t * theta) / sinTheta;
						r = scale0 * nodeAnim.rotateKeyframes[i].value + scale1 * targetQuat;
					}

					result.rotateKeyframes.push_back(KeyframeQuaternion{time, r});
					break;
				}
			}
		}
	}

	return result;
}

// ====================================================================
// ファイル読み込み処理 (Assimp)
// ====================================================================

Animation LoadAnimationFile(const std::string& directoryPath,const std::string& filename){
	Animation animation;
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(),0);

	assert(scene->mNumAnimations != 0);

	aiAnimation* animationAssimp = scene->mAnimations[0];
	animation.duration = float(animationAssimp->mDuration / animationAssimp->mTicksPerSecond);

	for(uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex){
		aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
		NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];

		for(uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex){
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
			KeyframeVector3 keyframe;
			keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
			keyframe.value = {-keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
			nodeAnimation.translateKeyframes.push_back(keyframe);
		}

		for(uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex){
			aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
			KeyframeQuaternion keyframe;
			keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
			keyframe.value = {keyAssimp.mValue.x, -keyAssimp.mValue.y, -keyAssimp.mValue.z, keyAssimp.mValue.w};
			nodeAnimation.rotateKeyframes.push_back(keyframe);
		}

		for(uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex){
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
			KeyframeVector3 keyframe;
			keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
			keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
			nodeAnimation.scaleKeyframes.push_back(keyframe);
		}
	}

	return animation;
}
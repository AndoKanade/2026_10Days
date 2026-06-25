#include "Skeleton.h"
#include "Logger.h"

// ====================================================================
// 初期化・構築処理
// ====================================================================
void Skeleton::Create(const Node& rootNode){
	joints.clear();
	jointMap.clear();

	// ルートから順にジョイントを構築
	root = CreateJoint(rootNode,std::nullopt,joints);

	// 構築されたジョイントを使って辞書を作成
	for(const Joint& joint : joints){
		jointMap.emplace(joint.name,joint.index);
	}

	// アニメーション前の初期姿勢行列を計算
	Update();
}

int32_t Skeleton::CreateJoint(const Node& node,const std::optional<int32_t>& parent,std::vector<Joint>& joints){
	Joint joint;
	joint.name = node.name;
	joint.localMatrix = node.localMatrix;
	joint.skeletonSpaceMatrix = MakeIdentity4x4();
	joint.transform = node.transform;
	joint.parent = parent;
	joint.index = static_cast<int32_t>(joints.size());

	joints.push_back(joint);

	// 子ノードがあれば再帰的にジョイントを作成
	for(const Node& childNode : node.children){
		int32_t childIndex = CreateJoint(childNode,joint.index,joints);
		joints[joint.index].children.push_back(childIndex);
	}

	return joint.index;
}

// ====================================================================
// 更新処理
// ====================================================================
void Skeleton::Update(){
	UpdateJointRecursive(root,MakeIdentity4x4());
}

void Skeleton::ApplyAnimation(const Animation& animation,float animationTime){
	for(Joint& joint : joints){
		if(auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end()){
			const NodeAnimation& rootNodeAnimation = (*it).second;

			if(!rootNodeAnimation.translateKeyframes.empty()){
				joint.transform.translate = AnimationController::CalculateInterpolatedTranslate(animationTime,rootNodeAnimation.translateKeyframes);
			}
			if(!rootNodeAnimation.rotateKeyframes.empty()){
				joint.transform.rotate = AnimationController::CalculateInterpolatedRotate(animationTime,rootNodeAnimation.rotateKeyframes);
			}
			if(!rootNodeAnimation.scaleKeyframes.empty()){
				joint.transform.scale = AnimationController::CalculateInterpolatedScale(animationTime,rootNodeAnimation.scaleKeyframes);
			}
		}
	}
}

void Skeleton::UpdateJointRecursive(int32_t jointIdx,const Matrix4x4& parentMatrix){
	Joint& joint = joints[jointIdx];

	// ローカル行列を再計算
	Matrix4x4 matRotate = MakeRotateQuaternionMatrix(joint.transform.rotate);
	joint.localMatrix = MakeAffineMatrix(joint.transform.scale,matRotate,joint.transform.translate);

	// 親行列 × ローカル行列 ＝ スケルトン空間行列
	joint.skeletonSpaceMatrix = Multiply(parentMatrix,joint.localMatrix);

	// 子を再帰的に処理
	for(int32_t childIdx:joint.children){
		UpdateJointRecursive(childIdx,joint.skeletonSpaceMatrix);
	}
}


// ====================================================================
// 描画処理
// ====================================================================
void Skeleton::DrawDebug(const Matrix4x4& worldMatrix){
	for(const Joint& joint : joints){
		// ジョイントの最終的なワールド行列を計算
		Matrix4x4 jointWorldMatrix = Multiply(joint.skeletonSpaceMatrix,worldMatrix);

		// 行列からワールド座標（平行移動成分）を抜き出す
		Vector3 jointPos = {
			jointWorldMatrix.m[3][0],
			jointWorldMatrix.m[3][1],
			jointWorldMatrix.m[3][2]
		};

		// 親がいる場合は、親の位置から自身の位置へ線を引くための座標計算
		if(joint.parent){
			Matrix4x4 parentWorldMatrix = Multiply(joints[*joint.parent].skeletonSpaceMatrix,worldMatrix);

			Vector3 parentPos = {
				parentWorldMatrix.m[3][0],
				parentWorldMatrix.m[3][1],
				parentWorldMatrix.m[3][2]
			};
		}
	}
}
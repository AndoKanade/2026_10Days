#include "Skeleton.h"
#include "Logger.h"

// 初期化・構築処理

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
	joint.bindPoseTransform = joint.transform;

	joints.push_back(joint);

	// 子ノードがあれば再帰的にジョイントを作成
	for(const Node& childNode : node.children){
		int32_t childIndex = CreateJoint(childNode,joint.index,joints);
		joints[joint.index].children.push_back(childIndex);
	}

	return joint.index;
}

// 更新処理

void Skeleton::Update(){
	// 全てのジョイントをループする
	for(size_t i = 0; i < joints.size(); ++i){
		Joint& joint = joints[i];

		// ローカル行列の計算
		Matrix4x4 matScale = MakeScaleMatrix(joint.transform.scale);
		Matrix4x4 matRotate = MakeRotateQuaternionMatrix(joint.transform.rotate);
		Matrix4x4 matTranslate = MakeTranslateMatrix(joint.transform.translate);

		joint.localMatrix = Multiply(Multiply(matScale,matRotate),matTranslate);

		// スケルトン空間行列（ワールド行列）の計算
		if(joint.parent.has_value()){
			// 親がいる場合: 自分のローカル × 親のスケルトン空間行列
			joint.skeletonSpaceMatrix = Multiply(joint.localMatrix,joints[*joint.parent].skeletonSpaceMatrix);
		} else{
			// 親がいなければローカルがそのまま
			joint.skeletonSpaceMatrix = joint.localMatrix;
		}
	}
}

void Skeleton::ApplyAnimation(const Animation& animation,float animationTime){
	for(auto& joint : joints){
		// そのジョイントの現在の時間における補間済みアニメーションデータを取得
		NodeAnimation nodeAnim = AnimationController::GetInterpolatedNode(animation,joint.name,animationTime);

		// 補間計算済みのデータを使う
		if(!nodeAnim.rotateKeyframes.empty()){
			joint.transform.rotate = nodeAnim.rotateKeyframes[0].value;
		}
		if(!nodeAnim.translateKeyframes.empty()){
			joint.transform.translate = nodeAnim.translateKeyframes[0].value;
		}
		if(!nodeAnim.scaleKeyframes.empty()){
			joint.transform.scale = nodeAnim.scaleKeyframes[0].value;
		}

		// ローカル行列の再計算
		joint.localMatrix = MakeAffineMatrixQuaternion(joint.transform.scale,joint.transform.rotate,joint.transform.translate);
	}
}
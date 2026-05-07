#include "ParticleEmitter.h"
#include "ParticleManager.h"

/// <summary>
/// コンストラクタ
/// エミッタの基本パラメータ（名前、位置、発生数、頻度）を設定します。
/// </summary>
ParticleEmitter::ParticleEmitter(const std::string& name,const Transform& transform,uint32_t count,float frequency)
	: name_(name),transform_(transform),count_(count),frequency_(frequency){}

/// <summary>
/// パーティクル発生処理
/// 自身が持つ設定値（座標や発生数）を使って、Managerに発生をリクエストします。
/// </summary>
void ParticleEmitter::Emit(){
	ParticleManager::GetInstance()->Emit(name_,transform_,count_,color_,velocity_,lifeTime_);
}

/// <summary>
/// 毎フレームの更新処理
/// 時間経過を計測し、設定された頻度（frequency_）ごとにパーティクルを発生させます。
/// </summary>
void ParticleEmitter::Update(){
	// 経過時間を加算
	timer_ += kDeltaTime_;

	// 蓄積された時間が「発生間隔」を超えている場合、発生処理を行う
	// (whileループにすることで、処理落ち等で時間が飛びすぎた場合でも回数分まとめて発生させる)
	while(timer_ >= frequency_){

		// 発生実行
		this->Emit();

		// タイマーから1回分の時間を消費
		timer_ -= frequency_;
	}
}
#include "SpecialGauge.h"

#include <algorithm>

#include "PuzzleConfig.h"

// 追加：消去結果からスペシャルゲージを加算する。
void SpecialGauge::AddFromClear(int32_t clearedCellCount,int32_t chainCount){
	// スペシャル発動中は追加チャージを行わない
	if(clearedCellCount <= 0 || isActivationActive_){
		return;
	}

	const int32_t additionalChainCount = std::max(chainCount - 1,0);
	const int32_t gainedValue =
		clearedCellCount * PuzzleConfig::kSpecialGaugePerClearedCell +
		additionalChainCount * PuzzleConfig::kSpecialGaugeBonusPerAdditionalChain;

	value_ = std::min(value_ + gainedValue,PuzzleConfig::kSpecialGaugeMax);
}

// スペシャル発動後の制限時間を1フレーム分進める
void SpecialGauge::Update(){
	if(!isActivationActive_){
		return;
	}

	++drainTimer_;
	if(drainTimer_ < PuzzleConfig::kSpecialGaugeDrainIntervalFrames){
		return;
	}

	drainTimer_ = 0;
	value_ = std::max(value_ - 1,0);
	if(value_ == 0){
		isActivationActive_ = false;
	}
}

//	ゲージが満タンでスペシャルを開始可能か
bool SpecialGauge::CanActivate() const{
	return !isActivationActive_ && value_ >= PuzzleConfig::kSpecialGaugeMax;
}

//	満タンのゲージでスペシャルを開始し、減少を始める
bool SpecialGauge::StartActivation(){
	if(!CanActivate()){
		return false;
	}

	drainTimer_ = 0;
	isActivationActive_ = true;
	return true;
}

//	発動可能ならゲージを消費して true を返す
bool SpecialGauge::Consume(){
	if(!isActivationActive_ || value_ <= 0){
		return false;
	}

	Reset();
	return true;
}

//	デバッグ操作用
void SpecialGauge::Fill(){
	value_ = PuzzleConfig::kSpecialGaugeMax;
	drainTimer_ = 0;
	isActivationActive_ = false;
}

//	デバッグ操作用
void SpecialGauge::Reset(){
	value_ = 0;
	drainTimer_ = 0;
	isActivationActive_ = false;
}

int32_t SpecialGauge::GetMaxValue() const{
	return PuzzleConfig::kSpecialGaugeMax;
}

float SpecialGauge::GetRatio() const{
	return static_cast<float>(value_) / static_cast<float>(PuzzleConfig::kSpecialGaugeMax);
}

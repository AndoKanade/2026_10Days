#include "SpecialGauge.h"

#include <algorithm>

#include "PuzzleConfig.h"

void SpecialGauge::AddFromClear(int32_t clearedCellCount,int32_t chainCount){
	if(clearedCellCount <= 0){
		return;
	}

	const int32_t additionalChainCount = std::max(chainCount - 1,0);
	const int32_t gainedValue =
		clearedCellCount * PuzzleConfig::kSpecialGaugePerClearedCell +
		additionalChainCount * PuzzleConfig::kSpecialGaugeBonusPerAdditionalChain;

	value_ = std::min(value_ + gainedValue,PuzzleConfig::kSpecialGaugeMax);
}

bool SpecialGauge::CanActivate() const{
	return value_ >= PuzzleConfig::kSpecialGaugeMax;
}

bool SpecialGauge::Consume(){
	if(!CanActivate()){
		return false;
	}

	value_ = 0;
	return true;
}

void SpecialGauge::Fill(){
	value_ = PuzzleConfig::kSpecialGaugeMax;
}

void SpecialGauge::Reset(){
	value_ = 0;
}

int32_t SpecialGauge::GetMaxValue() const{
	return PuzzleConfig::kSpecialGaugeMax;
}

float SpecialGauge::GetRatio() const{
	return static_cast<float>(value_) / static_cast<float>(PuzzleConfig::kSpecialGaugeMax);
}

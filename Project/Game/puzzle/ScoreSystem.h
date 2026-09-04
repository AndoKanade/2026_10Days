#pragma once
#include <cstdint>

// 消去完了イベント1回ごとに加算する。ゲージとは独立したスコア計算。
class ScoreSystem{
public:
	static constexpr int64_t kBasePointsPerCell = 100;
	// 倍率は10倍の整数で保持し、小数の誤差を避ける。
	static constexpr int64_t CellMultiplierTenths(int32_t cells){
		return cells <= 4 ? 10 : 11 + (static_cast<int64_t>(cells) - 5) / 3;
	}
	static constexpr int64_t ChainMultiplierTenths(int32_t chain){
		return chain <= 1 ? 10 : 10 + (static_cast<int64_t>(chain) - 1) * 3;
	}
	static constexpr int64_t Calculate(int32_t cells,int32_t chain){
		if(cells <= 0){ return 0; }
		// 基本点を変更して端数が出た場合は、最後に小数点以下を切り捨てる。
		return static_cast<int64_t>(cells) * kBasePointsPerCell *
			CellMultiplierTenths(cells) * ChainMultiplierTenths(chain) / 100;
	}
	void AddFromClear(int32_t cells,int32_t chain){
		if(cells <= 0){ return; }
		lastCells_ = cells;
		lastChain_ = chain > 0 ? chain : 1;
		lastGain_ = Calculate(cells,lastChain_);
		total_ += lastGain_;
		totalCells_ += cells;
	}
	void Reset(){ total_ = 0; totalCells_ = 0; lastGain_ = 0; lastCells_ = 0; lastChain_ = 1; }
	int64_t GetTotalCells() const{ return totalCells_; }
	int64_t GetTotal() const{ return total_; }
	int64_t GetLastGain() const{ return lastGain_; }
	int32_t GetLastCells() const{ return lastCells_; }
	int32_t GetLastChain() const{ return lastChain_; }
private:
	int64_t total_ = 0;
	int64_t totalCells_ = 0;
	int64_t lastGain_ = 0;
	int32_t lastCells_ = 0;
	int32_t lastChain_ = 1;
};

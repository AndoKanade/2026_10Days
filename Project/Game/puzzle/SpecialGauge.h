#pragma once

#include <cstdint>

// =============================================================================
// スペシャル発動に使用するゲージ
// =============================================================================
class SpecialGauge{
public:
	// 通電による消去結果をゲージへ加算する。
	// chainCount は最初の消去を1とし、2以上のとき連鎖ボーナスを加える。
	void AddFromClear(int32_t clearedCellCount,int32_t chainCount);

	// ゲージが満タンで発動可能か
	bool CanActivate() const;

	// 発動可能ならゲージを消費して true を返す
	bool Consume();

	// デバッグ操作用
	void Fill();
	void Reset();

	int32_t GetValue() const{ return value_; }
	int32_t GetMaxValue() const;
	float GetRatio() const;

private:
	int32_t value_ = 0;
};

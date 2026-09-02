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

	// スペシャル発動後の制限時間を1フレーム分進める
	void Update();

	// ゲージが満タンでスペシャルを開始可能か
	bool CanActivate() const;

	// 満タンのゲージでスペシャルを開始し、減少を始める
	bool StartActivation();

	// 発動可能ならゲージを消費して true を返す
	bool Consume();

	// デバッグ操作用
	void Fill();
	void Reset();

	int32_t GetValue() const{ return value_; }
	int32_t GetMaxValue() const;
	float GetRatio() const;
	bool IsActivationActive() const{ return isActivationActive_; }

private:
	int32_t value_ = 0;
	int32_t drainTimer_ = 0;
	bool isActivationActive_ = false;
};

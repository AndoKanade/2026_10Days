#pragma once
#include <chrono>

class TimeManager{
public:
    static TimeManager* GetInstance();

    // 初期化
    void Initialize();
    // 更新（FPS固定処理）
    void Update();

private:
    TimeManager() = default;
    ~TimeManager() = default;
    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;

    std::chrono::steady_clock::time_point reference_;
};
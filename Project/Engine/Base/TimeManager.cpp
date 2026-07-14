#include "TimeManager.h"
#include <thread>

TimeManager* TimeManager::GetInstance(){
    static TimeManager instance;
    return &instance;
}

void TimeManager::Initialize(){
    reference_ = std::chrono::steady_clock::now();
}

void TimeManager::Update(){
    const std::chrono::microseconds kMinTime(16666); // 1/60sec
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

    if(elapsed < kMinTime){
        std::this_thread::sleep_for(kMinTime - elapsed);
    }
    reference_ = std::chrono::steady_clock::now();
}
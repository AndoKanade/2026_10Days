#pragma once
#include "BaseScene.h"
#include <string>
#include <memory> 

/// <summary>
/// シーン工場のインターフェース (設計図)
/// </summary>
class AbstractSceneFactory{
public:
    virtual ~AbstractSceneFactory() = default;

    // シーン生成関数 (文字列を受け取って、そのシーンを返す)
    virtual std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) = 0;
};
#pragma once
#include <map>
#include <string>
#include <memory>
#include "Model.h"

class DXCommon;

class ModelManager{
public:
    static ModelManager* GetInstance();

    void Initialize(DXCommon* dxCommon);
    void Finalize();

    // モデル管理
    void LoadModel(const std::string& filePath);
    Model* FindModel(const std::string& filePath);

    // ゲッター
    ModelCommon* GetModelCommon() const{ return modelCommon.get(); }

    // デバッグ用
    void UpdateLightGui();

private:
    ModelManager() = default;
    ~ModelManager() = default;
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

    std::unique_ptr<ModelCommon> modelCommon = nullptr;
    std::map<std::string,std::unique_ptr<Model>> models;
};
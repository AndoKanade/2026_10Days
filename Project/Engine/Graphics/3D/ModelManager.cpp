#include "ModelManager.h"
#include "DXCommon.h"
#include "ModelCommon.h"

ModelManager* ModelManager::GetInstance(){
    static ModelManager instance;
    return &instance;
}

void ModelManager::Initialize(DXCommon* dxCommon){
    modelCommon = std::make_unique<ModelCommon>();
    modelCommon->Initialize(dxCommon);
}

void ModelManager::Finalize(){
    models.clear();
    modelCommon.reset();
}

void ModelManager::LoadModel(const std::string& filePath){
    if(models.contains(filePath)){
        return;
    }

    std::unique_ptr<Model> model = std::make_unique<Model>();
    model->Initialize(modelCommon.get(),"resource",filePath);
    models.insert(std::make_pair(filePath,std::move(model)));
}

Model* ModelManager::FindModel(const std::string& filePath){
    if(models.contains(filePath)){
        return models.at(filePath).get();
    }
    return nullptr;
}

void ModelManager::UpdateLightGui(){
#ifdef _DEBUG
    DirectionalLight* data = modelCommon->GetLightData();
    if(!data) return;

    ImGui::Begin("Lighting Control");
    ImGui::ColorEdit4("Light Color",&data->color.x);
    ImGui::SliderFloat3("Light Direction",&data->direction.x,-1.0f,1.0f);
    data->direction = Normalize(data->direction);
    ImGui::DragFloat("Intensity",&data->intensity,0.01f,0.0f,10.0f);
    ImGui::End();
#endif
}
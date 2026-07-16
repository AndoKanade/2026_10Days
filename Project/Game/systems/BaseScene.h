#pragma once

// --- 前方宣言 (インクルード循環防止) ---
class Obj3dCommon;
class Input;
class SceneManager;
class SpriteCommon;

/// <summary>
/// シーンの基底クラス (抽象クラス)
/// </summary>
class BaseScene{
public:
	// 仮想デストラクタ (継承元として必須)
	virtual ~BaseScene() = default;

	// --- 純粋仮想関数 (継承先で必ず実装する) ---

	// 初期化
	virtual void Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon) = 0;
	// 終了処理
	virtual void Finalize() = 0;

	// 更新
	virtual void Update() = 0;

	// 描画
	virtual void Draw() = 0;

	// --- セッター ---

	// 管理者(SceneManager)をセットする
	virtual void SetSceneManager(SceneManager* sceneManager){
		sceneManager_ = sceneManager;
	}

protected:
	// シーンマネージャ (借りてくるだけなのでポインタのみ。deleteしない)
	SceneManager* sceneManager_ = nullptr;
};
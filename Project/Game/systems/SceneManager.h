#pragma once

// --- 標準ライブラリ ---
#include <string>
#include <memory>

// --- エンジン内ヘッダー ---
#include "BaseScene.h"
#include "AbstractSceneFactory.h"

// --- 前方宣言 ---
class Obj3dCommon;
class SpriteCommon;
class Input;

/// <summary>
/// シーン管理クラス (シングルトンパターン)
/// ゲーム内のシーン遷移、現在のシーンの更新・描画を一括管理します。
/// </summary>
class SceneManager{
private:
	// シングルトンパターンのため、コンストラクタとデストラクタは隠蔽
	SceneManager() = default;
	~SceneManager() = default;

public:
	// コピーコンストラクタと代入演算子を無効化 (複製の禁止)
	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	// -------------------------------------------------
	// メインループ・ライフサイクル
	// -------------------------------------------------

	/// <summary>
	/// インスタンスの取得 (Meyers Singleton)
	/// </summary>
	static SceneManager* GetInstance();

	/// <summary>
	/// 終了処理
	/// 保持しているシーンやリソースの解放を行います。
	/// </summary>
	void Finalize();

	/// <summary>
	/// 更新処理
	/// 現在のシーンの更新と、シーン切り替えリクエストの処理を行います。
	/// </summary>
	void Update();

	/// <summary>
	/// 描画処理
	/// 現在のシーンの描画メソッドを呼び出します。
	/// </summary>
	void Draw();

	/// <summary>
	/// 次のシーンへの切り替え予約
	/// 実際の切り替えは Update() の冒頭で行われます。
	/// </summary>
	/// <param name="sceneName">遷移先のシーン名</param>
	void ChangeScene(const std::string& sceneName);

	// -------------------------------------------------
	// セットアップ (初期化時に使用)
	// -------------------------------------------------

	/// <summary>
	/// シーン生成ファクトリーの登録
	/// </summary>
	void SetFactory(AbstractSceneFactory* factory){
		sceneFactory_ = factory;
	}

	/// <summary>
	/// 共通リソースポインタの設定
	/// 各シーンに渡す基盤システムのポインタを保持します。
	/// </summary>
	void SetCommonPtr(Obj3dCommon* common,Input* input,SpriteCommon* spriteCommon){
		object3dCommon_ = common;
		input_ = input;
		spriteCommon_ = spriteCommon;
	}

private:
	// -------------------------------------------------
	// メンバ変数
	// -------------------------------------------------

	// シーン生成用ファクトリー (Frameworkが所有権を持つため、ここはポインタ参照のみ)
	AbstractSceneFactory* sceneFactory_ = nullptr;

	// 現在実行中のシーン
	// unique_ptr により、シーン切り替え時や終了時に自動的にメモリが解放されます
	std::unique_ptr<BaseScene> scene_;

	// 次に遷移するシーンの名前 (空文字列でない場合、遷移リクエストありと判断)
	std::string nextSceneName_ = "";

	// --- 共通リソース (借用ポインタ) ---
	Obj3dCommon* object3dCommon_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;
	Input* input_ = nullptr;
};
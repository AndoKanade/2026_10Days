#pragma once

// --- 標準ライブラリ ---
#include <memory>

// --- 基盤・システムヘッダー ---
#include "WinAPI.h"
#include "DXCommon.h"
#include "Input.h"

// --- マネージャー・共通クラス ---
#include "SrvManager.h"
#include "ImGuiManager.h"
#include "SoundManager.h"
#include "CameraManager.h"
#include "ParticleManager.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "TimeManager.h"
#include "SpriteCommon.h"
#include "Obj3DCommon.h"

// --- シーン関連 ---
#include "BaseScene.h"
#include "AbstractSceneFactory.h"

/// <summary>
/// ゲームエンジンの基盤クラス
/// ウィンドウ生成、DirectX初期化、入力管理、メインループの流れを制御します。
/// 継承先のクラス（MyGameなど）で具体的なゲームロジックと描画を実装します。
/// </summary>
class Framework{
public: // --- 公開メソッド ---

	/// <summary>
	/// 仮想デストラクタ
	/// </summary>
	virtual ~Framework() = default;

	/// <summary>
	/// 初期化処理
	/// ウィンドウ、DirectX、各マネージャの生成と初期化を行います。
	/// </summary>
	virtual void Initialize();

	/// <summary>
	/// 終了処理
	/// 確保したリソースの解放や、システムの終了処理を行います。
	/// </summary>
	virtual void Finalize();

	/// <summary>
	/// 毎フレームの更新処理
	/// 入力情報の更新や、共通システムの更新を行います。
	/// </summary>
	virtual void Update();

	/// <summary>
	/// 描画処理（純粋仮想関数）
	/// 実際の描画内容は継承先のクラスで実装する必要があります。
	/// </summary>
	virtual void Draw() = 0;

	/// <summary>
	/// ゲームループ終了リクエストの確認
	/// </summary>
	/// <returns>終了フラグが立っていれば true</returns>
	virtual bool IsEndRequest(){ return endRequest_; }

protected: // --- メンバ変数（継承先からもアクセス可能） ---

	// ゲームループ終了フラグ
	bool endRequest_ = false;

	// --- コアシステム群 (Frameworkが所有権を持つ) ---
	std::unique_ptr<WinAPI> winApi_;       // ウィンドウ管理
	std::unique_ptr<DXCommon> dxCommon_;   // DirectX基本設定
	std::unique_ptr<Input> input_;         // 入力管理

	// --- 描画共通設定 (Frameworkが所有権を持つ) ---
	std::unique_ptr<SpriteCommon> spriteCommon_;     // スプライト共通設定
	std::unique_ptr<Obj3dCommon> object3dCommon_;    // 3Dオブジェクト共通設定

	// --- シーン管理 ---
	std::unique_ptr<AbstractSceneFactory> sceneFactory_; // シーン生成ファクトリー
};
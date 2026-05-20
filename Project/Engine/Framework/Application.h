#pragma once
#include "Framework.h"
#include "SceneManager.h"
#include "RenderTexture.h"
#include "PostProcess.h"
#include <memory>
#include <string>

/// <summary>
/// アプリケーションクラス
/// Frameworkクラスを継承し、このゲーム固有の処理（シーン管理やループ動作）を実装します。
/// </summary>
class Application : public Framework{
public:
	// --- コンストラクタ・デストラクタ ---

	// コンストラクタ
	Application();

	// デストラクタ
	// 基底クラスの仮想デストラクタをオーバーライドします
	~Application() override;

	// --- シングルトンアクセサ ---
	static Application* GetInstance(){ return instance_; }

	// --- Frameworkの純粋仮想関数をオーバーライド ---

	/// <summary>
	/// 初期化処理
	/// アプリケーション起動時に一度だけ呼ばれます。
	/// ウィンドウ生成後のリソース読み込みや、最初のシーン設定を行います。
	/// </summary>
	void Initialize() override;

	/// <summary>
	/// 終了処理
	/// アプリケーション終了時に一度だけ呼ばれます。
	/// 確保したメモリやリソースの解放を行います。
	/// </summary>
	void Finalize() override;

	/// <summary>
	/// 更新処理
	/// 毎フレーム呼ばれ、ゲームの計算や入力処理を行います。
	/// </summary>
	void Update() override;

	/// <summary>
	/// 描画処理
	/// 毎フレーム呼ばれ、画面への描画コマンドを発行します。
	/// </summary>
	void Draw() override;

	// --- デバッグUI関数 ---

	/// <summary>
	/// ポストプロセスのデバッグUIを表示します。
	/// </summary>
	void ShowPostProcessUI();

private:
	// --- 静的メンバ変数 ---
	static Application* instance_;

	// --- ポストプロセス関連リソース ---
	std::unique_ptr<RenderTexture> renderTexture_;
	std::unique_ptr<PostProcess> postProcess_;
	PostProcess::Type currentPPType_ = PostProcess::Type::PostProcess;
	std::string currentMaskPath_;

	// --- Dissolveアニメーション用メンバ変数 ---
	bool isDissolving_ = false;           // アニメーション中かどうかのフラグ
	float dissolveTimer_ = 0.0f;          // アニメーションの経過時間
	const float kDissolveDuration = 2.0f; // アニメーションの合計時間（秒）
};
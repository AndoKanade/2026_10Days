#pragma once
#include "externals/imgui/imgui/imgui.h"
#include <Windows.h>
#include <cstdint>

// ImGuiのウィンドウプロシージャ用ハンドラ
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lPalam);

class WinAPI{
public:
	// ウィンドウメッセージを処理するコールバック関数
	static LRESULT CALLBACK WindowProc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam);

	// 初期化処理
	void Initialize(const wchar_t* title,int32_t width,int32_t height);

	// 更新処理
	void Update();

	// 終了処理
	void Finalize();

	// メッセージの取得とディスパッチ
	bool ProcessMessage();

	// ハンドルのゲッター
	HWND GetHwnd() const{ return hwnd; }
	HINSTANCE GetHinstance() const{ return wc.hInstance; }

	// 追加 フルスクリーン切り替え
	// フルスクリーン状態を設定する（true でボーダーレスフルスクリーン、false でウィンドウモード）
	void SetFullscreen(bool fullscreen);

	// フルスクリーンとウィンドウモードを切り替える
	void ToggleFullscreen(){ SetFullscreen(!isFullscreen); }

	// 現在フルスクリーンかどうかを取得する
	bool IsFullscreen() const{ return isFullscreen; }

	// 追加 リサイズ要求の受け取り
	// リサイズ要求があれば新しいクライアントサイズを返してフラグを下ろす
	// 1フレームに1回だけ呼ぶことで、ドラッグ中の連続したWM_SIZEを1回にまとめる
	bool ConsumeResizeRequest(int32_t& outWidth,int32_t& outHeight);

	// クライアント領域の論理サイズ設定
	// 描画リソースの実サイズはウィンドウに追従するが、
	// スプライトの座標系とカメラのアスペクト比はこの値を基準にする
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;

private:
	HWND hwnd = nullptr; // ウィンドウハンドル
	WNDCLASS wc{};       // ウィンドウクラス情報

	// 追加 フルスクリーン切り替え用の情報
	bool isFullscreen = false;          // 現在フルスクリーンかどうか
	WINDOWPLACEMENT windowPlacement{};  // ウィンドウモードに戻すための位置とサイズ
	LONG windowStyle = 0;               // ウィンドウモードに戻すためのウィンドウスタイル

	// 追加 リサイズ要求の情報
	bool isResizeRequested = false;     // リサイズ要求が来ているかどうか
	int32_t requestedWidth = 0;         // 要求された新しいクライアント幅
	int32_t requestedHeight = 0;        // 要求された新しいクライアント高さ

	// ウィンドウプロシージャから自身を参照するためのインスタンスポインタ
	static WinAPI* instance;
};
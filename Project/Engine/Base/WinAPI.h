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

	// クライアント領域のサイズ設定
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;

private:
	HWND hwnd = nullptr; // ウィンドウハンドル
	WNDCLASS wc{};       // ウィンドウクラス情報
};
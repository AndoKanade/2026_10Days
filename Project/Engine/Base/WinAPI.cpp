#include "WinAPI.h"
#include "imgui_impl_win32.h"

#ifdef USE_IMGUI
// ImGuiのWindows用ウィンドウプロシージャハンドラ
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lPalam);
#endif

// 追加 ウィンドウプロシージャから自身を参照するためのインスタンスポインタの実体
WinAPI* WinAPI::instance = nullptr;

// 追加 WM_SYSKEYDOWN の lParam で Alt キーが押されていることを示すビット（bit29）
static constexpr LPARAM kAltKeyFlag = 1 << 29;

// ウィンドウメッセージ処理のコールバック関数
LRESULT CALLBACK WinAPI::WindowProc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam){
#ifdef USE_IMGUI
	// ImGuiがイベントを消費した場合は処理を終了
	if(ImGui_ImplWin32_WndProcHandler(hwnd,msg,wparam,lparam)){
		return true;
	}
#endif

	switch(msg){
	case WM_DESTROY:
		// ウィンドウが破棄されたら終了メッセージを送る
		PostQuitMessage(0);
		return 0;

	// 追加 F11 でフルスクリーンを切り替える
	case WM_KEYDOWN:
		if(wparam == VK_F11 && instance){
			instance->ToggleFullscreen();
			return 0;
		}
		break;

	// 追加 Alt + Enter でフルスクリーンを切り替える
	case WM_SYSKEYDOWN:
		if(wparam == VK_RETURN && (lparam & kAltKeyFlag) != 0 && instance){
			instance->ToggleFullscreen();
			return 0;
		}
		break;

	// 追加 ウィンドウサイズの変更を記録する
	case WM_SIZE:
		if(instance && wparam != SIZE_MINIMIZED){
			// lParam の下位ワードが幅、上位ワードが高さ
			int32_t newWidth = LOWORD(lparam);
			int32_t newHeight = HIWORD(lparam);

			// サイズがゼロのときは描画リソースを作れないので無視する
			if(newWidth > 0 && newHeight > 0){
				instance->requestedWidth = newWidth;
				instance->requestedHeight = newHeight;
				instance->isResizeRequested = true;
			}
		}
		break;
	}

	return DefWindowProc(hwnd,msg,wparam,lparam);
}

// ウィンドウの初期化処理
void WinAPI::Initialize(const wchar_t* title,int32_t width,int32_t height){
	// 追加 ウィンドウプロシージャから参照できるように自身を登録する
	instance = this;

	// COMライブラリの初期化
	HRESULT hr = CoInitializeEx(0,COINIT_MULTITHREADED);

	// ウィンドウクラスの設定
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"Andou_Kanade";
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr,IDC_ARROW);

	// ウィンドウクラスの登録
	RegisterClass(&wc);

	// ウィンドウサイズを調整（クライアント領域を基準にする）
	RECT wrc = {0, 0, width, height};
	AdjustWindowRect(&wrc,WS_OVERLAPPEDWINDOW,false);

	// ウィンドウの生成
	hwnd = CreateWindow(
		wc.lpszClassName,
		title, // ウィンドウタイトル
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);

	// ウィンドウの表示
	ShowWindow(hwnd,SW_SHOW);
}

// 更新処理（現在は空）
void WinAPI::Update(){}

// 追加 フルスクリーン状態の設定（ボーダーレスウィンドウ方式）
void WinAPI::SetFullscreen(bool fullscreen){
	// 状態が変わらない場合は何もしない
	if(isFullscreen == fullscreen){
		return;
	}

	if(fullscreen){
		// ウィンドウモードに戻すための現在の位置とスタイルを保存する
		windowPlacement.length = sizeof(WINDOWPLACEMENT);
		GetWindowPlacement(hwnd,&windowPlacement);
		windowStyle = GetWindowLong(hwnd,GWL_STYLE);

		// ウィンドウが乗っているモニタの表示領域を取得する
		HMONITOR monitor = MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);
		MONITORINFO monitorInfo{};
		monitorInfo.cbSize = sizeof(MONITORINFO);
		GetMonitorInfo(monitor,&monitorInfo);
		const RECT& monitorRect = monitorInfo.rcMonitor;

		// 枠なしスタイルに変更し、モニタ全体を覆うように配置する
		SetWindowLong(hwnd,GWL_STYLE,WS_POPUP | WS_VISIBLE);
		SetWindowPos(
			hwnd,HWND_TOP,
			monitorRect.left,
			monitorRect.top,
			monitorRect.right - monitorRect.left,
			monitorRect.bottom - monitorRect.top,
			SWP_FRAMECHANGED | SWP_NOOWNERZORDER
		);
	} else{
		// 保存しておいたスタイルと位置を復元する
		SetWindowLong(hwnd,GWL_STYLE,windowStyle);
		SetWindowPlacement(hwnd,&windowPlacement);
		SetWindowPos(
			hwnd,nullptr,0,0,0,0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER
		);
	}

	isFullscreen = fullscreen;
}

// 追加 リサイズ要求の受け取り
bool WinAPI::ConsumeResizeRequest(int32_t& outWidth,int32_t& outHeight){
	// 要求が来ていなければ何も返さない
	if(!isResizeRequested){
		return false;
	}

	outWidth = requestedWidth;
	outHeight = requestedHeight;

	// 同じ要求を二重に処理しないようフラグを下ろす
	isResizeRequested = false;

	return true;
}

// メッセージの取得とディスパッチ
bool WinAPI::ProcessMessage(){
	MSG msg{};

	// メッセージがある場合は取得して処理
	if(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// 終了メッセージが来たらtrueを返す
	if(msg.message == WM_QUIT){
		return true;
	}

	return false;
}

// 終了処理
void WinAPI::Finalize(){
	CloseWindow(hwnd);
	CoUninitialize();
}
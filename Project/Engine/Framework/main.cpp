#include "Application.h"
#include "D3DResourceLeakChecker.h"
#include "TimeManager.h"
#include <memory>

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){
	// リソースリークチェック用オブジェクト
	D3DResourceLeakChecker leakCheck;

	// アプリケーション（Framework）の生成
	std::unique_ptr<Framework> game = std::make_unique<Application>();

	// --- 初期化処理 ---
	game->Initialize();

	// --- メインループ ---
	while(true){
		// 終了リクエストが来たらループを抜ける
		if(game->IsEndRequest()){
			break;
		}

		// 更新と描画
		game->Update();
		game->Draw();

		// 時間管理マネージャーの更新
		TimeManager::GetInstance()->Update();
	}

	// --- 終了処理 ---
	game->Finalize();

	return 0;
}
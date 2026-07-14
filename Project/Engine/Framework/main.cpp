#include "Application.h"
#include "D3DResourceLeakChecker.h"
#include "TimeManager.h"
#include <memory> 

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){
	D3DResourceLeakChecker leakCheck;
	std::unique_ptr<Framework> game = std::make_unique<Application>();

	// 初期化
	game->Initialize();

	// ゲームループ
	while(true){
		if(game->IsEndRequest()){
			break;
		}	

		// 更新と描画
		game->Update();
		game->Draw();

		TimeManager::GetInstance()->Update();
	}

	// 終了処理
	game->Finalize();

	return 0;
}
#include "SceneFactory.h"

// --- 生成対象のシーンヘッダー ---
// std::make_unique でコンストラクタを呼び出すため、
// クラスの定義（中身）が書かれたヘッダーをインクルードする必要があります。
#include "TitleScene.h"
#include "GameScene.h"
#include "GameClearScene.h"
#include "GameOverScene.h"

// std::make_unique用
#include <memory>

// シーン生成処理
std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName){

	// 1. タイトル画面
	if(sceneName == "TITLE"){
		return std::make_unique<TitleScene>();
	}

	// 2. ゲームプレイ画面
	if(sceneName == "GAME"){
		return std::make_unique<GameScene>();
	}

	// 3. ゲームクリア画面
	if(sceneName == "GAMECLEAR"){
		return std::make_unique<GameClearScene>();
	}

	// 4. ゲームオーバー画面
	if(sceneName == "GAMEOVER"){
		return std::make_unique<GameOverScene>();
	}

	// 5. 該当するシーン名がない場合
	// 予期せぬ文字列が来た場合は nullptr を返してエラー扱いにします
	return nullptr;
}
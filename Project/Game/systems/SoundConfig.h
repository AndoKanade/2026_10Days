#pragma once

#include "SoundManager.h"
#include <string>

/// <summary>
/// 追加：効果音のパスと初期音量をまとめて持つ設定。
/// シーンごとにパスを書くと差し替えのたびに探し回ることになるため、ここへ集約する。
/// 再生はこのファイルの関数を通して行い、シーン側はパスを直接扱わない。
/// </summary>
namespace SoundConfig{

	// --- 汎用SE（どのシーンからも同じ意味で鳴らすもの） ---

	// カーソル移動（メニューの項目移動、音量バーの増減など）
	inline const std::string kCursorMoveSePath = "resource/SE/move.mp3";

	// 決定（メニューの確定、シーンの開始など）
	inline const std::string kDecideSePath = "resource/SE/decide.mp3";

	// キャンセル（メニューを閉じる、一段戻るなど）
	inline const std::string kCancelSePath = "resource/SE/cancel.mp3";

	// --- ゲーム中のブロック操作SE ---

	// ブロックの左右移動
	inline const std::string kBlockMoveSePath = "resource/SE/blockMove.mp3";

	// ブロックの回転
	inline const std::string kBlockRotateSePath = "resource/SE/rotate.mp3";

	// ブロックが盤面に固定されたとき（置いたとき）
	inline const std::string kBlockPlaceSePath = "resource/SE/place.mp3";

	// ホールドでブロックを入れ替えたとき
	inline const std::string kBlockHoldSePath = "resource/SE/hold.mp3";

	// --- 初期音量 ---
	// オプション画面で調整した値が保存されている場合はそちらが優先されるため、
	// ここの値は「まだ一度も調整していないときの音量」として使われる。

	// 汎用SEは何度も鳴るため、BGMを邪魔しない控えめな音量にする
	constexpr float kCursorMoveSeVolume = 0.4f;
	constexpr float kDecideSeVolume = 0.6f;
	constexpr float kCancelSeVolume = 0.5f;

	// ブロック操作SEも連続で鳴るため控えめにする。
	// 設置は操作の区切りなので、移動・回転より少しだけ強くする。
	constexpr float kBlockMoveSeVolume = 0.35f;
	constexpr float kBlockRotateSeVolume = 0.4f;
	constexpr float kBlockPlaceSeVolume = 0.55f;
	constexpr float kBlockHoldSeVolume = 0.5f;

	/// <summary>
	/// 汎用SEをまとめてロードする。
	/// 二度目以降の呼び出しはSoundManager側で無視されるため、各シーンの初期化で呼んでよい。
	/// </summary>
	inline void LoadCommonSe(){
		SoundManager* soundManager = SoundManager::GetInstance();
		soundManager->SoundLoadFile(kCursorMoveSePath,SoundCategory::SE);
		soundManager->SoundLoadFile(kDecideSePath,SoundCategory::SE);
		soundManager->SoundLoadFile(kCancelSePath,SoundCategory::SE);
	}

	/// <summary>
	/// ゲーム中のブロック操作SEをまとめてロードする。
	/// </summary>
	inline void LoadBlockSe(){
		SoundManager* soundManager = SoundManager::GetInstance();
		soundManager->SoundLoadFile(kBlockMoveSePath,SoundCategory::SE);
		soundManager->SoundLoadFile(kBlockRotateSePath,SoundCategory::SE);
		soundManager->SoundLoadFile(kBlockPlaceSePath,SoundCategory::SE);
		soundManager->SoundLoadFile(kBlockHoldSePath,SoundCategory::SE);
	}

	// --- 再生窓口 ---
	// ロードできていないSEはSoundManager側で無視されるため、
	// 音源をまだ用意していなくても呼び出して構わない。

	/// カーソル移動SEを鳴らす
	inline void PlayCursorMove(){
		SoundManager::GetInstance()->PlayAudio(kCursorMoveSePath,kCursorMoveSeVolume);
	}

	/// 決定SEを鳴らす
	inline void PlayDecide(){
		SoundManager::GetInstance()->PlayAudio(kDecideSePath,kDecideSeVolume);
	}

	/// キャンセルSEを鳴らす
	inline void PlayCancel(){
		SoundManager::GetInstance()->PlayAudio(kCancelSePath,kCancelSeVolume);
	}

	/// ブロックの左右移動SEを鳴らす
	inline void PlayBlockMove(){
		SoundManager::GetInstance()->PlayAudio(kBlockMoveSePath,kBlockMoveSeVolume);
	}

	/// ブロックの回転SEを鳴らす
	inline void PlayBlockRotate(){
		SoundManager::GetInstance()->PlayAudio(kBlockRotateSePath,kBlockRotateSeVolume);
	}

	/// ブロックの設置SEを鳴らす
	inline void PlayBlockPlace(){
		SoundManager::GetInstance()->PlayAudio(kBlockPlaceSePath,kBlockPlaceSeVolume);
	}

	/// ホールドSEを鳴らす
	inline void PlayBlockHold(){
		SoundManager::GetInstance()->PlayAudio(kBlockHoldSePath,kBlockHoldSeVolume);
	}
}

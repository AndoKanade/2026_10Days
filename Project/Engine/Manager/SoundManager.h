#pragma once

// --- 標準ライブラリ ---
#include <string>
#include <map>
#include <vector>
#include <cstdint>

// --- DirectX / Windows関連 ---
#include <wrl.h>
#include <xaudio2.h>

// --- Media Foundation関連 ---
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

// --- ライブラリリンク設定 ---
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

/// <summary>
/// 音声データ構造体
/// フォーマット情報と、デコード済みの波形データを保持する
/// </summary>
struct SoundData{
	WAVEFORMATEX wfex;			// 波形フォーマット
	std::vector<BYTE> pBuffer;	// 波形データのバッファ (自動メモリ管理)
	unsigned int bufferSize;	// バッファのサイズ
};

/// <summary>
/// サウンド管理クラス (シングルトン)
/// MP3/WAVファイルのロード、再生、停止、一時停止などを管理する
/// </summary>
class SoundManager{
public: // --- シングルトンインスタンス取得 ---

	static SoundManager* GetInstance();

public: // --- システム初期化・終了 ---

	/// <summary>
	/// 初期化 (XAudio2, MediaFoundationの起動)
	/// </summary>
	void Initialize();

	/// <summary>
	/// 終了処理 (データの解放, エンジンの終了)
	/// </summary>
	void Finalize();

public: // --- 音声ロード・再生制御 ---

	/// <summary>
	/// 音声ファイルをロード (MP3, WAVなど)
	/// </summary>
	/// <param name="filename">ファイルパス (キーとして使用)</param>
	void SoundLoadFile(const std::string& filename);

	/// <summary>
	/// 音声を再生
	/// </summary>
	/// <param name="filename">ファイルパス</param>
	/// <param name="volume">音量 (0.0=無音, 1.0=最大)</param>
	/// <param name="loop">trueで無限ループ</param>
	void PlayAudio(const std::string& filename,float volume = 1.0f,bool loop = false);

	/// <summary>
	/// 音声を停止 (停止後は最初から再生になる)
	/// </summary>
	void StopAudio(const std::string& filename);

	/// <summary>
	/// 一時停止 (再生位置を保持したまま止める)
	/// </summary>
	void PauseAudio(const std::string& filename);

	/// <summary>
	/// 再開 (一時停止した位置から再生する)
	/// </summary>
	void ResumeAudio(const std::string& filename);

	/// <summary>
	/// 再生中かどうかを確認
	/// </summary>
	bool IsPlaying(const std::string& filename);

public: // --- 音量制御 (追加) ---

	/// <summary>
	/// 個別の音量を設定 (再生中なら即座に反映される)
	/// </summary>
	/// <param name="filename">ファイルパス</param>
	/// <param name="volume">音量 (0.0=無音, 1.0=最大)</param>
	void SetVolume(const std::string& filename,float volume);

	/// <summary>
	/// 個別の音量を取得 (未設定の場合は最大音量を返す)
	/// </summary>
	float GetVolume(const std::string& filename) const;

	/// <summary>
	/// マスター音量を設定 (すべての音声にまとめて掛かる)
	/// </summary>
	void SetMasterVolume(float volume);

	/// <summary>
	/// マスター音量を取得
	/// </summary>
	float GetMasterVolume() const;

	/// <summary>
	/// ImGuiで音量を調整するデバッグUIを表示する
	/// マスター音量と、ロード済み音声ごとの音量・再生状態を操作できる
	/// </summary>
	void ShowVolumeGui();

private: // --- コンストラクタ・デストラクタ (外部からの生成禁止) ---
	SoundManager() = default;
	~SoundManager() = default;
	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;

private: // --- 内部ヘルパー関数 ---

	// 音声データのメモリ解放
	void Unload(SoundData* soundData);

private: // --- メンバ変数 ---

	// XAudio2本体
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	// マスターボイス (最終的な出力先)
	IXAudio2MasteringVoice* masterVoice_ = nullptr;

	// ロード済み音声データの管理コンテナ [キー:ファイル名]
	std::map<std::string,SoundData> soundDatas_;

	// 再生中のソースボイス管理コンテナ [キー:ファイル名]
	std::map<std::string,IXAudio2SourceVoice*> activeVoices_;

	// 追加：音声ごとの音量 [キー:ファイル名]。停止中でも値を保持する
	std::map<std::string,float> volumes_;

	// 追加：マスター音量 (すべての音声に掛かる係数)
	float masterVolume_ = 1.0f;
};
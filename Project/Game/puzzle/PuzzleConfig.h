#pragma once

#include <cstdint>
#include <cmath>

#include "MyMath.h"
#include "BlockShape.h"

// =============================================================================
// パズルゲームの調整用定数
// 盤面サイズ・落下速度・固定猶予など、調整のたびに触る値をここに集約する。
// マジックナンバーをコード中に直接書かず、必ずこの名前空間の定数を参照すること。
// =============================================================================
namespace PuzzleConfig{

	// --- 盤面サイズ（マス単位）---

	// 通常時の盤面の幅（マス数）
	constexpr int32_t kBoardWidth = 10;

	// 盤面の高さ（マス数）
	constexpr int32_t kBoardHeight = 10;

	// 横断が難しすぎて消えない場合に切り替える、狭い盤面の幅（マス数）
	// 実際の切り替え処理は担当Cがデバッグ用UIで行う。ここでは候補値のみ定義する。
	constexpr int32_t kBoardWidthNarrow = 6;

	// 配列確保などで使う、盤面幅の最大値
	// 幅を実行時に切り替えても配列を作り直さずに済むよう、常にこの値で確保する。
	constexpr int32_t kBoardWidthMax = kBoardWidth;

	// --- 盤面の描画レイアウト（3D空間・ワールド単位）---
	// 見た目は2Dだが、描画は3Dモデル（defaultBlock のキューブ）を並べて表現する。
	// 盤面は原点を中心に XY 平面へ並べ、カメラは -Z 側から見る。

	// 隣り合うマスの中心どうしの距離（ワールド単位）
	constexpr float kCellWorldSize = 1.0f;

	// 1マスに置くキューブモデルの拡大率。
	// defaultBlock のキューブは1辺2なので、2 * この値 がマスの見た目の大きさになる。
	// kCellWorldSize より少し小さくして、マスの間に隙間を作る。
	constexpr float kCellModelScale = 0.45f;

	// 盤面を並べる Z 座標
	constexpr float kBoardCenterZ = 0.0f;

	// --- 時間関連（フレーム単位）---
	// このエンジンは可変デルタタイムを持たず 60fps 固定で動く前提のため、
	// 時間はすべてフレーム数で数える。コメントに秒換算を併記する。

	// 想定フレームレート（秒→フレーム換算の参考値）
	constexpr float kFrameRate = 60.0f;

	// ブロックが自動で1マス落下するまでのフレーム数（60 = 約1.0秒）
	constexpr int32_t kFallIntervalFrames = 60;

	// 操作できる時間10秒ごとに2フレームずつ短縮する。最速でも0.3秒/マス。
	constexpr int32_t kFallSpeedStepFrames = 10 * static_cast<int32_t>(kFrameRate);// 何秒ごとに落下速度を上げるか
	constexpr int32_t kFallSpeedReductionFrames = 2;// 何フレームずつ落下速度を上げるか
	constexpr int32_t kFallIntervalFramesMin = 18;// 最速で何フレームまで落下速度を上げるか（18 = 約0.3秒）
	constexpr int32_t GetFallIntervalFrames(int64_t activeFrames){
		const int64_t steps = activeFrames > 0 ? activeFrames / kFallSpeedStepFrames : 0;
		const int64_t maxSteps = (kFallIntervalFrames - kFallIntervalFramesMin +
			kFallSpeedReductionFrames - 1) / kFallSpeedReductionFrames;
		if(steps >= maxSteps){ return kFallIntervalFramesMin; }
		return kFallIntervalFrames - static_cast<int32_t>(steps) * kFallSpeedReductionFrames;
	}

	// 下キーで加速させているときの落下フレーム数（6 = 約0.1秒）
	constexpr int32_t kFallIntervalFramesFast = 6;

	// 着地してから盤面に固定されるまでの猶予フレーム数（30 = 約0.5秒）
	constexpr int32_t kLockDelayFrames = 30;

	// 追加：通電成立から実際にマスを消すまでの演出フレーム数（消えるマスを光らせる時間）（20 = 約0.3秒）
	constexpr int32_t kClearEffectFrames = 20;

	// --- ブロック関連 ---

	// ブロックが出現する列（盤面上端。左端を0とした列インデックス）
	constexpr int32_t kSpawnColumn = kBoardWidth / 2 - 1;

	// ブロックが出現するときの基準マスの行。
	// 負の値は盤面上端（y=0）より上の空中を表す。落下してくる様子を見せるための余白で、
	// この範囲には盤面データを持たない。天井より上にはみ出したまま固定されるとゲームオーバー。
	constexpr int32_t kSpawnRow = -2;

	// マスが空であることを表す元ブロックID
	constexpr int32_t kEmptyBlockId = -1;
	// 元ブロックに属さない、固定済みの十字マス。
	constexpr int32_t kStrongestBlockId = -2;

	// --- スペシャルゲージ関連 ---

	// スペシャルを1回発動するために必要なゲージ量
	constexpr int32_t kSpecialGaugeMax = 30;

	// 通電して消去した1マスあたりのゲージ増加量
	constexpr int32_t kSpecialGaugePerClearedCell = 1;

	// 2連鎖目以降、連鎖が1段増えるごとに加算するボーナス
	constexpr int32_t kSpecialGaugeBonusPerAdditionalChain = 3;

	// スペシャル発動後、ゲージが0になるまでの制限時間（600フレーム = 約10秒）
	constexpr int32_t kSpecialReadyDurationFrames = 600;

	// ゲージを1減らす間隔
	constexpr int32_t kSpecialGaugeDrainIntervalFrames =
		kSpecialReadyDurationFrames / kSpecialGaugeMax;

	// --- ネクスト・ホールド関連 ---

	// 追加：ネクストとして先読み表示するブロックの個数
	constexpr int32_t kNextQueueSize = 3;

	// 追加：盤面の壁からプレビュー（ネクスト・ホールド）表示までの余白マス数
	constexpr int32_t kPreviewMarginCols = 2;

	// 追加：ブロックの形が基準マスから四方に張り出しうる最大マス数（I字縦置き基準）。
	// ホールド表示を左の壁と重ねないための余白計算に使う。
	constexpr int32_t kPreviewShapeMaxExtent = 2;

	// 変更：ネクストの各枠に確保する縦方向のマス数。
	// プレビューは常に回転0の形で表示するため、必要な高さは回転0での最大の縦幅
	// （L字・J字の3マス）ぶんで足りる。1マスぶんは重ならないための余白。
	constexpr int32_t kNextPreviewRowSpan = 3;

	// --- ブロックの質感 ---
	// ブロックは面取りキューブで描画する。辺の面取り面が平行光源を拾うことで、
	// 面の中心と辺で明るさに差が出て立体的なツヤが乗る。

	// 鏡面反射の鋭さ。小さいほどハイライトが広く柔らかくなる。
	// 面が平らなので下げすぎると面全体が白く濁って色がくすむ。控えめな値にしておく。
	constexpr float kBlockShininess = 30.0f;

	// 環境マップ（スカイボックス）の映り込みの強さ。
	// 全チャンネルに一律で足されるため、上げすぎると彩度が落ちる。
	constexpr float kBlockEnvironmentCoefficient = 0.03f;

	// ライティングを通すと、カメラ正面を向いた面はおよそ 0.57 倍まで暗くなる。
	// 下の色をそのまま渡すと沈んで見えるため、この係数で持ち上げてから渡す。
	// 面取り面はこれより明るくなり、上辺は白く飛んでハイライトになる。
	constexpr float kBlockLitColorGain = 1.75f;

	// ゴーストや使用済みホールドを暗く見せるときの、元の色に対する倍率。
	constexpr float kDimColorRate = 0.35f;

	// 追加：通電中のマスを明るく見せるときの設定。
	// 別の色で塗りつぶすと元のブロックの種類が分からなくなるため、色相は変えずに
	// 白へ少し寄せてから倍率をかける。明るい色は倍率だけでは頭打ちになって差が出ないため、
	// 白へ寄せるぶん（＝彩度の低下）で「光っている」を見せる。
	constexpr float kPoweredWhiteMixRate = 0.25f; // 白へ寄せる割合
	constexpr float kPoweredColorGain = 1.25f;    // 明るさの倍率

	// --- ブロックの種類ごとの色 ---
	// 並び順は BlockShape::Type に対応する。種類を足したらここにも色を足すこと。
	// 落ちものパズルの一般的なピース配色とは別の色相で組んでいる。
	//
	// 値は「画面に出したい色」（カラーピッカーで選ぶのと同じ sRGB の値）で書く。
	// 描画先が sRGB のレンダーターゲットで、シェーダには線形の値を渡す必要があるため、
	// GetBlockColor() が線形へ変換してから返す。ここに線形の値を直接書かないこと。
	inline const Vector4 kBlockColors[BlockShape::kTypeCount] = {
		{0.40f, 0.85f, 0.20f, 1.0f}, // L 黄緑
		{0.92f, 0.20f, 0.35f, 1.0f}, // T 紅
		{1.00f, 0.68f, 0.05f, 1.0f}, // I 山吹
		{0.60f, 0.30f, 0.95f, 1.0f}, // J 藤紫
	};

	// sRGB の1チャンネルを線形に変換する。
	inline float SrgbToLinear(float value){
		return (value <= 0.04045f) ? (value / 12.92f) : std::pow((value + 0.055f) / 1.055f,2.4f);
	}

	// 追加：sRGB で書いた色を、シェーダへ渡せる線形の値へ変換する。
	// 画面に出したい色をそのまま書けるよう、色の定数はすべてこれを通してから使う。
	inline Vector4 ToLinearColor(const Vector4& srgb){
		return {SrgbToLinear(srgb.x), SrgbToLinear(srgb.y), SrgbToLinear(srgb.z), srgb.w};
	}

	// ブロックの種類に対応する色を、シェーダへ渡せる線形の値で返す。
	inline Vector4 GetBlockColor(BlockShape::Type type){
		return ToLinearColor(kBlockColors[static_cast<int32_t>(type)]);
	}

	// ライティングを有効にして描くものへ渡す色を返す（減衰ぶんを持ち上げる）。
	inline Vector4 ApplyLitGain(const Vector4& color){
		return {color.x * kBlockLitColorGain, color.y * kBlockLitColorGain, color.z * kBlockLitColorGain, color.w};
	}

	// 追加：指定した色を通電中の見た目にしたものを返す。
	// 色相を保ったまま明るくするだけなので、何のブロックだったかは色で分かり続ける。
	inline Vector4 MakePoweredColor(const Vector4& color){
		return {
			(color.x + (1.0f - color.x) * kPoweredWhiteMixRate) * kPoweredColorGain,
			(color.y + (1.0f - color.y) * kPoweredWhiteMixRate) * kPoweredColorGain,
			(color.z + (1.0f - color.z) * kPoweredWhiteMixRate) * kPoweredColorGain,
			color.w
		};
	}

	// 指定した色を暗くしたものを返す（ゴーストや使用済み表示に使う）。
	inline Vector4 MakeDimColor(const Vector4& color){
		return {color.x * kDimColorRate, color.y * kDimColorRate, color.z * kDimColorRate, color.w};
	}

}

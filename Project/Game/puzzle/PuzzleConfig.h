#pragma once

#include <cstdint>

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

}

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include "MyMath.h"
#include "Cell.h"
#include "GridPos.h" // 追加：マス座標の共通型
#include "PuzzleConfig.h"
#include "Difficulty.h" // 追加：消す範囲を決める難易度

// 前方宣言
class Obj3dCommon;
class Obj3D;

// =============================================================================
// 盤面クラス
// 10×10のマス配列（データ）を保持し、外周をU字に囲む壁の描画を担当する。
// 見た目は2Dだが、描画は3Dモデル（defaultBlock のキューブ）を並べて表現する。
// 空きマスは描画しない。マスの中身の描画はブロック実装時に追加する。
// ブロックの落下や通電などのゲームロジックはまだ持たない。
// =============================================================================
class Board{
public:

	// 前方宣言した Obj3D を unique_ptr で持つため、コンストラクタ・デストラクタは cpp 側で定義する
	Board();
	~Board();

	// モデルの読み込みとオブジェクト生成などの初期化
	void Initialize(Obj3dCommon* object3dCommon);

	// 毎フレームの更新
	void Update();

	// 盤面（U字の壁）の描画
	void Draw();

	// --- ゲッター ---

	// 現在有効な盤面の幅（マス数）
	int32_t GetWidth() const{ return width_; }

	// 盤面の高さ（マス数）
	int32_t GetHeight() const{ return PuzzleConfig::kBoardHeight; }

	// 指定マスのデータを取得する（範囲チェックは呼び出し側の責任）
	const Cell& GetCell(int32_t x,int32_t y) const{ return cells_[y][x]; }

	// --- ブロック配置（3人分担の境界インターフェース。担当Aが中身を実装する）---

	// 追加：指定したマス群すべてが盤面内かつ空きなら配置可能とみなす。
	// 現状はスタブ。担当Aが実装するまで常に false を返す。
	bool CanPlace(const std::vector<GridPos>& cells) const;

	// 追加：指定したマス群へ blockId・端子ビットを書き込んで盤面に固定する。
	// terminals は cells と同じ並び・同じ要素数を想定する
	// （対応する要素が無いマスは0のまま）。
	// 変更：type は描画の色分けに使う元ブロックの種類。通電判定には影響しない。
	// 固定と同時に通電判定を行い、ゴールまで繋がっていれば対象マスを消す。
	void Place(const std::vector<GridPos>& cells,int32_t blockId,const std::vector<uint8_t>& terminals,BlockShape::Type type);

	// 追加：落下中ブロック用の配置可否判定。
	// 天井より上（y < 0）は空中とみなして通す。左右の壁・床・既存ブロックとの重なりのみ不可とする。
	bool CanFall(const std::vector<GridPos>& cells) const;

	// --- 判定・座標変換 ---

	// 指定マス座標が現在の盤面の範囲内かどうか
	bool IsInside(int32_t x,int32_t y) const;

	// 盤面のマス座標を、そのマスの中心のワールド座標に変換する
	Vector3 GridToWorld(int32_t x,int32_t y) const;

	// --- セッター ---

	// 盤面の幅を切り替える（6と10の切り替え用。デバッグUIから呼ぶ想定）
	void SetWidth(int32_t width);

	// 追加：通電が成立したときに消す範囲を切り替える。
	// タイトルで選んだ値を GameScene から渡す。詳しくは Difficulty.h を参照。
	void SetDifficulty(Difficulty difficulty){ difficulty_ = difficulty; }
	Difficulty GetDifficulty() const{ return difficulty_; }

	// 追加：マスの中身と消去演出の状態をすべて初期状態へ戻す（壁はそのまま）。
	// タイトル背景のデモプレイを、天井まで積み上がったときに最初からやり直すために使う。
	void Reset();

	// 追加：消去演出中かどうか。true の間は呼び出し側でブロックの操作・落下を止める想定。
	bool IsBusy() const{ return isClearing_; }

	// 変換できた場合だけtrueを返し、直後に通電再判定する。
	bool ConvertToStrongest(int32_t x,int32_t y);

	struct ClearResult{
		int32_t cellCount;
		int32_t chainCount;
	};
	// 実際に消去が完了した結果を一度だけ受け取る。
	std::vector<ClearResult> TakeClearResults();

private:

	// 借りてくるポインタ（このクラスでは生成・解放しない）
	Obj3dCommon* object3dCommon_ = nullptr;

	// 盤面データ。幅は最大値で確保し、実際に使う範囲を width_ で制御する。
	std::array<std::array<Cell,PuzzleConfig::kBoardWidthMax>,PuzzleConfig::kBoardHeight> cells_{};

	// 現在有効な盤面の幅（マス数）
	int32_t width_ = PuzzleConfig::kBoardWidth;

	// 追加：通電が成立したときに消す範囲。既定はタイトルと同じ Normal。
	Difficulty difficulty_ = Difficulty::Normal;

	// 盤面の外周をU字（左・下・右、上は開き）に囲む壁ブロックのオブジェクト
	std::vector<std::unique_ptr<Obj3D>> wallObjs_;

	// 追加：盤面に固定されたマスの見た目を表す3Dオブジェクト
	std::vector<std::unique_ptr<Obj3D>> cellObjs_;

	// 追加：消去演出中に、消える予定として保持しているマス
	std::vector<GridPos> clearingCells_;

	// 追加：消去演出中かどうか
	bool isClearing_ = false;

	// 追加：消去演出の経過フレーム数
	int32_t clearTimer_ = 0;
	int32_t chainCount_ = 0;
	std::vector<ClearResult> clearResults_;

	// 追加：今回の消去演出で、電源（最下段）からいちばん遠いマスまでの行の距離。
	// 「光の波」が下段から上段へ届くまでの進み具合を計算する基準に使う。
	int32_t clearWaveMaxDistance_ = 0;
  
	// U字の壁ブロックを1個生成して wallObjs_ に追加する（modelPath で使うモデルを指定する）
	void CreateWallBlock(int32_t x,int32_t y,const std::string& modelPath);
	// 追加：現在の width_ に合わせて、U字の壁ブロックを作り直す。
	// 右の壁・下の壁は幅に応じて位置・範囲が変わるため、幅切り替え時にも呼び直す必要がある。
	void RebuildWalls();
	// 追加：cells_ の埋まっているマスから cellObjs_ を作り直す
	// GPU用の定数バッファをマスごとに新規確保するため、消去演出中の毎フレーム更新には使わない。
	void RebuildCellObjects();

	// 追加：消去演出中の「光の波」アニメーションを、既存の cellObjs_ の色・スケールだけを
	// 書き換えて毎フレーム反映する軽量版（オブジェクトの作り直しはしない）。
	void UpdateClearingCellVisuals();

	// 追加：消去演出中のマス1個ぶんについて、光の波が届いているか・届いていたら
	// どれだけ膨らませるかを計算する（RebuildCellObjects/UpdateClearingCellVisuals共通処理）。
	void ComputeClearWaveState(int32_t y,bool& waveReached,float& popScale) const;

	// 追加：指定マスが現在消去演出中かどうか
	bool IsClearingCell(int32_t x,int32_t y) const;

	// 追加：電源（最下段）から実際にどこまで通電が届いているかを幅優先探索で調べる。
	// ゴールに届いているかは問わない。見た目のハイライト（2.6の常時可視化）に使う。
	std::array<std::array<bool,PuzzleConfig::kBoardWidthMax>,PuzzleConfig::kBoardHeight> ComputePoweredMask() const;

	// 追加：電源（最下段）から幅優先探索で通電範囲を調べ、ゴール（左右端）まで
	// 繋がっていれば、その範囲のマスを消去演出の対象にする（この時点ではまだ消さない）。
	void ResolveConduction();

	// 追加：Hard で使う。通電した一かたまりから行き止まりの枝を取り除き、
	// 電源とゴールを実際に結んでいるマスだけを返す。
	std::vector<GridPos> PruneDeadEndCells(const std::vector<GridPos>& component) const;

	// 追加：空きマスを詰めるように、各列のマスをマス単位で下へ落とす。
	// 対象にする列は、clearedCells に1マスでも含まれる列（今回の消去でマスが
	// 空いた列）に加えて、clearedBlockIds と同じ元ブロックIDの残骸が残っている列
	// （支えを失って構造的に浮いた可能性がある列）。それ以外の無関係な列は触らない。
	// 修正：forceAllColumns が true のときは、上記の判定に関わらず全列を対象にする。
	// Easy の横列消去は「行を丸ごと消す」仕様のため、その行のうち元々空きマスだった
	// 列（せり出しブロックの下の穴など）は clearedCells に入らず対象列から漏れる。
	// 漏れた列だけ他の列と一緒に落ちず、上のブロックが取り残されて見えるバグになる
	// ため、Easy の消去確定時はこのフラグで全列を強制的に詰め直す。
	void ApplyGravity(const std::vector<GridPos>& clearedCells,const std::vector<int32_t>& clearedBlockIds,bool forceAllColumns = false);
};

# successed.md

10daysJam「つなぐ」で新しくできたこと・作ったものの記録。

**書き方のルール**
- 新しい作業が終わるたびに「作業ログ」に日付ごとに追記する。
- 仕様が変わったら「現在の状態」を**上書き**する。変わった理由・経緯はログ側に残す。
- 「現在の状態」だけ読めば今どうなっているかが分かる、を維持する。

---

## 現在の状態

当面のゴール：**「配置まで」＝ 10×10 盤面にブロックが落ちて積める状態**
（全体の開発ロードマップは `CLAUDE.md` の「4. 開発ロードマップ」を参照）

### 描画方針

見た目は2Dだが、描画はすべて3Dで行う。ブロック・マス・壁は当面すべて `resource/defaultBlock/defaultBlock.obj`（1辺2の単位キューブ、tex は uvChecker）で代用する。

- 盤面は原点中心の XY 平面に配置。カメラは -Z 側（`GameScene` の既存カメラ z=-30 をそのまま利用）。
- 着色は各 `Obj3D` の `GetMaterial()->color`。`enableLighting=0` でフラットな2D的見た目に。
- 行列はカメラ追従のため毎フレーム `Update()`。

### ファイル構成

新規ファイルは `Game/puzzle/` に置く。

| ファイル | 内容 |
| --- | --- |
| `Game/puzzle/PuzzleConfig.h` | 調整用定数の集約ヘッダ。実装ロジックは持たない。 |
| `Game/puzzle/Cell.h` | マス構造体。`blockId`（空 = -1）＋端子4bit（未使用）＋ `IsEmpty()`。 |
| `Game/puzzle/Board.h` / `Board.cpp` | 10×10 のマスデータ配列＋U字の壁の描画。 |
| `Game/scenes/GameScene` | `Board board_;` をメンバに持ち、Initialize / Update / Draw から呼び出す。変更箇所は「追加：」コメントで明示。 |

`.vcxproj` / `.vcxproj.filters` に登録済み。`Game\puzzle` をインクルードディレクトリに追加（3構成すべて）、フィルタ「ヘッダー ファイル\Game\Puzzle」を新設。

### `PuzzleConfig.h` の定数

- 盤面サイズ：10×10（狭い盤面の候補として 6）
- `kCellWorldSize`：マス中心間の距離 1.0
- `kCellModelScale`：キューブ拡大率 0.45
- `kBoardCenterZ`
- 落下間隔・固定猶予（フレーム単位、60fps 固定前提）
- 出現列、空マスID

### `Board` の現状

- **持っているデータ**：`cells_`（10×10 のマス配列）。論理データとして保持し、`GetCell()` で参照する。
- **描画しているもの**：U字の壁ブロックのみ。マス領域のすぐ外側を左・下・右で囲み、上辺は開ける。`wallObjs_`（`std::vector<std::unique_ptr<Obj3D>>`）＋ `CreateWallBlock(x, y)`。色は `kWallColor`。
- **描画していないもの**：マスの中身。ブロック実装時に追加する。
- **API**：`Initialize(Obj3dCommon*)` / `Update()` / `Draw()` / `GridToWorld()` / `IsInside()` / `SetWidth()`
  - `GridToWorld()`：マス座標 → ワールド座標（`Vector3`、y=0 が上）。負座標・範囲外座標もそのまま計算できるので、壁は grid 座標 x=-1 / x=width / y=height に配置している。
  - `SetWidth()`：6/10 切り替え用。口だけ実装済みで UI 連携は未実装。
- 前方宣言した `Obj3D` を `unique_ptr` で持つため、コンストラクタ／デストラクタは cpp 側で `= default` 定義。

### ビルド・動作確認

Debug/x64 でビルド成功（`MyGameEngine.exe` 生成確認）。タイトル画面でスペースキーを押すと GAME シーンに入り盤面が見える。
**実機での見た目確認はまだ。**

---

## 今後の予定

### 「配置まで」に残っている作業

1. `Game/puzzle/GridPos.h`：盤面のマス座標を表す構造体（`struct GridPos { int x; int y; };`）。
2. `Game/puzzle/BlockShape.h/.cpp`：L字・T字の形。4回転分のマス相対座標テーブル（固定データ）。
3. `Game/puzzle/FallingBlock.h/.cpp`：落下中ブロック。基準座標＋回転index、自動落下、左右移動、回転（衝突時は拒否）、次ブロック出現。
4. 落下中ブロック・盤面に置かれたブロックの3D描画（`defaultBlock` キューブ、種類ごとに色分け）。
5. `Board` にロジック追加：`CanPlace(cells)` / `Place(cells, blockId)`、着地して盤面に固定、天井到達（ゲームオーバー）判定。
6. `GameScene` に `FallingBlock` を組み込み、キー入力（左右・回転・下加速）を接続。
7. デバッグUI：盤面幅 6/10 の切り替え。

### 次にやるステップ

`GridPos.h` → `BlockShape.h/.cpp` の順で着手予定。

### 「配置まで」に含めないもの（Day2以降）

配線描画、通電BFS、消去、最強マス、連鎖、状態機械、スコア、タイトル/リザルト。

---

## 3人分担とインターフェース

| 担当 | 範囲 |
| --- | --- |
| A | `Board`（データ配列・座標変換・衝突判定・固定）＋ `Cell` ＋ `GridPos` |
| B | `BlockShape`（形テーブル）＋ `FallingBlock`（入力・回転・自動落下・出現） |
| C | `PuzzleConfig`（定数管理）＋ `GameScene` 統合 ＋ 描画 ＋ デバッグUI |

境界インターフェース（合意済み）：

- `struct GridPos { int x; int y; };`
- `Board::CanPlace(const std::vector<GridPos>&)`
- `Board::Place(const std::vector<GridPos>&, int blockId)`
- `Board::GetCell(int, int)`
- `FallingBlock::GetOccupiedCells()`
- `FallingBlock::GetBlockId()`
- `BlockShape::GetCells(Type, int rotation)`

---

## 未使用リソースの整理（削除作業は未実施）

2026-08-31 調査。コードから参照されているのは以下のみと確認。

**使用中**：`Fence/fence.obj`（+mtl+png）、`Skybox/rostock_laage_airport_4k.dds`、`uvChecker.png`（ルート）、`You_and_Me.mp3`、`noise0.png` `noise1.png`、`level/` 一式（level.json / level.obj / circle.obj / levelCircle.obj + 各mtl + white.png）、`resource.h` `MyGameEngine.rc`、`defaultBlock/`（defaultBlock.obj / .mtl。盤面の3D描画で使用）

**削除候補（テンプレート由来で未参照）**：`AnimatedCube/` `Circle/` `Plane/` `Sphere/` `Terrain/` `human/` `simpleSkin/` `levelCircle/` の各フォルダ、`You_and_Me.wav`、`circle.png` `circle2.png` `gradationLine.png`

※ `level/white.png` は当初 `Board` のスプライト着色に使っていたが、3D描画への切り替えで参照しなくなった。level 側で使っていなければ削除候補に移せる（要確認）。

---

## 作業ログ

### 2026-08-31

**決定：パズル基礎部分の設計と3人分担**
「配置まで（10×10盤面にブロックを落として積める状態）」を当面のゴールに設定。担当A＝盤面、担当B＝ブロック、担当C＝統合・描画・定数、で分割。境界インターフェースを先に合意（詳細は「3人分担とインターフェース」）。新規ファイルは `Game/puzzle/` に置く想定。

**作成：`Game/puzzle/PuzzleConfig.h`**
調整用定数の集約ヘッダ。`.vcxproj` / `.vcxproj.filters` への登録、インクルードディレクトリとフィルタの追加もこのときに実施。

**作成：盤面クラス（`Cell.h` / `Board.h` / `Board.cpp`）＋ `GameScene` への組み込み**
初版はスプライトで実装。10×10 の各マスを `Sprite` で描き、白テクスチャ `resource/level/white.png` に色を乗算して空マス＝暗いグレー／埋マス＝明るいグレーに着色。外周は壁スプライト1枚。`GridToScreen()` でマス座標→スクリーン座標変換。`GameScene` では、盤面はスプライトなので3D描画のあと最前面に描画していた。Debug/x64 でビルド成功。

**変更：スプライト → 3Dモデル描画に切り替え**
「見た目は2Dだが描画は3D表現」と方針確定。`Sprite` を `Obj3D` に置換し、`Initialize(Obj3dCommon*)` に変更。ピクセル単位のレイアウト定数（`kCellSize` `kBoardOriginX/Y` `kCellGap`）を廃止し、ワールド単位の `kCellWorldSize` `kCellModelScale` `kBoardCenterZ` に置き換え。`GridToScreen()` → `GridToWorld()`。壁は背面の薄い板1枚に。

**変更：背面パネルを廃止し、外周をU字の壁ブロックに**
背面の板（背景）を削除し、マス領域の外側を左・下・右の壁ブロックで囲む形（上辺は開けたU字）に変更。`wallObj_`（単一パネル）→ `wallObjs_`（vector）＋ `CreateWallBlock(x,y)`。パネル用定数 `kWallMargin` `kWallThickness` `kWallDepthOffset` を削除。

**変更：10×10の空マスキューブを廃止**
空マスを埋めていた `cellObjs_` を全削除。関連する `RefreshCellColors()`・`kEmptyCellColor`・`kFilledCellColor` も削除。`cells_` は論理データとして残す。これにより `Board` が描画するのはU字の壁のみになった。Debug/x64 でビルド成功。

**調査：resource フォルダの未使用リソース洗い出し**
削除候補を特定（詳細は「未使用リソースの整理」）。削除作業自体は未実施。
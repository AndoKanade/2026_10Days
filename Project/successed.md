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
| `Game/puzzle/GridPos.h` | マス座標の共通型 `struct GridPos { int32_t x; int32_t y; };`。境界インターフェースの受け渡しに使う。 |
| `Game/puzzle/BlockShape.h` / `BlockShape.cpp` | 形テーブル。`enum class Type { L, T }` ＋ `GetCells(Type, int rotation)`。**中身はスタブ（空配列を返す）。担当B実装予定。** |
| `Game/puzzle/FallingBlock.h` / `FallingBlock.cpp` | 落下中ブロック。`GetOccupiedCells()` ＋ `GetBlockId()`。**中身はスタブ。担当B実装予定。** |
| `Game/puzzle/Board.h` / `Board.cpp` | 10×10 のマスデータ配列＋U字の壁の描画。`CanPlace()` / `Place()` の宣言を追加（**中身はスタブ。担当A実装予定**）。 |
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
- **描画しているもの**：U字の壁ブロック（`wallObjs_` ＋ `CreateWallBlock(x, y)`、色 `kWallColor`）と、盤面に固定されたマス（`cellObjs_` ＋ `RebuildCellObjects()`、色 `kFilledCellColor`）。`RebuildCellObjects()` は `Place()` のたびに `cells_` から作り直す。
- **API**：`Initialize(Obj3dCommon*)` / `Update()` / `Draw()` / `GridToWorld()` / `IsInside()` / `SetWidth()` / `CanPlace()` / `Place()`
  - `CanPlace(cells)`：全マスが範囲内かつ空きなら true。
  - `Place(cells, blockId)`：各マスに blockId を書き込み、`RebuildCellObjects()` を呼ぶ。
  - `GridToWorld()`：マス座標 → ワールド座標（`Vector3`、y=0 が上）。負座標・範囲外座標もそのまま計算できるので、壁は grid 座標 x=-1 / x=width / y=height に配置している。
  - `SetWidth()`：6/10 切り替え用。口だけ実装済みで UI 連携は未実装。
- 前方宣言した `Obj3D` を `unique_ptr` で持つため、コンストラクタ／デストラクタは cpp 側で `= default` 定義。

### `BlockShape` / `FallingBlock` の現状

- `BlockShape::GetCells(Type, rotation)`：T字の4回転テーブルを実装済み。L字はまだ空配列（担当B予定）。基準(0,0)は横棒の中央マス。
- `FallingBlock`：`Spawn` / `Update` / `MoveLeft` / `MoveRight` / `Rotate` / `SetSoftDrop` / `GetOccupiedCells` / `GetType` / `GetBlockId`。自動落下・固定猶予・衝突時拒否の回転を実装。壁蹴り（押し戻し）は未実装。
- `GameScene`：`fallingBlock_` を持ち、WASD（A/D=左右、W=回転、S=加速落下）で操作。固定されたら次のT字を自動出現。出現不可なら `isGameOver_`（現状は出現停止のみ。天井警告やリザルト遷移は未実装）。

### ビルド・動作確認

Debug/x64 でビルド成功（`MyGameEngine.exe` 生成確認）。タイトル画面でスペースキーを押すと GAME シーンに入り、盤面とT字ブロックが見える想定。
**実機での見た目・操作確認はまだ。**

---

## 今後の予定

### 「配置まで」に残っている作業

**共通の土台（役割分担の前提）は用意済み。** `GridPos.h`、`BlockShape` / `FallingBlock` のヘッダ雛形、`Board::CanPlace` / `Place` の宣言を作り、スタブ実装でビルドが通る状態にした。3人が並行して中身を埋められる。

1. ~~`Game/puzzle/GridPos.h`~~ **完了。**
2. `Game/puzzle/BlockShape.h/.cpp`：**T字は完了**（4回転テーブル実装）。L字の形テーブルが未実装。
3. `Game/puzzle/FallingBlock.h/.cpp`：**自動落下・左右移動・回転（拒否方式）・次ブロック出現は完了**。壁蹴りは未実装。
4. 落下中ブロック・盤面に置かれたブロックの3D描画：**単色で完了**。種類ごとの色分けは未対応（`Cell` が種類を持たないため）。
5. `Board` の `CanPlace` / `Place` ＋ 着地固定：**完了**。天井到達（ゲームオーバー）判定は未実装（現状は出現不可で停止するのみ）。
6. `GameScene` に `FallingBlock` を組み込み、キー入力（WASD）を接続：**完了**。
7. デバッグUI：盤面幅 6/10 の切り替え。担当C。**未着手。**

### 次にやるステップ

3人分担へ移行。各担当が上記2〜7のスタブ／宣言を埋める。

### 「配置まで」に含めないもの（Day2以降）

配線描画、通電BFS、消去、最強マス、連鎖、状態機械、スコア、タイトル/リザルト。

---

## 3人分担とインターフェース

| 担当 | 範囲 |
| --- | --- |
| A | `Board`（データ配列・座標変換・衝突判定・固定）＋ `Cell` ＋ `GridPos` |
| B | `BlockShape`（形テーブル）＋ `FallingBlock`（入力・回転・自動落下・出現） |
| C | `PuzzleConfig`（定数管理）＋ `GameScene` 統合 ＋ 描画 ＋ デバッグUI |

境界インターフェース（合意済み・**ヘッダとスタブ実装をコードに反映済み。ビルド通過**）：

- `struct GridPos { int32_t x; int32_t y; };`（`GridPos.h`。プロジェクトの他コードに合わせ `int32_t`）
- `Board::CanPlace(const std::vector<GridPos>&) const` → スタブは常に `false`
- `Board::Place(const std::vector<GridPos>&, int32_t blockId)` → スタブは何もしない
- `Board::GetCell(int32_t, int32_t)` → 実装済み
- `FallingBlock::GetOccupiedCells() const` → スタブは空配列
- `FallingBlock::GetBlockId() const` → スタブは初期値（空ID）
- `BlockShape::GetCells(BlockShape::Type, int32_t rotation)` → スタブは空配列（`BlockShape` は名前空間）

---

## 未使用リソースの整理（削除作業は未実施）

2026-08-31 調査。コードから参照されているのは以下のみと確認。

**使用中**：`Fence/fence.obj`（+mtl+png）、`Skybox/rostock_laage_airport_4k.dds`、`uvChecker.png`（ルート）、`You_and_Me.mp3`、`noise0.png` `noise1.png`、`level/` 一式（level.json / level.obj / circle.obj / levelCircle.obj + 各mtl + white.png）、`resource.h` `MyGameEngine.rc`、`defaultBlock/`（defaultBlock.obj / .mtl。盤面の3D描画で使用）

**削除候補（テンプレート由来で未参照）**：`AnimatedCube/` `Circle/` `Plane/` `Sphere/` `Terrain/` `human/` `simpleSkin/` `levelCircle/` の各フォルダ、`You_and_Me.wav`、`circle.png` `circle2.png` `gradationLine.png`

※ `level/white.png` は当初 `Board` のスプライト着色に使っていたが、3D描画への切り替えで参照しなくなった。level 側で使っていなければ削除候補に移せる（要確認）。

---

## 作業ログ

### 2026-09-02（2）

**実装：T字ブロックの出現・操作・落下・盤面固定**
「まずT字を落として積める」ところまで。

- `BlockShape.cpp`：T字（Tテトロミノ・4マス）の4回転分の相対座標テーブルを実装。基準(0,0)は横棒の中央マスで回転しても動かない。`GetCells()` は回転indexを0〜3に丸めて `switch(type)` で引く。L字はまだ空を返す（担当B予定）。
- `FallingBlock.h/.cpp`：`Spawn()` / `Update()` / `MoveLeft()` / `MoveRight()` / `Rotate()` / `SetSoftDrop()` / `GetType()` を追加。`Update()` は「真下に置けるか」で空中/着地を判定し、空中は落下間隔ごとに1マス落下、着地は `kLockDelayFrames` の固定猶予を毎フレーム進め、猶予切れで `Board::Place()` して true を返す。移動・回転が成功すると固定猶予をリセット。回転は重なったら拒否（押し戻しなし）。出現位置は `kSpawnColumn` の y=1。
- `Board.cpp`：`CanPlace()`（範囲内かつ空きマスか）と `Place()`（blockId書き込み＋`RebuildCellObjects()`）を実装（スタブ→本実装）。`cellObjs_` と `RebuildCellObjects()` を追加し、固定されたマスをキューブで描画（色 `kFilledCellColor`）。`Update()` / `Draw()` に `cellObjs_` のループを追加。
- `GameScene`：`FallingBlock fallingBlock_` ＋ `fallingObjs_`（マス数分のキューブ、色 `kFallingBlockColor`）＋ `nextBlockId_` ＋ `isGameOver_` を追加。`Initialize()` で最初のT字を出現。`Update()` で WASD 入力（A/D=左右、W=回転、S=加速落下）→ `fallingBlock_.Update()` → 固定されたら次のT字を出現（置けなければ `isGameOver_`）→ `SyncFallingObjs()` で描画位置を同期。`Draw()` で落下ブロックを描画。
- 操作キー：A/D/W/S。元ブロックIDは出現ごとに 0 から連番。
- Debug/x64 ビルド成功。実機での見た目確認はまだ。
- 既存の「スペースキーで GAMECLEAR へ遷移」はそのまま残している。

### 2026-09-02

**作成：共通の土台（役割分担の前提）**
3人分担に入る前の共通部分だけを実装。`Game/puzzle/` に以下を追加した。

- `GridPos.h`：マス座標の共通型。合意インターフェースは `int` だが、プロジェクトの他コード（`Board::GetCell` など）が `int32_t` なので合わせた。
- `BlockShape.h` / `BlockShape.cpp`：名前空間 `BlockShape` に `enum class Type { L, T }`、`kRotationCount`、`GetCells(Type, int32_t rotation)`。中身はスタブで、`.cpp` 内の空配列を const 参照で返すだけ。担当Bが形テーブルを実装する。
- `FallingBlock.h` / `FallingBlock.cpp`：`FallingBlock` クラス。private に `type_` / `rotation_` / `origin_` / `blockId_`。public は合意済みの `GetOccupiedCells()`（空配列を返す）と `GetBlockId()`（`blockId_` をそのまま返す）のみ。落下・移動・回転・出現は担当Bが追加する。
- `Board.h` / `Board.cpp`：`CanPlace(const std::vector<GridPos>&) const` と `Place(const std::vector<GridPos>&, int32_t)` の宣言を追加。変更箇所は「追加：」コメントで明示。スタブは `CanPlace` が常に `false`、`Place` が空実装。担当Aが中身を実装する。

`.vcxproj` / `.vcxproj.filters` に3ヘッダ＋2ソースを登録（フィルタは既存の「Game\Puzzle」）。Debug/x64 でビルド成功（0 エラー、警告は既存分のみ）。実機動作は未確認（描画に関わる変更はないため見た目の変化なし）。

これで A/B/C が同じインターフェースに対して並行着手でき、常にビルドが通る状態になった。

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
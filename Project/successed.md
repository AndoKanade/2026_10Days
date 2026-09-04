# successed.md

10daysJam「つなぐ」で新しくできたこと・作ったものの記録。

**書き方のルール**
- 新しい作業が終わるたびに「作業ログ」に日付ごとに追記する。
- 仕様が変わったら「現在の状態」を**上書き**する。変わった理由・経緯はログ側に残す。
- 「現在の状態」だけ読めば今どうなっているかが分かる、を維持する。

---

## 現在の状態

現在の到達点：**10×10 盤面にブロックが落ちて積め、配線が描画され、電源からゴールまで繋がると通電マスがまとめて消え、落下して連鎖する**ところまで実装済み。
（全体の開発ロードマップは `CLAUDE.md` の「4. 開発ロードマップ」を参照）

未実装の主な項目：最強マス変換、明示的な状態機械への整理、スコア計算・表示、天井警告や消去エフェクトなどの演出、盤面幅 6/10 の切り替えUI。

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
| `Game/puzzle/Cell.h` | マス構造体。`blockId`（空 = -1）＋端子4bit `terminals`（マス同士の通電判定に使用）＋壁判定専用の `wallTerminals` ＋ `IsEmpty()`。 |
| `Game/puzzle/GridPos.h` | マス座標の共通型 `struct GridPos { int32_t x; int32_t y; };`。境界インターフェースの受け渡しに使う。 |
| `Game/puzzle/BlockShape.h` / `BlockShape.cpp` | 形テーブル。`enum class Type { L, T, I, J }` ＋ 種類数 `kTypeCount` ＋ `GetCells()` / `GetTerminals()` / `GetWallTerminals()`。**T字・L字・I字・J字とも4回転分を実装済み。** |
| `Game/puzzle/FallingBlock.h` / `FallingBlock.cpp` | 落下中ブロック。出現・自動落下・左右移動・回転（拒否方式）・ハードドロップ・着地予測を実装済み。 |
| `Game/puzzle/Board.h` / `Board.cpp` | 10×10 のマスデータ配列＋U字の壁の描画＋配線描画。`CanPlace()` / `CanFall()` / `Place()` ＋ 通電判定BFS・消去演出・マス単位落下・連鎖ループを実装済み。 |
| `Game/scenes/GameScene` | `Board board_;` をメンバに持ち、Initialize / Update / Draw から呼び出す。変更箇所は「追加：」コメントで明示。 |

`.vcxproj` / `.vcxproj.filters` に登録済み。`Game\puzzle` をインクルードディレクトリに追加（3構成すべて）、フィルタ「ヘッダー ファイル\Game\Puzzle」を新設。

### `PuzzleConfig.h` の定数

- 盤面サイズ：10×10（狭い盤面の候補として 6）
- `kCellWorldSize`：マス中心間の距離 1.0
- `kCellModelScale`：キューブ拡大率 0.45
- `kBoardCenterZ`
- 落下間隔・固定猶予（フレーム単位、60fps 固定前提）
- 出現列 `kSpawnColumn`、出現行 `kSpawnRow`（= -2、天井より上の空中）、空マスID

### `Board` の現状

- **持っているデータ**：`cells_`（10×10 のマス配列。`blockId` ＋ `terminals` ＋ `wallTerminals`）。論理データとして保持し、`GetCell()` で参照する。消去演出用に `clearingCells_` / `isClearing_` / `clearTimer_` も持つ。
- **描画しているもの**：
  - U字の壁ブロック（`wallObjs_` ＋ `CreateWallBlock(x, y, modelPath)`）。左右の壁＝ゴールなので `goalBlock/goalBlock.obj`、下の壁＝電源なので `supplyBlock/supplyBlock.obj`（角は下扱い）。どちらも `enableLighting=0` でテクスチャそのまま表示（色の乗算はしない）。
  - 盤面に固定されたマスと、その配線（`cellObjs_` ＋ `RebuildCellObjects()`、モデル `defaultBlock`）。`RebuildCellObjects()` は `Place()` と消去・落下のたびに `cells_` から作り直す。マスの色は 消去演出中（`kClearingCellColor`）＞通電中（`kPoweredCellColor`）＞通常（`kFilledCellColor`）の優先順。配線は端子が立っている方向だけ中心から辺へ細い棒で描き、通電中は `kWireLitColor`、非通電は `kWireUnlitColor`。
- **API**：`Initialize(Obj3dCommon*)` / `Update()` / `Draw()` / `GridToWorld()` / `IsInside()` / `SetWidth()` / `CanPlace()` / `CanFall()` / `Place()` / `IsBusy()`
  - `CanPlace(cells)`：全マスが範囲内かつ空きなら true。現状は未使用（一般用途の判定として保持）。
  - `CanFall(cells)`：落下中ブロック専用。天井より上（y<0）は空中として通し、左右の壁・床・既存ブロックの重なりだけ不可。`FallingBlock` はこちらを使う。
  - `Place(cells, blockId, terminals, wallTerminals)`：各マスに blockId・端子ビット・壁の先端ビットを書き込み（範囲外マスはスキップ）、`ResolveConduction()` で通電判定を行ってから `RebuildCellObjects()` を呼ぶ。
  - `GridToWorld()`：マス座標 → ワールド座標（`Vector3`、y=0 が上）。負座標・範囲外座標もそのまま計算できるので、壁は grid 座標 x=-1 / x=width / y=height に配置している。
  - `SetWidth()`：6/10 切り替え用。口だけ実装済みで UI 連携は未実装。
  - `IsBusy()`：消去演出中かどうか。`GameScene` はこれが true の間ブロックの操作・落下・出現を止める。
- **通電・消去・落下（内部処理）**：
  - `ComputePoweredMask()`：電源（最下段）から幅優先探索で、いまどこまで通電が届いているかを調べる（ゴール到達は問わない）。配線・マスの光らせ表示に使う。
  - `ResolveConduction()`：電源から幅優先探索で一かたまりを集め、ゴール（左右端の `wallTerminals`）まで届いていれば、行き止まりの枝を取り除いた残りのマスを消去対象にする。枝分かれ・ループは残す。即座には消さず消去演出状態に入る。
  - `ApplyGravity(clearedColumns)`：消去が起きた列だけを対象に、マス単位で下詰めする（ブロックの形は保持しない）。
  - `Update()` は消去演出タイマーが満了したら 実際の消去 → `ApplyGravity` → `ResolveConduction`（再判定）→ `RebuildCellObjects` を行い、通電がなくなるまで連鎖する。
- 前方宣言した `Obj3D` を `unique_ptr` で持つため、コンストラクタ／デストラクタは cpp 側で `= default` 定義。

### `BlockShape` / `FallingBlock` の現状

- `BlockShape`：`GetCells()` / `GetTerminals()` / `GetWallTerminals()` を T字・L字・I字・J字とも4回転分実装済み（いずれも4マスのテトロミノ）。基準(0,0)は棒の中央マス（I字は左から2番目／上から2番目のマス）。J字は L字の左右反転。I字は180度回すと同じ形になるため rot0＝rot2、rot1＝rot3。端子ビットは形テーブルの隣接関係から計算し、先端のマスは反対側の辺も露出させて別ブロックの先端と繋がれるようにする。`GetWallTerminals()` は「本来の先端」だけを残した壁（ゴール）・床（電源）到達判定専用のビット（T字は出っ張りのみ、L字・I字・J字は1本道なので両端）。種類の総数は `kTypeCount`（現在4）。
- `FallingBlock`：`Spawn` / `Update` / `HardDrop` / `MoveLeft` / `MoveRight` / `Rotate` / `SetSoftDrop` / `GetOccupiedCells` / `GetLandingCells` / `GetType` / `GetBlockId` / `IsLockedAboveCeiling`。自動落下・固定猶予・衝突時拒否の回転・ハードドロップを実装。固定時は `Board::Place` に端子ビット・壁の先端ビットも渡す。出現は `kSpawnRow`（= -2、天井より上の空中）から。衝突判定は `Board::CanFall`。固定時に y<0 のマスが残れば `IsLockedAboveCeiling()` が true。`GetLandingCells()` はいま真下に落とした場合の着地マス（ゴースト表示用）。壁蹴り（押し戻し）は未実装。
- `GameScene`：`fallingBlock_` を持ち、WASD（A/D=左右、W=回転、S=加速落下）＋ Enter でハードドロップ。消去演出中（`board_.IsBusy()`）は操作・落下・出現を止める。`nextType_` でネクストを管理し、`SpawnNextBlock()` が「ネクストを出現 → 次のネクストを抽選」を行う。`PickNextBlockType()` は `BlockShape::kTypeCount` を使って全種類（L字・T字・I字・J字）を等確率抽選（`randomEngine_`）。固定されたら自動で次を出現。固定時に `IsLockedAboveCeiling()` が true（または出現不可）なら `SceneManager::ChangeScene("GAMEOVER")` へ遷移。落下中ブロック（`fallingObjs_`）と着地予測ゴースト（`ghostObjs_`、暗色・少し小さめ）を描画。通電・消去の可視化は `Board` 側が担当する。ネクストは ImGui「GameScene Debug」→「Next Block」に等幅テキストのグリッドで表示。天井警告演出は未実装。

### ビルド・動作確認

Debug/x64 でビルド成功（`MyGameEngine.exe` 生成確認）。タイトル画面でスペースキーを押すと GAME シーンに入り、盤面・落下ブロック・配線・通電と消去が見える想定。
**実機での見た目・操作・当たり判定バランスの確認はまだ。**

---

## 今後の予定

### 完了済み

1. ~~`Game/puzzle/GridPos.h`~~ **完了。**
2. `Game/puzzle/BlockShape.h/.cpp`：**T字・L字・I字・J字とも完了**（各4回転テーブル＋端子ビット＋壁の先端ビット。いずれも4マスのテトロミノ）。
3. `Game/puzzle/FallingBlock.h/.cpp`：**自動落下・左右移動・回転（拒否方式）・ハードドロップ・次ブロック出現＋ネクスト抽選は完了**。壁蹴りは未実装。
4. 落下中ブロック・盤面に置かれたブロックの3D描画＋配線描画：**完了**。種類ごとの色分けは未対応（`Cell` が種類を持たないため）。
5. `Board` の `CanPlace` / `CanFall` / `Place` ＋ 着地固定：**完了**。天井到達（ゲームオーバー）判定も**完了**（天井より上に残って固定、または次ブロックが出現位置に置けなければ GAMEOVER シーンへ遷移）。
6. `GameScene` に `FallingBlock` を組み込み、キー入力（WASD＋Enter）を接続：**完了**。
7. 通電判定BFS・配線の光らせ表示・通電成立時の消去演出・マス単位の落下・連鎖ループ：**完了**。

### 残っている作業

- デバッグUI：盤面幅 6/10 の切り替え。**未着手。**
- 最強マス変換（消去後に同じ元ブロックIDが1マスだけ残ったら全方向端子のマスにする。判定は落下前）。**未着手。**
- 即時処理で書いている進行を、明示的な状態機械へ整理する。**未着手。**
- スコア計算（通電マス数に応じた倍率テーブル）・連鎖倍率・スコア表示。**未着手。**
- 演出：通電が経路を走るアニメーション、消去エフェクト、天井接近時の警告。**未着手。**
- タイトル／リザルト画面の体裁と、タイトルへの復帰・リスタート。**未着手。**

---

## 3人分担とインターフェース

| 担当 | 範囲 |
| --- | --- |
| A | `Board`（データ配列・座標変換・衝突判定・固定）＋ `Cell` ＋ `GridPos` |
| B | `BlockShape`（形テーブル）＋ `FallingBlock`（入力・回転・自動落下・出現） |
| C | `PuzzleConfig`（定数管理）＋ `GameScene` 統合 ＋ 描画 ＋ デバッグUI |

境界インターフェース（合意済み・**すべて本実装済み。ビルド通過**）：

- `struct GridPos { int32_t x; int32_t y; };`（`GridPos.h`。プロジェクトの他コードに合わせ `int32_t`）
- `Board::CanPlace(const std::vector<GridPos>&) const` → 実装済み（現状は未使用）
- `Board::CanFall(const std::vector<GridPos>&) const` → 実装済み（落下中ブロックの衝突判定はこちらを使う）
- `Board::Place(const std::vector<GridPos>&, int32_t blockId, const std::vector<uint8_t>& terminals, const std::vector<uint8_t>& wallTerminals)` → 実装済み（固定と同時に通電判定を行う）
- `Board::GetCell(int32_t, int32_t)` → 実装済み
- `FallingBlock::GetOccupiedCells() const` → 実装済み
- `FallingBlock::GetBlockId() const` → 実装済み
- `BlockShape::GetCells / GetTerminals / GetWallTerminals(BlockShape::Type, int32_t rotation)` → 実装済み（`BlockShape` は名前空間）

---

## 未使用リソースの整理（削除作業は未実施）

2026-08-31 調査。コードから参照されているのは以下のみと確認。

**使用中**：`Fence/fence.obj`（+mtl+png）、`Skybox/rostock_laage_airport_4k.dds`、`uvChecker.png`（ルート）、`You_and_Me.mp3`、`noise0.png` `noise1.png`、`level/` 一式（level.json / level.obj / circle.obj / levelCircle.obj + 各mtl + white.png）、`resource.h` `MyGameEngine.rc`、`defaultBlock/`（固定マス・落下ブロック・ゴーストの描画）、`goalBlock/`（左右の壁）、`supplyBlock/`（下の壁）

**削除候補（テンプレート由来で未参照）**：`AnimatedCube/` `Circle/` `Plane/` `Sphere/` `Terrain/` `human/` `simpleSkin/` `levelCircle/` の各フォルダ、`You_and_Me.wav`、`circle.png` `circle2.png` `gradationLine.png`

※ `level/white.png` は当初 `Board` のスプライト着色に使っていたが、3D描画への切り替えで参照しなくなった。level 側で使っていなければ削除候補に移せる（要確認）。

---

## 作業ログ

### 2026-09-04（2）

**追加：ブロックの種類に J字（L字の左右反転）を追加**

- `BlockShape::Type` に `J` を追加し、`kTypeCount` を 3 → 4 に更新。
- `kJShapeCells` を4回転分追加。基準(0,0)は縦棒／横棒の中央マスで、L字と同じ取り方。rot0＝縦棒＋左下の足、rot1＝横棒＋左上の足、rot2＝縦棒＋右上の足、rot3＝横棒＋右下の足（rot0 から時計回り）。
- 端子ビット `kJShapeTerminals`・壁の先端ビット `kJShapeWallTerminals` を追加。J字は L字の鏡像で枝分かれのない1本道なので、`ComputeWallTerminals()` では L字・I字と同じく全マスをそのまま通す。
- `GetCells()` / `GetTerminals()` / `GetWallTerminals()` の switch に `Type::J` の分岐を追加。
- ネクスト表示（ImGui）の種類名 switch に `J` を追加。抽選側は `kTypeCount` を見ているため変更不要。

これで左右どちらの壁も同じくらい狙いやすくなった。Debug/x64 でビルド成功。実機での見た目・難易度の確認はまだ。

### 2026-09-04

**追加：ブロックの種類に I字（4マスの棒）を追加**

種類を増やすにあたり、O字（2x2）は全マスが degree 2 の閉ループになって先端が1つも生まれず、壁にも他ブロックにも繋がらないため候補から除外した。S字・Z字は段差ができて積みにくく難易度調整が必要になるため今回は見送り、ロードマップ Day 6 に挙げていた I字だけを追加した。

- `BlockShape::Type` に `I` を追加し、種類の総数を表す `kTypeCount`（現在3）を新設。
- `kIShapeCells` を4回転分追加。基準(0,0)は棒の左から2番目／上から2番目のマス（横棒のとき基準の左右に1マスと2マス伸びる形で、出現列からはみ出しにくい）。I字は180度回すと同じ形になるため rot0＝rot2、rot1＝rot3 の同じ内容。
- 端子ビット `kIShapeTerminals`・壁の先端ビット `kIShapeWallTerminals` を追加。I字は L字と同じく枝分かれのない1本道なので、`ComputeWallTerminals()` では全マスをそのまま通す（両端が壁・床の先端になる）。
- `GetCells()` / `GetTerminals()` / `GetWallTerminals()` の switch に `Type::I` の分岐を追加。
- `GameScene::PickNextBlockType()` を、`0 or 1` の直書きから `BlockShape::kTypeCount` を使った全種類の等確率抽選（`static_cast<BlockShape::Type>`）に変更。以後、種類を足したときに直すのは `Type` と `kTypeCount` だけで済む。
- ネクスト表示（ImGui）の種類名 switch に `I` を追加。

Debug/x64 でビルド成功。実機での見た目・難易度の確認はまだ。

### 2026-09-03

**まとめ：通電・消去・連鎖・配線描画をコードに反映（ログ未記載分）＋ドキュメント更新**

（8）以降、ログに残さないまま以下が実装されていた。コードを正としてこの日付でまとめ、「現在の状態」「今後の予定」「3人分担とインターフェース」を実装に合わせて上書きした。

- `Cell` に壁（ゴール）・床（電源）到達判定専用の `wallTerminals` を追加。`terminals`（マス同士の通電判定用）と役割を分けた。
- `BlockShape` に `GetTerminals()` / `GetWallTerminals()` を追加。端子ビットは形テーブルの隣接から計算し、先端のマスは反対側の辺も露出させる。壁の先端は形ごとに固定（T字は出っ張りのみ、L字は両端）。
- `Board::Place` の引数に `terminals` / `wallTerminals` を追加。固定と同時に `ResolveConduction()` を呼ぶ。
- `Board` に通電判定BFS（`ResolveConduction` ＝ 電源→ゴールの一かたまりから行き止まりの枝を除いた残りを消去対象に、`ComputePoweredMask` ＝ 光らせ表示用）、消去演出（`clearingCells_` / `isClearing_` / `clearTimer_` ＋ `kClearEffectFrames`）、マス単位の落下（`ApplyGravity`、消去が起きた列のみ）、通電がなくなるまでの連鎖ループ（`Update` 内）を実装。
- `RebuildCellObjects()` が配線（端子方向の細い棒）も生成するようにした。色は 消去演出中＞通電中＞通常 の優先順、配線は通電中のみ明るい色。
- `Board::IsBusy()` を追加。`GameScene` は消去演出中はブロックの操作・落下・出現を止める。
- `FallingBlock::HardDrop()` を追加。`GameScene` で Enter に割り当て。
- ゲームオーバー判定に「天井より上に残って固定（`IsLockedAboveCeiling`）」を追加。

### 2026-09-02（8）

**変更：壁の描画モデルを goalBlock / supplyBlock に差し替え**

- `resource/goalBlock/`（obj+mtl+png）と `resource/supplyBlock/`（同）を新規追加（モデルは別途作成済み）。
- `Board`：`CreateWallBlock(x, y)` に `const std::string& modelPath` 引数を追加。`Initialize` で左右の壁は `goalBlock/goalBlock.obj`、下の壁は `supplyBlock/supplyBlock.obj` を指定（下の角2つは下の壁扱い）。3モデルとも `Initialize` で `LoadModel`。
- 壁の色乗算（旧 `kWallColor`）を廃止。`enableLighting=0` のみ残し、モデルのテクスチャをそのまま表示する。`kWallColor` 定数は削除。
- 盤面に固定されたマス（`cellObjs_`）は従来どおり `defaultBlock` ＋ `kFilledCellColor`。
- Debug/x64 ビルド成功。実機での見た目確認はまだ。

### 2026-09-02（7）

**実装：着地予測（ゴースト）表示「ここに落とすとこうなる」**

- `FallingBlock::GetLandingCells(const Board&)` を追加。現在の基準座標・回転のまま、`Board::CanFall` が通らなくなるまで真下に下げ、着地位置の占有マスを返す（盤面は変更しない）。
- `GameScene` に `ghostObjs_`（本体と同じマス数の Obj3D プール）を追加。色 `kGhostBlockColor`（落下色を暗くした影っぽい色）、拡大率は `kCellModelScale * kGhostScaleRate`（`kGhostScaleRate` = 0.7、本体より少し小さく）。
- `SyncFallingObjs()` で本体に加えてゴーストも `GetLandingCells()` の位置へ毎フレーム同期。`Update()` でゴーストも行列更新、`Draw()` はゴースト → 本体の順（本体が上に重なる）。ゲームオーバー中は非表示。
- Obj3D パイプラインはアルファブレンド無効のため、半透明ではなく暗い色＋小さめスケールで区別している。
- Debug/x64 ビルド成功。実機確認はまだ。

### 2026-09-02（6）

**変更：ブロックの出現位置を天井より上の空中からに**

- `PuzzleConfig::kSpawnRow`（＝ -2）を追加。負値＝盤面上端 y=0 より上の空中。`FallingBlock::Spawn` の出現基準を `{kSpawnColumn, 1}`（マジックナンバー）から `{kSpawnColumn, kSpawnRow}` に変更。
- `Board::CanFall(cells)` を追加。落下中ブロック専用の判定で、天井より上（y<0）は空中として通し、左右の壁（x範囲外）・床（y>=高さ）・既存ブロックの重なりだけ不可とする。`FallingBlock` の Spawn / MoveLeft / MoveRight / Rotate / Update の落下判定を `CanPlace` から `CanFall` に差し替え。`Board::CanPlace` は残置（未使用だが一般用途の判定として保持）。
- ゲームオーバー判定を変更。出現位置が常に空中になったため「Spawn 失敗」ではほぼ検出できない。代わりに `FallingBlock::IsLockedAboveCeiling()` を追加し、固定時に占有マスが1つでも y<0 なら true。`GameScene` は固定検出時にこれを見て（Spawn失敗も保険で併用）`GAMEOVER` へ遷移。
- 出現直後のブロックは盤面（U字の壁）より上に描画され、落下して視界に入ってくる。
- Debug/x64 ビルド成功。実機確認はまだ。

### 2026-09-02（5）

**実装：L字ブロック（Lテトロミノ・4マス）の追加**

- `BlockShape.cpp`：`kLShapeCells`（4回転分の相対座標テーブル）を追加し、`GetCells()` の `Type::L` で返すようにした。基準(0,0)は縦棒／横棒の中央マス。rot0＝縦棒＋右下の足、以降時計回り。T字と同じくrot0で最上段（y=-1）にマスが来るため、出現基準 y=1 で天井に接して出現する。
- `GameScene::PickNextBlockType()`：T字・L字を等確率（`uniform_int_distribution<int32_t>(0,1)`）で抽選するように変更。`const` を外し、メンバ乱数エンジン `randomEngine_`（`Initialize` で `random_device` シード、`ParticleManager` と同じ流儀）を使う。
- 落下ブロックの色は T/L 共通（種類ごとの色分けはしていない）。`fallingObjs_` は4マス分のプールで T/L どちらも足りる。
- 補足：CLAUDE.md の「L字（3マス）／T字（3マス）」は記載当時の想定。実装は L・T とも4マスのテトロミノになっている。
- Debug/x64 ビルド成功。実機確認はまだ。

### 2026-09-02（4）

**実装：ImGui にネクスト（次に落ちてくるブロック）表示＋ネクスト管理**
L字追加の前段として、次ブロックの仕組みと表示を用意。

- `GameScene` に `nextType_`（次に出るブロックの種類）を追加。
- `SpawnNextBlock()`：`nextType_` を `fallingBlock_.Spawn()` に渡して出現させ、`nextBlockId_` を進め、`PickNextBlockType()` で次の `nextType_` を抽選する。初回出現（`Initialize`）と固定後の出現（`Update`）の両方でこれを呼ぶ。
- `PickNextBlockType()`：現状はT字のみ対応のため常に `Type::T` を返す。L字の形テーブルを実装したらここで T/L をランダム抽選する。
- `ShowNextBlockGui()`：ImGui の「GameScene Debug」ウィンドウ先頭に「Next Block」ヘッダを追加。種類名（T/L）と、形テーブル（回転0）の外接矩形を等幅テキストで `[]` / 空白のグリッド表示する。`#ifdef USE_IMGUI` 内のみ。
- `nextBlockId_` の採番は 0 始まりのまま（`SpawnNextBlock` 内で Spawn 後にインクリメント）。
- `std::min` / `std::max` は Windows の min/max マクロと衝突するため使わず、if 比較で外接矩形を求めている。
- Debug/x64 ビルド成功。実機確認はまだ。

### 2026-09-02（3）

**実装：天井到達でゲームオーバーシーンへ遷移**
`GameScene::Update()` で、ブロック固定後の次のT字の `Spawn()` が失敗したとき（出現位置＝天井付近が既に埋まっている＝天井より上まで積み上がった）に `isGameOver_` を立て、`SceneManager::GetInstance()->ChangeScene("GAMEOVER")` を呼んで即 `return`。T字の出現基準は y=1 で、rot0 では最上段（y=0）にマスが来るため、中央列が天井付近まで積み上がると出現できずゲームオーバーになる。天井警告の点滅演出は未実装。

### 2026-09-02（2）

**実装：T字ブロックの出現・操作・落下・盤面固定**
「まずT字を落として積める」ところまで。

- `BlockShape.cpp`：T字（Tテトロミノ・4マス）の4回転分の相対座標テーブルを実装。基準(0,0)は横棒の中央マスで回転しても動かない。`GetCells()` は回転indexを0〜3に丸めて `switch(type)` で引く。L字はまだ空を返す（担当B予定）。
- `FallingBlock.h/.cpp`：`Spawn()` / `Update()` / `MoveLeft()` / `MoveRight()` / `Rotate()` / `SetSoftDrop()` / `GetType()` を追加。`Update()` は「真下に置けるか」で空中/着地を判定し、空中は落下間隔ごとに1マス落下、着地は `kLockDelayFrames` の固定猶予を毎フレーム進め、猶予切れで `Board::Place()` して true を返す。移動・回転が成功すると固定猶予をリセット。回転は重なったら拒否（押し戻しなし）。出現位置は `kSpawnColumn` の y=1。
- `Board.cpp`：`CanPlace()`（範囲内かつ空きマスか）と `Place()`（blockId書き込み＋`RebuildCellObjects()`）を実装（スタブ→本実装）。`cellObjs_` と `RebuildCellObjects()` を追加し、固定されたマスをキューブで描画（色 `kFilledCellColor`）。`Update()` / `Draw()` に `cellObjs_` のループを追加。
- `GameScene`：`FallingBlock fallingBlock_` ＋ `fallingObjs_`（マス数分のキューブ、色 `kFallingBlockColor`）＋ `nextBlockId_` ＋ `isGameOver_` を追加。`Initialize()` で最初のT字を出現。`Update()` で WASD 入力（A/D=左右、W=回転、S=加速落下）→ `fallingBlock_.Update()` → 固定されたら次のT字を出現（置けなければ `isGameOver_`）→ `SyncFallingObjs()` で描画位置を同期。`Draw()` で落下ブロックを描画。
- 操作キー：A/D/W/S。元ブロックIDは出現ごとに 0 から連番。
- Debug/x64 ビルド成功。実機での見た目確認はまだ。

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
**追加：フルスクリーン対応（ボーダーレスウィンドウ方式）**
`WinAPI` に `SetFullscreen()` / `ToggleFullscreen()` / `IsFullscreen()` を追加。フルスクリーン時は `WINDOWPLACEMENT` とウィンドウスタイルを保存してから `WS_POPUP` に変更し、`MonitorFromWindow()` で取得したモニタ全体に広げる。復帰時は保存した値を戻す。`WindowProc` は静的関数なので `static WinAPI* instance` を持たせ、`WM_KEYDOWN` の F11 と `WM_SYSKEYDOWN` の Alt+Enter で切り替える。`DXCommon::CreateSwapChain()` に `MakeWindowAssociation(DXGI_MWA_NO_ALT_ENTER)` を追加し、DXGI 側の自動フルスクリーン切り替えを無効化。スワップチェーンは 1280x720 のままで、`DXGI_SCALING_STRETCH` により画面サイズへ引き伸ばされるため描画側の変更は不要。Debug/x64 でビルド成功。

**追加：フルスクリーン時の画質低下を解消（描画解像度の動的リサイズ）**
描画が常に 1280x720 で行われ、DXGI が画面サイズへ引き伸ばしていたのが画質低下の原因。論理解像度と物理解像度を分離して解決した。論理解像度 1280x720（`WinAPI::kClientWidth/kClientHeight`）は据え置きで、Sprite の正射影行列と Camera のアスペクト比はこれを使い続けるため、Sprite・Camera・Game 配下のシーンコードは無変更。`WinAPI` に `WM_SIZE` の受け取りと `ConsumeResizeRequest()` を追加（1フレーム1回だけ拾うことでドラッグ中の連続 WM_SIZE を1回にまとめる）。`DXCommon` に `CalcResolution()`・`WaitForGPU()`・`Resize()` を追加し、スワップチェーンはウィンドウ全体、深度バッファと RenderTexture は「ウィンドウに収まる最大の16対9矩形」で作り直す。`PreDraw()` はオフスクリーン用とスワップチェーン用でビューポートを切り替え、後者は中央寄せのレターボックス矩形にしてクリア色を黒にした（余白が黒帯になる）。ディスクリプタは初回確保分を使い回す（深度SRVは `depthSrvIndex_`、RenderTexture は `Application` がハンドルを保持）。リサイズのたびに確保するとディスクリプタが枯渇するため。`Application::HandleResize()` を `Framework::Update()` 直後に呼ぶ。副次的に、フルスクリーンで ImGui の表示位置がずれる問題も解消。Debug/x64 でビルド成功。

# CloverPic

CloverPic は C++20 / MinGW-w64 ベースのペイントアプリ MVP です。現在の構成は **core-driven single-surface** です。core はプロジェクトモデル、レイヤー、ブラシ、UI scene、コマンド、ヒットテスト、フレームスケジューラ、RGBA フレーム生成を所有します。platform adapter はウィンドウ、入力変換、ファイル/画像/フォント/デバイス情報、最終表示だけを担当します。

core/adapter の契約は `docs/CoreAdapterAPI.md` を参照してください。

## 構成

```text
src/
  Core/               # プラットフォーム非依存のアプリ本体
  Platform/Windows/   # 最初の single-window adapter
  Utils/              # 共通基本型
```

`CloverPicCore` はプラットフォーム非依存の static library としてビルドされます。`CloverPic.exe` は Windows adapter とリンクされます。

## MVP 機能

- プリセットキャンバス作成、`.vvp` の開く/保存、PNG エクスポート。
- dirty-rect 対応の single-surface soft UI。
- キャンバスのズーム/パン、ブラシ、消しゴム、基本色切り替え。
- レイヤー選択、追加、削除、表示切り替え。
- Undo/Redo、最近使ったファイル、platform text rasterizer API。


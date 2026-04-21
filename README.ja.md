<p align="center">
  <img src="logo.svg" width="320" height="320" alt="VividPic Logo">
</p>

<h1 align="center">VividPic</h1>

<p align="center">
  <a href="README.md">中文</a> | <a href="README.en.md">English</a> | <a href="README.ja.md">日本語</a>
</p>

> ⚠️ **声明：このプロジェクトは個人の学習用プロジェクトであり、技術的な探求と練習のみを目的としています。最終的な商業製品の品質を示すものではありません。**

## 概要

VividPic は、イラストレーターおよび漫画家向けの純粋な C++ ネイティブ Windows ペイントソフトウェアです。自前の軽量 UI フレームワークとレンダリングパイプラインを採用し、Windows 10/11 上で低遅延・高忠実度の描画体験を提供することを目指しています。

現在活発に開発中で、基本キャンバスエンジン、レイヤーシステム、ブラシエンジン、およびデュアルスタックのタブレットサポートが既に実装されています。

## 主な機能

- **純粋なネイティブ実装**：Win32 API と Direct2D をベースに構築。Qt や Electron などの外部 UI フレームワークに依存しません。
- **自前のレンダリングパイプライン**：CPU ソフトコンポジット + D2D ディスプレイ表示。256×256 の TileGrid チャンク型メモリ管理で Copy-on-Write をサポート。
- **プロフェッショナルなレイヤーシステム**：8 種類のブレンドモード（Normal、Multiply、Screen、Overlay、Difference、Add、Subtract、Darken、Lighten）に加え、不透明度と表示/非表示の制御が可能。
- **ブラシエンジン**：5 種類のティップ（RoundHard、RoundSoft、Flat、Bristle、Texture）をサポート。flow、wetMix、spacing、筆圧マッピングに対応。
- **デュアルスタックのタブレットサポート**：Windows Ink API（優先）→ WinTab API（フォールバック）→ Mouse。筆圧、傾き、回転を完全にサポート。
- **メモリ認識システム**：キャンバス作成前に安全なメモリ予算を自動計算し、緑/黄/赤のダッシュボードインジケータで表示。
- **ダークテーマ UI**：DPI 対応スケーリング（`Theme::Scale`）で、100%～200% の DPI 設定に対応。
- **ドッキング可能なパネルレイアウト**：Workspace で左右パネルのフローティングとドッキングをサポート。

## 技術スタック

| カテゴリ | 技術 |
|----------|------|
| 言語 | C++20 |
| プラットフォーム | Windows 10/11 (x64) |
| コンパイラ | MinGW-w64 GCC 13.1.0 |
| ビルドシステム | CMake 3.25+ / Ninja |
| レンダリング | Direct2D / WIC / GDI+ |
| 入力 | Windows Ink API / WinTab |
| 標準ライブラリ | STL / `std::filesystem` |

## クイックビルド

### 必要条件

- Windows 10/11 x64
- CLion（推奨、bundled CMake + MinGW + Ninja）
- または手動で MinGW-w64 GCC 13+ と CMake 3.25+ をインストール

### CLion でのビルド（推奨）

1. プロジェクトのルートディレクトリを CLion で開く
2. Toolchain が bundled MinGW を指していることを確認
3. **Build**（Ctrl+F9）または **Run**（Shift+F10）をクリック

### コマンドラインでのビルド

```powershell
# MinGW\bin が PATH に含まれていることを確認
$env:PATH = "C:\Users\CloverIris\AppData\Local\Programs\CLion\bin\mingw\bin;$env:PATH"

cmake -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
```

## プロジェクト構成

```
VividPic/
├── src/
│   ├── App/          # Application シングルトン、メッセージポンプ、ライフサイクル
│   ├── Core/         # Project、Layer、LayerManager、BlendMode、MemoryAdvisor
│   ├── Render/       # RenderBackend、D2DCanvas、TilePool、BrushEngine、BrushPreset
│   ├── Tablet/       # TabletInput（WindowsInkDriver + WinTabDriver + TabletManager）
│   ├── UI/
│   │   ├── Core/     # Window、Theme、メッセージ配信基底クラス
│   │   ├── Widgets/  # Button、Panel、EditBox、ComboBox、CanvasView
│   │   ├── Panels/   # BrushPanel、ColorsPanel、LayersPanel、NavigatorPanel
│   │   └── Screens/  # HomeScreen、NewCanvasDialog、Workspace
│   └── Utils/        # Types（String、Point、Rect、Size、Color など）
├── assets/           # ランタイムアセット
├── PRD.md            # 製品要件ドキュメント
├── UI.md             # UI デザイン仕様書
└── AGENTS.md         # 開発制約と進捗
```

## 開発ロードマップ

| マイルストーン | 状態 | 内容 |
|--------------|------|------|
| M1 | ✅ | 基盤フレームワーク、HomeScreen、Window/Theme システム |
| M2 | ✅ | キャンバスエンジン、Direct2D レンダリング、Workspace、基本ブラシ |
| M3 | ✅ | TileGrid レイヤーシステム、8 種類のブレンドモード、パネル、ショートカット |
| M4 | ✅ | 完全なブラシエンジン、5 種類のティップ、BrushPanel、WinTab サポート |
| M5 | 🚧 | 選択ツール、変形、塗りつぶし、テキストツール、ナビゲーター |
| M6 | ⏳ | ファイル I/O（.vvp / PNG / PSD）、履歴/Undo、フィルター |
| M7 | ⏳ | クラウド同期、タイムラプス、エクスポート機能 |
| M8 | ⏳ | 多言語化、設定パネル、パフォーマンス最適化 |

## ライセンス

このプロジェクトは個人の学習作品であり、現在オープンソースライセンスはありません。コードは学習・参考用に提供されています。

---

<p align="center">Made with ❤️ for learning.</p>

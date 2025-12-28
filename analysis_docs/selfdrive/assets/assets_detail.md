# selfdrive/assets 詳細解説

このドキュメントでは、`selfdrive/assets` ディレクトリ内の各ファイル・サブディレクトリについて、初心者向けに技術的な背景や使い方、各行・各ファイルの意味を丁寧に解説します。

---

## assets.qrc / translations_assets.qrc
- Qtのリソースファイル。アプリに埋め込む画像・音声・フォント・翻訳ファイルなどをXML形式で列挙
- 例: `<file>icons/icon_warning.png</file>` のように、どの素材を含めるか指定
- `translations_assets.qrc`は多言語対応用の翻訳ファイル群を管理

## assets.cc
- C++でQtリソースを扱うための実装ファイル
- Qtリソース（.qrcで定義したファイル）をC++コードから参照・利用するための関数や定数を定義

## compress-images.sh
- 画像ファイル（PNG/JPG等）を一括で圧縮・最適化するシェルスクリプト
- 画像追加・更新時に実行し、容量削減や表示高速化を図る
- 主要コマンド例や各行の意味：

```bash
#!/usr/bin/env bash
# 画像圧縮用スクリプト
# ...（実際のコマンドやfind, optipng, jpegoptim等の使い方が記載されている想定）
```

## prep-svg.sh
- SVGファイルを最適化・変換するシェルスクリプト
- SVG追加・編集時に実行し、不要なメタデータ削除やサイズ削減を行う
- 主要コマンド例や各行の意味：

```bash
#!/usr/bin/env bash
# SVG最適化用スクリプト
# ...（実際のコマンドやsvgo等の使い方が記載されている想定）
```

## fonts/
- UIで使うフォントファイル（例: NotoSans, Roboto等）を格納
- Qtリソースとしてアプリに埋め込まれる

## icons/
- ボタンやUI部品のアイコン画像（PNG, SVG等）を格納
- 例: `icon_warning.png`, `icon_ok.svg` など

## images/
- UIや地図、背景などで使う汎用画像を格納

## sounds/
- 操作音や警告音などの音声ファイル（WAV, MP3等）を格納

## body/ , offroad/ , training/
- 特定画面や機能ごとの素材をまとめたサブディレクトリ
- 例: `body/`は車両本体の画像や3Dモデル、`offroad/`はオフロード画面用素材、`training/`はチュートリアル用素材

## .gitignore
- 一時ファイルや生成物など、Git管理対象外とするパターンを記述

---


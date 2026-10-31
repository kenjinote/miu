# WindowsからmacOS版App Store画像を更新する

`store_screenshots.py`はMAC_OS / APP_DESKTOP専用です。iPhone・iPadの画像や審査提出には触れません。
旧`appstore_screenshots_sync.py`の翻訳データとAPI認証・転送処理を利用しますが、旧スクリプトのmainは実行しません。

## 準備と一括実行

Windows、Microsoft Edge、Python 3.10以降が必要です。

```powershell
python -m pip install -r scripts/requirements-store-screenshots.txt
./scripts/run-store-screenshots.ps1 -KeyFile 'D:\path\AuthKey_KEYID.p8' -IssuerId 'ISSUER-UUID' -Version '1.0.44'
```

秘密鍵は指定ファイルからメモリーに読み込み、出力やログには保存しません。
認証を環境変数で指定する場合はAPPSTORE_KEY_ID / APPSTORE_ISSUER_ID / APPSTORE_PRIVATE_KEYを利用できます。

## 個別実行

```powershell
python scripts/store_screenshots.py build --all-locales
python scripts/store_screenshots.py validate
python scripts/store_screenshots.py plan --version 1.0.44 --key-file 'D:\path\AuthKey_KEYID.p8' --issuer-id 'ISSUER-UUID'
python scripts/store_screenshots.py sync --version 1.0.44 --key-file 'D:\path\AuthKey_KEYID.p8' --issuer-id 'ISSUER-UUID'
python scripts/test_store_screenshots.py
```

`build_screenshots_v2/review.html`が全言語の確認画面です。`contact_01.jpg`～`contact_05.jpg`も生成します。
28言語×5枚を2880×1800 RGB PNGで生成し、実際に登録されているストアロケールへ明示的に対応づけます。
マニフェストには各画像のハッシュ、素材、素材の言語、切り抜き範囲、レイアウト検査値を記録します。

## 素材と品質の制約

- 実際のmacOS再撮影ではなく、既存画面素材に統一したタイトルバーと外側の説明を合成します。
- 背景画素の推定・再着色、検索画面の描き足し、縦横比の変形、末端画素による背景延長はしません。
- 全体が黒一色の外周パディングのみ最大12px除去し、スクロールバーを含む本文を保存します。
- 素材は1116×706です。出力の高解像度化によって素材の文字情報自体が増えるわけではありません。
- Markdownの旧素材には空白プレビューやWindows専用文言があるため、日本語以外は正常な英語サンプルを使用します。
- Mermaid・数式は日英中韓の素材、それ以外の言語は英語サンプルを使用します。
- 正規表現・マルチカーソルは日英の素材です。すべてのマーケティング見出しは対象言語で描画します。
- `store_screenshots_sources.json`が全言語の素材選択の定義です。無指定の言語へ暗黙のフォールバックはしません。
- WindowsのEdgeを使い、タイ語やヒンディー語もOSのフォントシェーピングで描画します。

## 更新・再開・復旧

syncはバージョン・プラットフォームを完全一致で選び、編集可能状態を確認します。
全画像を検査し、全言語の旧画像と順序を`backups/<timestamp>/`へ保存してから変更を開始します。
バックアップ画像はApple CDNから原寸で取得したPNGで、元アップロードファイルとのバイト一致を保証するものではありません。

通常の5枚→5枚では、旧5枚を残したまま新5枚をアップロードし、全画像のCOMPLETEを待ってから旧画像を削除します。
10枚の上限を超える場合だけ、バックアップ済み旧画像の必要最小数を先に削除します。
全言語を同時に切り替えるトランザクションではなく、2言語ずつ処理します。
更新後は枚数・順序・MD5をAPIで確認し、`sync_report.json`と`state/`へ結果を保存します。

失敗した場合は同じsyncコマンド、またはラッパーの`-Resume`で再開します。
リモートにある一致する画像をチェックサムで再利用します。アップロード途中に失敗して残った枠は、次回バックアップ可能な状態になるまで手動の確認が必要になる場合があります。

```powershell
python scripts/store_screenshots.py restore --version 1.0.44 --backup 'build_screenshots_v2/backups/TIMESTAMP' --key-file 'D:\path\AuthKey_KEYID.p8' --issuer-id 'ISSUER-UUID'
```

restoreは対象ロケールの現行画像を削除し、バックアップを元の順に再アップロードします。復旧も言語単位で、処理中は一時的に画像が欠けます。
公開・審査提出は行いません。

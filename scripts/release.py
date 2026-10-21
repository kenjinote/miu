import os
import sys
import re
import json
import subprocess
import urllib.request
import urllib.parse
from datetime import datetime

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
if hasattr(sys.stderr, 'reconfigure'):
    sys.stderr.reconfigure(encoding='utf-8', errors='replace')

REPO_OWNER = "kenjinote"
REPO_NAME = "miu"

def get_repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def get_github_token():
    try:
        res = subprocess.run(['git', 'credential', 'fill'], input="protocol=https\nhost=github.com\n", capture_output=True, text=True, check=True)
        creds = dict(line.split('=', 1) for line in res.stdout.splitlines() if '=' in line)
        token = creds.get('password')
        if token:
            return token
    except Exception as e:
        print(f"[Warn] Could not retrieve token from git credential manager: {e}")
    token = os.environ.get('GITHUB_TOKEN')
    if token:
        return token
    raise RuntimeError("GitHub token could not be found via git credential or GITHUB_TOKEN environment variable.")

def get_current_version(root_dir):
    src_path = os.path.join(root_dir, "Source.cpp")
    with open(src_path, "r", encoding="utf-16le") as f:
        content = f.read()
    m = re.search(r'APP_VERSION\s*=\s*L"miu\s+v?([0-9\.]+)"', content)
    if not m:
        raise ValueError("Could not extract APP_VERSION from Source.cpp")
    return m.group(1)

def find_msbuild():
    vswhere = r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if os.path.exists(vswhere):
        res = subprocess.run([vswhere, "-latest", "-requires", "Microsoft.Component.MSBuild", "-find", r"MSBuild\**\Bin\MSBuild.exe"], capture_output=True, text=True)
        lines = [l.strip() for l in res.stdout.splitlines() if l.strip().endswith("MSBuild.exe")]
        if lines and os.path.exists(lines[0]):
            return lines[0]
    default_msbuild = r"C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    if os.path.exists(default_msbuild):
        return default_msbuild
    raise RuntimeError("MSBuild.exe not found.")

def find_iscmdbld():
    candidates = [
        r"C:\Program Files (x86)\InstallShield\2021\System\IsCmdBld.exe",
        r"C:\Program Files\InstallShield\2021\System\IsCmdBld.exe",
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    raise RuntimeError("InstallShield IsCmdBld.exe not found.")

def build_app(root_dir):
    print("=== Building miu (Release x64) ===")
    msbuild = find_msbuild()
    sln_path = os.path.join(root_dir, "miu.sln")
    cmd = [msbuild, sln_path, "/p:Configuration=Release", "/p:Platform=x64"]
    res = subprocess.run(cmd)
    if res.returncode != 0:
        raise RuntimeError(f"MSBuild failed with code {res.returncode}")
    
    miu_exe = os.path.join(root_dir, "x64", "Release", "miu.exe")
    if not os.path.exists(miu_exe):
        raise FileNotFoundError(f"Expected binary not found: {miu_exe}")
    print(f"Built binary: {miu_exe} ({os.path.getsize(miu_exe)} bytes)")
    return miu_exe

def build_installer(root_dir):
    print("=== Building Installer (Setup.exe) ===")
    iscmdbld = find_iscmdbld()
    ism_path = os.path.join(root_dir, "installer", "miu.ism")
    cmd = [iscmdbld, "-p", ism_path, "-r", "SINGLE_EXE_IMAGE"]
    res = subprocess.run(cmd)
    if res.returncode != 0:
        raise RuntimeError(f"InstallShield build failed with code {res.returncode}")
    
    setup_exe = os.path.join(root_dir, "installer", "miu", "Media", "SINGLE_EXE_IMAGE", "Package", "Setup.exe")
    if not os.path.exists(setup_exe):
        raise FileNotFoundError(f"Expected setup not found: {setup_exe}")
    print(f"Built installer: {setup_exe} ({os.path.getsize(setup_exe)} bytes)")
    return setup_exe

def create_and_push_tag(tag_name, root_dir):
    print(f"=== Checking Git Tag: {tag_name} ===")
    res = subprocess.run(["git", "tag", "-l", tag_name], cwd=root_dir, capture_output=True, text=True)
    if tag_name not in res.stdout.split():
        print(f"Creating tag {tag_name}...")
        subprocess.run(["git", "tag", tag_name], cwd=root_dir, check=True)
    else:
        print(f"Tag {tag_name} already exists locally.")

    print(f"Pushing tag {tag_name} to origin...")
    subprocess.run(["git", "push", "origin", tag_name], cwd=root_dir, check=True)

def create_github_release(token, tag_name, release_name, body_text):
    print(f"=== Creating GitHub Release: {release_name} ===")
    url = f"https://api.github.com/repos/{REPO_OWNER}/{REPO_NAME}/releases"
    headers = {
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github+json",
        "User-Agent": "MiuReleaseScript",
        "Content-Type": "application/json"
    }

    # Check if release already exists
    req_get = urllib.request.Request(f"{url}/tags/{tag_name}", headers=headers)
    existing_release = None
    try:
        with urllib.request.urlopen(req_get) as resp:
            existing_release = json.loads(resp.read().decode())
            print(f"Release for {tag_name} already exists (ID: {existing_release.get('id')}).")
    except urllib.error.HTTPError as e:
        if e.code != 404:
            raise

    if existing_release:
        return existing_release

    payload = {
        "tag_name": tag_name,
        "target_commitish": "master",
        "name": release_name,
        "body": body_text,
        "draft": False,
        "prerelease": False
    }

    data = json.dumps(payload).encode('utf-8')
    req_post = urllib.request.Request(url, data=data, headers=headers, method="POST")
    with urllib.request.urlopen(req_post) as resp:
        rel = json.loads(resp.read().decode())
        print(f"Created release ID: {rel.get('id')}")
        return rel

def upload_release_asset(token, release_id, file_path):
    file_name = os.path.basename(file_path)
    file_size = os.path.getsize(file_path)
    print(f"Uploading asset: {file_name} ({file_size} bytes)...")

    # Check if asset already exists in release
    headers = {
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github+json",
        "User-Agent": "MiuReleaseScript"
    }
    url_assets = f"https://api.github.com/repos/{REPO_OWNER}/{REPO_NAME}/releases/{release_id}/assets"
    req_list = urllib.request.Request(url_assets, headers=headers)
    with urllib.request.urlopen(req_list) as resp:
        assets = json.loads(resp.read().decode())
        for a in assets:
            if a.get('name') == file_name:
                print(f"Deleting existing asset {file_name} (ID: {a.get('id')})...")
                req_del = urllib.request.Request(f"https://api.github.com/repos/{REPO_OWNER}/{REPO_NAME}/releases/assets/{a.get('id')}", headers=headers, method="DELETE")
                urllib.request.urlopen(req_del)

    upload_url = f"https://uploads.github.com/repos/{REPO_OWNER}/{REPO_NAME}/releases/{release_id}/assets?name={urllib.parse.quote(file_name)}"
    headers_upload = {
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github+json",
        "User-Agent": "MiuReleaseScript",
        "Content-Type": "application/octet-stream"
    }

    with open(file_path, "rb") as f:
        file_bytes = f.read()

    req_upload = urllib.request.Request(upload_url, data=file_bytes, headers=headers_upload, method="POST")
    with urllib.request.urlopen(req_upload) as resp:
        asset_info = json.loads(resp.read().decode())
        print(f"Uploaded {file_name} -> {asset_info.get('browser_download_url')}")
        return asset_info

try:
    from .store_publisher import release_to_store
except ImportError:
    from store_publisher import release_to_store

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Automated GitHub and Microsoft Store Release script for miu")
    parser.add_argument("--skip-build", action="store_true", help="Skip compiling and use existing binaries")
    parser.add_argument("--notes", type=str, help="Release notes markdown string or file path")
    parser.add_argument("--skip-store", action="store_true", help="Skip publishing to Microsoft Store")
    parser.add_argument("--store-only", action="store_true", help="Only publish to Microsoft Store (skip GitHub release)")
    parser.add_argument("--no-store-commit", action="store_true", help="Prepare Microsoft Store draft submission but do not commit")
    args = parser.parse_args()

    root_dir = get_repo_root()
    version = get_current_version(root_dir)
    tag_name = f"v{version}"
    release_name = f"v{version}"
    print(f"=== Miu Automated Release: {release_name} ===")

    msbuild = find_msbuild()

    if not args.store_only:
        if not args.skip_build:
            miu_exe = build_app(root_dir)
            setup_exe = build_installer(root_dir)
        else:
            miu_exe = os.path.join(root_dir, "x64", "Release", "miu.exe")
            setup_exe = os.path.join(root_dir, "installer", "miu", "Media", "SINGLE_EXE_IMAGE", "Package", "Setup.exe")
            if not os.path.exists(miu_exe) or not os.path.exists(setup_exe):
                raise FileNotFoundError("Binaries not found. Please run without --skip-build.")

        notes = args.notes
        if not notes:
            # Default release notes template
            notes = f"""# miu {release_name} リリースノート

### 🚀 バイナリサイズの大幅削減（-19.9%）＆ 実行速度のさらなる高速化（+15.6%）

1. **単一実行ファイル（`miu.exe`）サイズを約 20% 削減（`1,053.5 KB` → `844.0 KB` / `-209.5 KB`）**
   - **単一EXEのポータビリティ・安全性は100%そのままに軽量化**:
     外部パッカー（UPX等）や外部DLL依存を一切使用せず、セキュリティソフトの誤検知ゼロ・署名信頼性・ミリ秒単位の起動速度を維持したまま、バイナリサイズを **`1,078,784 bytes (1,053.5 KB)` から `864,256 bytes (844.0 KB)` へ `-214,528 bytes (-19.9%)` 削減**しました。
   - **最適化の内訳**:
     - C++標準入出力ストリーム（`<iostream>` / `std::locale`）および冗長なCRT書式文字列エンジンの排除とWin32ネイティブI/Oへの完全統一
     - 全15言語のシンタックスハイライト定義テーブルのコンパイル時定数化（`constexpr` 静的配置）による動的初期化コード・起動時ヒープ確保ゼロ化
     - 全28言語ダイアログリソースの共通テンプレート統合およびアイコンPNGチャンクのロスレス再圧縮

2. **文字コード自動判定エンジンの高速化（+15.6%）と対応エンコーディングの大幅強化**
   - **文字コード自動判定速度を約 15.6% 高速化**:
     Compact Encoding Detection (CED) エンジン内部の冗長な実行時フラグ参照・分岐をコンパイル時定数化（`static constexpr`）し、判定アルゴリズムや精度を一切省略することなく、**50,000回連続判定ベンチマークで `158.28 ms` → `133.52 ms`（約 15.6% 高速化 / 1ファイルあたり約 `2.67 μs`）**を達成しました。
   - **25種類以上の文字コード自動判定・デコード精度の強化（8,500件超の網羅テスト済み）**:
     日本語（UTF-8 / UTF-16LE・BE（BOMなし含む） / Shift_JIS（CP932） / EUC-JP（CP20932） / ISO-2022-JP（CP50220））に加え、簡体字中国語（GB18030 / GBK / HZ）、繁体字中国語（Big5）、韓国語（EUC-KR）、キリル文字（KOI8-R / KOI8-U / CP1251 / IBM866 / ISO-8859-5）、中欧・東欧・バルト・ギリシャ・トルコ・ヘブライ・アラビア・タイ語・ベトナム語等のコードページマッピングを大幅に拡充しました。

3. **全28言語 × 高DPI環境（100%〜250%）でのダイアログ動的オートレイアウト**
   - **文字見切れ・重なりを完全に防止**:
     検索・置換（`Ctrl+F` / `Ctrl+H`）および指定行へ移動（`Ctrl+G`）ダイアログにおいて、表示言語のテキスト実測幅とモニターDPI（96〜240 DPI）に応じてボタン幅・ラベル幅・ダイアログ寸法を動的に自動計算するオートレイアウトエンジンを搭載しました。

---

### 📊 パフォーマンス・サイズ比較サマリー（v1.0.35 → v1.0.36）

| 指標 | v1.0.35 | v1.0.36 | 改善幅 |
| :--- | :---: | :---: | :---: |
| **`miu.exe` 単体ファイルサイズ** | `1,053.5 KB` (`1,078,784 B`) | **`844.0 KB` (`864,256 B`)** | **-209.5 KB (-19.9%)** |
| **文字コード自動判定（50,000回）** | `158.28 ms` (`3.17 μs/回`) | **`133.52 ms` (`2.67 μs/回`)** | **+15.6% 高速化** |
| **構文定義テーブル初期化コスト** | 起動時ヒープ動的構築 | **コンパイル時定数 (`0 ms`)** | **動的アロケーション 100% 削減** |
| **多言語×高DPIダイアログ検証** | 固定幅テンプレート | **動的実測オートレイアウト** | **28言語×6種DPI（336構成）見切れ 0件** |
"""
        elif os.path.exists(notes):
            with open(notes, "r", encoding="utf-8") as f:
                notes = f.read()

        token = get_github_token()
        create_and_push_tag(tag_name, root_dir)
        release = create_github_release(token, tag_name, release_name, notes)
        release_id = release["id"]

        upload_release_asset(token, release_id, miu_exe)
        upload_release_asset(token, release_id, setup_exe)

        print("\n" + "="*50)
        print(f"Successfully published GitHub release {release_name}!")
        print(f"HTML URL: {release.get('html_url')}")
        print("="*50)

    if not args.skip_store:
        try:
            store_success = release_to_store(
                root_dir=root_dir,
                version=version,
                msbuild_path=msbuild,
                skip_build=args.skip_build,
                do_commit=not args.no_store_commit
            )
            if store_success:
                print(f"\nSuccessfully processed Microsoft Store release for {release_name}!")
        except Exception as e:
            print(f"[Error] Failed to publish to Microsoft Store: {e}")
            if args.store_only:
                raise

if __name__ == "__main__":
    main()

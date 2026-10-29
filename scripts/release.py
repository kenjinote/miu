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
            notes = f"# miu {release_name} リリースノート\n\n" + r"""### 🇯🇵 日本語 IME（Microsoft IME / Google 日本語入力 / ATOK 等）の入力不具合修正 ＆ 高速化・軽量化・セキュリティ強化

1. **日本語 IME 変換・入力の不具合修正（重要）**
   - **IME / TSF（Text Services Framework）との互換性修正**: `v1.0.42` で導入したプロセス緩和策ポリシー（`ProcessExtensionPointDisablePolicy` / `ProcessImageLoadPolicy`）および `SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)` により、Windows の IMM32 / TSF フックおよびサードパーティ製 IME（Google 日本語入力・ATOK 等）の COM インプロセス TIP モジュール読み込みがブロックされ、日本語入力ができなくなる問題を修正しました。
   - **カレントディレクトリ DLL ハイジャック防止の維持**: IME 拡張ポイントや TSF モジュールのロードを阻害しない `SetDllDirectoryW(L"")` 方式へ切り替え、日本語 IME との完全な互換性と DLL プリロード攻撃防止を両立しました。
   - **WebView2 プレビュー連携時の IME フォーカス制御改善**: 起動時の不要なバックグラウンド WebView2 初期化を廃止し、プレビュー（`Alt+P`）表示切り替え時のエディタフォーカス維持と、WebView2 内の `<input>` / `<textarea>` フォーム要素への直接入力・IME 変換の両立を実現しました。

2. **全文検索・一括置換・シンタックスハイライト・描画の超高速化（最大 605 倍）**
   - **AVX2 先頭・末尾文字同時照合 SIMD 全文検索（最大 605.1 倍高速化）**: `PieceTable` のピース内連続メモリ走査と AVX2（32バイト単位）による先頭・末尾文字同時ベクトル照合により、32 MB 文書の前方検索を **245.3 倍**（`705.66 ms` → `2.88 ms`）、後方検索を **605.1 倍**（`2,708.09 ms` → `4.48 ms`）へ高速化しました。
   - **$O(N + M)$ 線形一括置換（323.3 倍高速化）**: 単一パスでピース配列を再構築するアルゴリズムにより、5,000箇所の一括置換を `169.53 ms` → **`0.52 ms`（323.3 倍高速化）** に短縮しました。
   - **シンタックスハイライト高速スキャナ（22.7 倍高速化）＆ 描画スクラッチバッファ永続化**: フレーム毎のヒープ確保をゼロ化し、Pele 図表エンジンのアロケーションも 1 パスあたり 58,576 回削減（**1.34 倍高速化**）しました。

3. **EXEサイズの大幅最小化（-788.50 KB / -15.32% 削減：`5,148.00 KB` → `4,359.50 KB`）＆ セキュリティ保護**
   - **Hot / Cold 最適化プラグマ分離 ＆ 1,107 関数の `noexcept` 化**: `.text`（-13.82%）・`.pdata`（-41.77%）・`ced.lib`（-28.94%）を圧縮し、単一実行ファイルを **4.35 MB** に最小化しました。
   - **セキュリティ強化**: ヒープ破損時即時終了保護（`HeapEnableTerminationOnCorruption`）、正規表現検索の ReDoS（壊滅的バックトラッキング）事前検証ガード（`isSafeUserRegex`）、および `<VcpkgEnabled>false</VcpkgEnabled>` によるサプライチェーン保護を備えています。
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

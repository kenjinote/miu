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
        res = subprocess.run(['git', 'credential', 'fill'], input="protocol=https\nhost=github.com\n", capture_output=True, text=True, check=True, timeout=30)
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
        res = subprocess.run([vswhere, "-latest", "-requires", "Microsoft.Component.MSBuild", "-find", r"MSBuild\**\Bin\MSBuild.exe"], capture_output=True, text=True, timeout=30)
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
    res = subprocess.run(cmd, timeout=1800)
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
    res = subprocess.run(cmd, timeout=1800)
    if res.returncode != 0:
        raise RuntimeError(f"InstallShield build failed with code {res.returncode}")
    
    setup_exe = os.path.join(root_dir, "installer", "miu", "Media", "SINGLE_EXE_IMAGE", "Package", "Setup.exe")
    if not os.path.exists(setup_exe):
        raise FileNotFoundError(f"Expected setup not found: {setup_exe}")
    print(f"Built installer: {setup_exe} ({os.path.getsize(setup_exe)} bytes)")
    return setup_exe

def create_and_push_tag(tag_name, root_dir):
    print(f"=== Checking Git Tag: {tag_name} ===")
    res = subprocess.run(["git", "tag", "-l", tag_name], cwd=root_dir, capture_output=True, text=True, timeout=30)
    if tag_name not in res.stdout.split():
        print(f"Creating tag {tag_name}...")
        subprocess.run(["git", "tag", tag_name], cwd=root_dir, check=True, timeout=30)
    else:
        print(f"Tag {tag_name} already exists locally.")

    print(f"Pushing tag {tag_name} to origin...")
    subprocess.run(["git", "push", "origin", tag_name], cwd=root_dir, check=True, timeout=120)

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
        with urllib.request.urlopen(req_get, timeout=60) as resp:
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
    with urllib.request.urlopen(req_post, timeout=60) as resp:
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
    with urllib.request.urlopen(req_list, timeout=60) as resp:
        assets = json.loads(resp.read().decode())
        for a in assets:
            if a.get('name') == file_name:
                print(f"Deleting existing asset {file_name} (ID: {a.get('id')})...")
                req_del = urllib.request.Request(f"https://api.github.com/repos/{REPO_OWNER}/{REPO_NAME}/releases/assets/{a.get('id')}", headers=headers, method="DELETE")
                urllib.request.urlopen(req_del, timeout=60)

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
    with urllib.request.urlopen(req_upload, timeout=600) as resp:
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
            notes = f"# miu {release_name} リリースノート / Release Notes\n\n" + r"""## 🇯🇵 日本語 (Japanese)
v1.0.56 では、Windows・macOS・iOS・Android 全プラットフォームにおける HTML ライブプレビューの `<script>` 動作統一と、未保存 HTML スニペットへの対応を実施しました。

- **未保存 HTML スニペットの `<script>` 実行対応**：新規作成の未保存バッファにおいて、`<!DOCTYPE html>` 等を省略して `<canvas>`・`<div>`・`<script>`・`<style>`・`<button>` 等から直接書き始めた場合でも HTML プレビューとして描画・スクリプト実行されるようになりました（オフライン隔離・ローカル保護は維持）。
- **プラットフォーム間の HTML プレビュー CSP 統一**：macOS / iOS / Android の HTML プレビュー CSP を Windows 版と揃え、`eval()` / `new Function()` および `blob:` / `data:` URL スクリプトの実行に対応しました。
- **`.html` 保存直後のライブプレビュー即時反映**：新規テキストを `.html` として保存した瞬間にプレビューが自動的に HTML モードへ切り替わるよう改善しました。

---

## 🇺🇸 English
Version 1.0.56 unifies HTML Live Preview `<script>` execution across Windows, macOS, iOS, and Android, and enables instant `<script>` previewing in unsaved HTML snippets.

- **Unsaved HTML Snippet `<script>` Support**: Unsaved scratchpad buffers starting directly with HTML elements such as `<canvas>`, `<div>`, `<script>`, `<style>`, or `<button>` (without requiring a `<!DOCTYPE html>` prologue) now automatically render in HTML Live Preview with `<script>` execution while preserving strict offline isolation.
- **Cross-Platform HTML Preview CSP Parity**: Aligned HTML Live Preview CSP on macOS, iOS, and Android with Windows to support `eval()` / `new Function()` as well as `blob:` and `data:` scripts.
- **Instant Preview Refresh on `.html` Save**: Saving an untitled buffer as `.html` now immediately switches Live Preview into HTML mode without requiring an extra edit or toggle.

---

## 🇨🇳 简体中文 (Simplified Chinese)
v1.0.56 统一了 Windows、macOS、iOS 与 Android 全平台的 HTML 实时预览 `<script>` 行为，并新增对未保存 HTML 片段脚本的直接支持。

- **支持未保存 HTML 片段执行 `<script>`**：在新建未保存缓冲区中直接编写以 `<canvas>`、`<div>`、`<script>`、`<style>`、`<button>` 等开头的 HTML 片段（无需 `<!DOCTYPE html>` 声明）时，现可直接渲染并运行脚本，同时保持严格的离线安全隔离。
- **全平台 HTML 预览 CSP 统一**：将 macOS / iOS / Android 的 HTML 预览 CSP 与 Windows 版对齐，支持 `eval()` / `new Function()` 以及 `blob:` / `data:` 脚本加载。
- **另存为 `.html` 后即时刷新预览**：将未命名缓冲区保存为 `.html` 文件后，实时预览会立即自动切换至 HTML 模式。

---

## 🇹🇼 繁體中文 (Traditional Chinese)
v1.0.56 統一了 Windows、macOS、iOS 與 Android 全平台的 HTML 即時預覽 `<script>` 行為，並新增對未儲存 HTML 片段腳本的直接支援。

- **支援未儲存 HTML 片段執行 `<script>`**：在新建未儲存緩衝區中直接編寫以 `<canvas>`、`<div>`、`<script>`、`<style>`、`<button>` 等開頭的 HTML 片段（無需 `<!DOCTYPE html>` 宣告）時，現可直接渲染並執行腳本，同時維持嚴格的離線安全隔離。
- **全平台 HTML 預覽 CSP 統一**：將 macOS / iOS / Android 的 HTML 預覽 CSP 與 Windows 版對齊，支援 `eval()` / `new Function()` 以及 `blob:` / `data:` 腳本載入。
- **另存為 `.html` 後即時重新整理預覽**：將未命名緩衝區儲存為 `.html` 檔案後，即時預覽會立即自動切換至 HTML 模式。

---

## 🇰🇷 한국어 (Korean)
v1.0.56에서는 Windows, macOS, iOS, Android 전 플랫폼의 HTML 실시간 미리보기 `<script>` 동작을 통일하고 저장하지 않은 HTML 스니펫 지원을 추가했습니다.

- **미저장 HTML 스니펫 `<script>` 실행 지원**: 새로 만든 미저장 버퍼에서 `<!DOCTYPE html>` 없이 `<canvas>`, `<div>`, `<script>`, `<style>`, `<button>` 등으로 바로 작성해도 오프라인 격리를 유지한 채 HTML 미리보기 및 스크립트가 즉시 실행됩니다.
- **플랫폼 간 HTML 미리보기 CSP 통일**: macOS / iOS / Android의 HTML 미리보기 CSP를 Windows 버전과 동일하게 맞춰 `eval()` / `new Function()` 및 `blob:` / `data:` 스크립트를 지원합니다.
- **`.html` 저장 즉시 미리보기 자동 전환**: 새 문서를 `.html`로 저장하는 즉시 실시간 미리보기가 HTML 모드로 전환됩니다.
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

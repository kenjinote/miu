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
v1.0.57 では、Windows 版における単一行テキスト切り取り・削除後のカーソル移動不具合の修正、編集操作後のカーソル基準位置同期の強化、およびインストーラー版のプログラムアイコン設定修正を実施しました。

- **単一行の切り取り・削除後の矢印キー移動を修正**：単一行内で選択範囲を切り取り（`Ctrl+X`）または削除した際に行開始オフセット計算の符号が反転し、上下矢印キーでカーソルが意図しない列へ飛んだり、行末からの左矢印キー移動が停止したりする問題を修正しました。
- **編集・置換・Undo/Redo 後のカーソル基準座標同期と UTF-8 境界保護を強化**：文字入力・ペースト・削除・インデント・コメント切替・行移動・一括置換・元に戻す／やり直し直後に水平基準座標（`desiredX`）を正確に再計算し、UTF-8 マルチバイト文字境界での安全なカーソル移動を保証しました。
- **Windows「インストールされているアプリ」アイコン表示を修正**：インストーラー（`Setup.exe`）でインストールした際、Windows 設定の「インストールされているアプリ」一覧に `miu` のアプリアイコンが正しく表示されるよう修正しました。

---

## 🇺🇸 English
Version 1.0.57 fixes a cursor navigation regression after single-line cut/delete operations on Windows, strengthens post-edit caret horizontal coordinate synchronization, and fixes the program icon shown in Windows "Installed apps".

- **Fixed Arrow-Key Navigation After Single-Line Cut / Delete**: Fixed a sign error in single-line line-offset adjustment when cutting (`Ctrl+X`) or deleting a selection within a line, preventing vertical arrow jumps to wrong columns and left-arrow stalls on multibyte UTF-8 text.
- **Hardened Post-Edit Caret Column Sync & UTF-8 Boundary Safety**: Synchronized caret `desiredX` immediately after typing, pasting, deleting, indenting, comment toggling, moving lines, Replace All, and Undo/Redo, while enforcing strict UTF-8 code-point boundary progression.
- **Fixed Windows "Installed apps" Program Icon**: Updated the Windows installer (`Setup.exe`) so the `miu` application icon displays properly in Windows Settings > Apps > Installed apps.

---

## 🇨🇳 简体中文 (Simplified Chinese)
v1.0.57 修复了 Windows 版单行剪切/删除后的光标移动异常，强化了编辑操作后的光标水平坐标同步，并修复了安装程序在 Windows“已安装的应用”中的图标显示。

- **修复单行剪切/删除后的方向键光标移动问题**：修复在单行内剪切（`Ctrl+X`）或删除选中文本时行起始偏移量符号计算错误，解决上下方向键跳转到错误列以及在多字节 UTF-8 行末按左方向键无法移动的问题。
- **强化编辑、替换与撤销/重做后的光标列同步及 UTF-8 边界保护**：在输入、粘贴、删除、缩进、注释切换、移动行、全部替换及撤销/重做后精确更新光标水平基准坐标（`desiredX`），并确保严格按 UTF-8 字符边界安全移动。
- **修复 Windows“已安装的应用”图标显示**：更新安装程序（`Setup.exe`），使 Windows 设置“已安装的应用”列表中正确显示 `miu` 程序图标。

---

## 🇹🇼 繁體中文 (Traditional Chinese)
v1.0.57 修復了 Windows 版單行剪下／刪除後的游標移動異常，強化了編輯操作後的游標水平座標同步，並修復了安裝程式在 Windows「已安裝的應用程式」中的圖示顯示。

- **修復單行剪下／刪除後的方向鍵游標移動問題**：修復在單行內剪下（`Ctrl+X`）或刪除選取文字時行起始位移符號計算錯誤，解決上下方向鍵跳轉至錯誤欄位以及在多位元組 UTF-8 行末按左方向鍵無法移動的問題。
- **強化編輯、取代與復原／重做後的游標欄位同步及 UTF-8 邊界保護**：在輸入、貼上、刪除、縮排、註解切換、移動行、全部取代及復原／重做後精確更新游標水平基準座標（`desiredX`），並確保嚴格依 UTF-8 字元邊界安全移動。
- **修復 Windows「已安裝的應用程式」圖示顯示**：更新安裝程式（`Setup.exe`），使 Windows 設定「已安裝的應用程式」清單中正確顯示 `miu` 程式圖示。

---

## 🇰🇷 한국어 (Korean)
v1.0.57에서는 Windows 버전의 단일 줄 잘라내기/삭제 후 커서 이동 오류를 수정하고, 편집 후 커서 기준 좌표 동기화를 강화했으며, Windows '설치된 앱' 목록의 아이콘 표시를 개선했습니다.

- **단일 줄 잘라내기/삭제 후 방향키 이동 오류 수정**: 한 줄 내에서 선택 영역을 잘라내기(`Ctrl+X`)하거나 삭제할 때 줄 시작 오프셋 부호가 반대로 적용되어 위/아래 방향키 입력 시 엉뚱한 열로 이동하거나 줄 끝에서 왼쪽 방향키가 멈추던 문제를 수정했습니다.
- **편집·바꾸기·실행 취소/다시 실행 후 커서 열 동기화 및 UTF-8 경계 보호 강화**: 입력, 붙여넣기, 삭제, 들여쓰기, 주석 토글, 줄 이동, 모두 바꾸기 및 실행 취소/다시 실행 직후 수평 기준 좌표(`desiredX`)를 정확히 갱신하고 멀티바이트 UTF-8 문자 경계에서 안전한 커서 이동을 보장합니다.
- **Windows '설치된 앱' 아이콘 표시 수정**: 설치 프로그램(`Setup.exe`)으로 설치 시 Windows 설정의 '설치된 앱' 목록에 `miu` 프로그램 아이콘이 정상적으로 표시되도록 수정했습니다.
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

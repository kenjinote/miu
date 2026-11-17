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
v1.0.55 では、既存の機能・UI・描画仕様を完全に維持したまま、ファイル読込・検索・シンタックスハイライト・編集・スクロール・描画の各ホットパスを高速化しました。

- **UTF-8 検証の SIMD 高速化**：AVX2 / SSE2 / ARM NEON による 16/32 バイト ASCII 一括スキップを導入し、ファイル読込・保存検証を大幅に高速化しました。
- **検索・置換および 65 言語ハイライトの高速化**：大文字小文字を区別しない検索・上方向検索・全置換への高速スキャン適用と、Markdown 等の正規表現プリチェックにより処理負荷を大幅に削減しました。
- **大容量ファイルでの編集・スクロール・描画最適化**：単一行編集時の最長行追跡の `O(1)` 化、スクロール時のブロックコメント判定のチェックポイント化、および一時アロケーション削減によりレスポンスを向上しました。

---

## 🇺🇸 English
Version 1.0.55 delivers across-the-board speed optimizations across file loading, search, syntax highlighting, editing, scrolling, and rendering while preserving 100% identical behavior and visual output.

- **SIMD-Accelerated UTF-8 Validation**: Added 16/32-byte AVX2 / SSE2 / ARM NEON ASCII fast-skip to significantly speed up file opening and encoding checks.
- **Faster Search & 65-Language Syntax Highlighting**: Accelerated case-insensitive search, backward search, and Replace All, and added required-character prechecks for Markdown and other syntax rules.
- **Smoother Large-File Editing, Scrolling & Rendering**: Optimized single-line max-width tracking to `O(1)`, added line-boundary block-comment checkpoints for smooth scrolling, and eliminated per-frame temporary allocations.

---

## 🇨🇳 简体中文 (Simplified Chinese)
v1.0.55 在完全保持原有功能与显示效果一致的前提下，全面优化了文件读取、查找替换、语法高亮、输入与大文件滚动的性能。

- **UTF-8 校验 SIMD 加速**：采用 AVX2 / SSE2 / ARM NEON 的 16/32 字节 ASCII 快速跳过，显著提升文件打开与编码检测速度。
- **查找替换与 65 种语言语法高亮提速**：全面加速不区分大小写查找、向上查找与全部替换，并为 Markdown 等语法规则增加前置字符快速过滤。
- **大文件编辑、滚动与渲染优化**：将单行编辑最长行追踪优化为 `O(1)`，为块注释解析引入行边界检查点，并消除渲染与光标计算中的临时内存分配。

---

## 🇹🇼 繁體中文 (Traditional Chinese)
v1.0.55 在完全維持原有功能與顯示效果一致的前提下，全面最佳化了檔案讀取、尋找取代、語法突顯、輸入與大檔案捲動的效能。

- **UTF-8 驗證 SIMD 加速**：採用 AVX2 / SSE2 / ARM NEON 的 16/32 位元組 ASCII 快速跳過，顯著提升檔案開啟與編碼偵測速度。
- **尋找取代與 65 種語言語法突顯提速**：全面加速不區分大小寫尋找、向上尋找與全部取代，並為 Markdown 等語法規則加入前置字元快速過濾。
- **大檔案編輯、捲動與渲染最佳化**：將單行編輯最長行追蹤最佳化為 `O(1)`，為區塊註解解析導入行邊界檢查點，並消除渲染與游標計算中的暫時記憶體配置。

---

## 🇰🇷 한국어 (Korean)
v1.0.55에서는 기존 기능과 화면 표시를 100% 동일하게 유지하면서 파일 열기, 검색/바꾸기, 구문 강조, 편집, 스크롤 및 렌더링 전반의 속도를 개선했습니다.

- **SIMD 기반 UTF-8 검증 고속화**: AVX2 / SSE2 / ARM NEON 16/32바이트 ASCII 고속 스킵을 적용하여 파일 열기 및 인코딩 감지 속도를 크게 높였습니다.
- **검색/바꾸기 및 65개 언어 구문 강조 고속화**: 대소문자 구분 없는 검색, 이전 찾기, 모두 바꾸기를 고속화하고 Markdown 등 구문 정규식에 필수 문자 사전 필터를 적용했습니다.
- **대용량 파일 편집·스크롤·렌더링 최적화**: 단일 행 편집 시 최장 행 추적을 `O(1)`로 개선하고 블록 주석 체크포인트 및 프레임 내 임시 할당 제거로 반응성을 높였습니다.
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

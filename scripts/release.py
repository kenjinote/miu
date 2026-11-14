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
v1.0.53 では、ライブプレビュー保護・描画精度の強化、正規表現検索／65言語ハイライトの改善、およびファイル保存保護・多言語UIの修正を実施しました。

- **ライブプレビューのセキュリティ・描画精度向上**：オフライン分離（CSP・URLサニタイズ）を強化し、Markdown リンク／表／LaTeX 数式および全33種 Mermaid ダイアグラムの描画安定性を向上しました。
- **正規表現・Emmet・65言語ハイライトの改善**：複数行正規表現（`^` / `$` / `\uXXXX`）の検索・置換、Emmet 展開、全角文字を含む矩形選択、ネストコメント解析の精度を改善しました。
- **安全な保存と多言語UIの改善**：非 UTF-8 保存時の文字欠落確認、アトミック保存・未保存データの保護強化、および Windows の OS 表示言語自動検出（日本語ほか30言語以上）を修正しました。

---

## 🇺🇸 English
Version 1.0.53 strengthens Live Preview security & rendering accuracy, improves Regex & 65-language syntax highlighting, and enhances file-save safety & multi-language UI detection.

- **Live Preview Security & Rendering**: Hardened offline isolation (CSP & URL sanitization) and improved Markdown link/table/math parsing alongside all 33 Mermaid diagram renderers.
- **Regex, Emmet & 65-Language Syntax**: Improved multiline Regex (`^` / `$` / `\uXXXX`) find & replace, Emmet expansion, CJK rectangular selection alignment, and nested comment/literal highlighting.
- **Safer Saving & Multi-Language UI**: Added data-loss confirmation for non-UTF-8 saves, improved atomic/read-only saving & draft recovery, and fixed Windows OS UI language detection across 30+ languages.

---

## 🇨🇳 简体中文 (Simplified Chinese)
v1.0.53 强化了实时预览安全与渲染精度，改进了正则表达式与 65 种语言语法高亮，并优化了文件保存保护与多语言界面识别。

- **实时预览安全与渲染优化**：增强离线隔离（CSP 与 URL 过滤），提升 Markdown 链接/表格/LaTeX 公式及全部 33 种 Mermaid 图表的渲染稳定性。
- **正则查找、Emmet 与语法高亮**：改进多行正则表达式（`^` / `$` / `\uXXXX`）查找替换、Emmet 展开、全角矩形选择对齐及 65 种语言嵌套注释解析。
- **安全保存与多语言界面**：新增非 UTF-8 编码保存字符丢失提醒，强化原子保存与草稿恢复，并修复 Windows 系统界面语言自动识别（支持 30 余种语言）。

---

## 🇹🇼 繁體中文 (Traditional Chinese)
v1.0.53 強化了即時預覽安全與渲染精準度，改進了正規表示式與 65 種語言語法突顯，並最佳化了檔案儲存保護與多語言介面辨識。

- **即時預覽安全與渲染最佳化**：增強離線隔離（CSP 與 URL 過濾），提升 Markdown 連結/表格/LaTeX 公式及全部 33 種 Mermaid 圖表的渲染穩定性。
- **正規尋找、Emmet 與語法突顯**：改進多行正規表示式（`^` / `$` / `\uXXXX`）尋找取代、Emmet 展開、全角矩形選取對齊及 65 種語言巢狀註解解析。
- **安全儲存與多語言介面**：新增非 UTF-8 編碼儲存字元遺失提醒，強化原子儲存與草稿復原，並修正 Windows 系統介面語言自動辨識（支援 30 餘種語言）。

---

## 🇰🇷 한국어 (Korean)
v1.0.53에서는 실시간 미리보기 보안·렌더링 정밀도 강화, 정규식 및 65개 언어 구문 강조 개선, 안전한 파일 저장 및 다국어 UI 감지를 개선했습니다.

- **실시간 미리보기 보안 및 렌더링**: 오프라인 격리(CSP·URL 필터링)를 강화하고 Markdown 링크/표/LaTeX 수식 및 33종 Mermaid 다이어그램 렌더링 안정성을 높였습니다.
- **정규식·Emmet·65개 언어 구문 강조**: 다중 행 정규식(`^` / `$` / `\uXXXX`) 찾기·바꾸기, Emmet 확장, 전각 문자 사각형 선택 정렬 및 중첩 주석 분석을 개선했습니다.
- **안전한 저장 및 다국어 UI**: 비 UTF-8 인코딩 저장 시 문자 손실 확인 추가, 원자적 저장·임시 보관 복구 강화 및 Windows OS 표시 언어 자동 감지(30개 이상 언어)를 수정했습니다.
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

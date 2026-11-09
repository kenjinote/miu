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
            notes = f"# miu {release_name} リリースノート / Release Notes\n\n" + r"""## 🇯🇵 日本語 (Japanese)
いつも **miu** をご利用いただきありがとうございます。
v1.0.49 では、Mermaid 図表プレビューで全角括弧を含む日本語・多言語ラベルを入力した際の動作改善と、全33種類のダイアグラムにおけるマルチバイト文字処理の強化を行いました。

### 🛠 主な改善・修正内容
- **Mermaid 図表プレビューの全角括弧・長文ラベル折り返し処理を修正**:
  - フローチャート等のノードラベルに日本語・韓国語・中国語などの全角文字と全角括弧（`（1）`、`【1】`、`「1」` など）を組み合わせて入力した際（例：`A[あいうえおかきくけこ（1）]`）、自動折り返し位置の計算で動作が重くなったり停止したりする問題を修正しました。
- **全33種類の Mermaid ダイアグラムにおける多言語文字の解析を強化**:
  - 日本語（`だ`・`む`・`場`・`校`・`項` など）や韓国語・中国語・絵文字などの UTF-8 マルチバイト文字が空白文字と誤認されないよう改善し、ラベル・タイトル・設定値・太字／斜体記法がすべての図表で正確に描画されるようになりました。
- **高速な編集・正規表現一括置換・ライブプレビュー**:
  - 大容量ファイルの高速編集、全角・多言語対応の正規表現検索・一括置換、Markdown / LaTeX 数式 / Mermaid 図表のリアルタイムプレビューが軽快に動作します。

---

## 🇺🇸 English
Thank you for using **miu**.
Version 1.0.49 fixes a freeze when wrapping Mermaid diagram labels containing full-width brackets and CJK characters, and strengthens multi-byte UTF-8 character handling across all 33 Mermaid diagram types.

### 🛠 Key Improvements & Fixes
- **Fixed Mermaid Diagram Line-Wrapping Freeze with Full-Width Brackets**:
  - Resolved an issue where node labels combining Japanese, Korean, or Chinese text with full-width brackets (such as `（1）`, `【1】`, or `「1」`, e.g., `A[あいうえおかきくけこ（1）]`) could cause the preview to slow down or stop responding during line-wrap calculation.
- **Improved Multi-Byte UTF-8 Character Parsing Across All 33 Mermaid Diagrams**:
  - Prevented UTF-8 continuation bytes inside Japanese, Korean, Chinese, and other multi-byte characters from being misinterpreted as whitespace in labels, titles, configuration values, and inline bold/italic formatting.
- **High-Speed Editing, Multi-Language Regex & Live Preview**:
  - Smooth editing of large files, accurate Unicode regular expression Find & Replace, and instant offline Markdown, LaTeX & Mermaid Live Preview.

---

## 🇨🇳 简体中文 (Simplified Chinese)
感谢您使用 **miu**。
v1.0.49 修复了 Mermaid 图表预览中包含全角括号的中日韩标签在自动换行时可能出现卡顿或无响应的问题，并全面强化了 33 种图表的 UTF-8 多字节字符解析。

- **修复 Mermaid 图表全角括号与长文本自动换行卡顿问题**：修复了在流程图等节点标签中同时使用中文、日文、韩文与全角括号（如 `（1）`、`【1】`、`「1」` 等）时，自动换行计算可能导致预览变慢或停止响应的问题。
- **强化全部 33 种 Mermaid 图表的多语言字符解析**：修复了部分 UTF-8 多字节字符在标签、标题、配置项及粗体/斜体语法中被误判为空白字符的问题，确保所有图表准确渲染。

---

## 🇹🇼 繁體中文 (Traditional Chinese)
感謝您使用 **miu**。
v1.0.49 修復了 Mermaid 圖表預覽中包含全形括號的中日韓標籤在自動換行時可能出現卡頓或停止回應的問題，並全面強化了 33 種圖表的 UTF-8 多位元組字元解析。

- **修復 Mermaid 圖表全形括號與長文字自動換行卡頓問題**：修復了在流程圖等節點標籤中同時使用中文、日文、韓文與全形括號（如 `（1）`、`【1】`、`「1」` 等）時，自動換行計算可能導致預覽變慢或停止回應的問題。
- **強化全部 33 種 Mermaid 圖表的多語言字元解析**：修復了部分 UTF-8 多位元組字元在標籤、標題、設定值及粗體／斜體語法中被誤判為空白字元的問題，確保所有圖表準確繪製。

---

## 🇰🇷 한국어 (Korean)
언제나 **miu**를 이용해 주셔서 감사합니다.
v1.0.49에서는 Mermaid 다이어그램 미리보기에서 전각 괄호가 포함된 다국어 라벨을 줄바꿈할 때 동작이 느려지거나 멈추는 문제를 수정하고, 33종의 모든 다이어그램에서 UTF-8 다중 바이트 문자 처리를 개선했습니다.

- **Mermaid 다이어그램 전각 괄호 및 긴 라벨 자동 줄바꿈 멈춤 현상 수정**: 플로차트 등의 노드 라벨에 한국어·일본어·중국어 등 전각 문자와 전각 괄호(`（1）`, `【1】`, `「1」` 등)를 함께 입력했을 때 자동 줄바꿈 계산 중 미리보기가 무거워지거나 응답하지 않던 문제를 해결했습니다.
- **전체 33종 Mermaid 다이어그램의 다국어 문자 파싱 강화**: UTF-8 다중 바이트 문자가 공백 문자로 오인되지 않도록 개선하여 라벨·제목·굵게/기울임꼴 서식이 정확하게 렌더링됩니다.
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

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
v1.0.51 では、全プラットフォームにわたる**ライブプレビューの完全オフライン分離・セキュリティ強化**、**全65言語シンタックス＆Mermaid図表エンジンの精度向上**、および**アトミック保存・大容量処理・多言語IME入力の信頼性強化**を実施しました。

### ✨ 主な改善・セキュリティ強化内容
- **ライブプレビューの完全オフライン分離・セキュリティ強化**:
  - Markdown / HTML / SVG / Mermaid / LaTeX プレビューに厳格な Content Security Policy（CSP）と外部通信遮断（WebView2 / WKWebView / Android WebView）を適用し、外部リソースへの意図しない通信やスクリプト混入を防止しました。
  - タスクリストのチェック切替ブリッジに暗号論的ノンス（ワンタイムトークン）検証を導入し、`<` で始まる非 Web テキストファイルの安全なエスケープ表示および長文インライン強調記法の線形時間 `O(N)` パースに対応しました。
- **シンタックスハイライト＆Mermaid / LaTeX エンジンの精度・安定性向上**:
  - Python 三重引用符や全65言語のブロックコメント終端解析の精度向上、CRLF 改行を含む未閉鎖 Markdown コードフェンスの高速ハイライト、Prolog `-->` 演算子・Batch `REM` 境界・Rust/F#/Haskell シングルクォート解析を改善しました。
  - Mermaid `stateDiagram-v2` の複合状態、入れ子 `block-beta`、各種数値チャートの境界値保護と Emmet 展開の上限保護を強化しました。
- **ファイル保存の安全性（アトミック保存）・大容量処理・多言語入力の改善**:
  - アトミック保存と安全なメモリマップ（`mmap`）再マッピングにより上書き保存時のデータ保護を強化し、数GBクラスの分割保存に対応しました。
  - UTF-16 サロゲートペア（絵文字・4バイト文字）の IME 削除精度向上、モノクロ（1-bit）グリフの鮮明な描画、外部変更通知「再読み込み」の多言語対応を改善しました。

---

## 🇺🇸 English
Thank you for using **miu**.
Version 1.0.51 delivers comprehensive **Live Preview offline isolation & security hardening**, **syntax highlighting & Mermaid/LaTeX engine precision improvements**, and **atomic file-saving, large-file, and IME input reliability enhancements** across all platforms.

### ✨ Key Security, Stability & Accuracy Improvements
- **Live Preview Offline Isolation & Security Hardening**:
  - Enforced strict offline Content Security Policy (CSP), network request blocking across WebView2 / WKWebView / Android WebView, and HTML tag/URL sanitization so previews remain 100% local and safe.
  - Added per-session cryptographic nonce verification for task-list checkbox toggling, safe `<pre>` escaping for non-Web text files starting with `<`, and linear-time `O(N)` inline Markdown emphasis parsing.
- **Syntax Highlighting & Mermaid / LaTeX Engine Precision**:
  - Improved Python triple-quoted string and block-comment delimiter progression across all 65 language categories, linear-time highlighting for unclosed CRLF Markdown code fences, Prolog `-->` operator & Batch `REM` word-boundary accuracy, and Rust/F#/Haskell single-quote lifetimes/identifiers.
  - Hardened Mermaid `stateDiagram-v2` composite states, nested `block-beta` layouts, numeric chart edge-case guards, and Emmet expansion caps.
- **Atomic File Saving, Large-File Reliability & Input Enhancements**:
  - Added atomic file saving with safe `mmap` remapping to protect files during saves, plus chunked multi-GB file writing on Windows.
  - Fixed UTF-16 surrogate-pair (emoji & 4-byte character) IME deletion, improved 1-bit monochrome font glyph rendering, and localized external file modification reload prompts.

---

## 🇨🇳 简体中文 (Simplified Chinese)
感谢您使用 **miu**。
v1.0.51 全面强化了**实时预览的离线隔离与安全防护**、**65 种语言语法高亮与 Mermaid/LaTeX 引擎精度**，并提升了**原子化文件保存、超大文件读写与多语言输入法（IME）稳定性**。

- **实时预览完全离线隔离与安全加固**：为 Markdown / HTML / SVG / Mermaid / LaTeX 预览启用严格的离线安全策略（CSP）、拦截外部网络请求、净化 HTML 标签与链接，并为任务列表勾选交互加入加密随机令牌（Nonce）校验，安全转义以 `<` 开头的纯文本文件，实现线性时间 `O(N)` 行内强调解析。
- **语法高亮与 Mermaid 图表引擎精度提升**：优化 Python 三引号与块注释闭合识别、包含 CRLF 换行的未闭合 Markdown 代码块高速高亮、Prolog `-->` 运算符、Batch `REM` 边界及 Rust/F#/Haskell 单引号解析；增强 Mermaid `stateDiagram-v2` 复合状态、`block-beta` 嵌套与数值图表的渲染稳定性。
- **原子化安全保存、大文件支持与输入体验优化**：引入原子化保存与安全的内存映射（`mmap`）重映射机制，支持数 GB 超大文件分块保存，修复 UTF-16 代理对（表情符号与四字节字符）的输入法退格删除问题，并优化单色点阵字体渲染与外部修改重新加载提示的多语言本地化。

---

## 🇹🇼 繁體中文 (Traditional Chinese)
感謝您使用 **miu**。
v1.0.51 全面強化了**即時預覽的離線隔離與安全防護**、**65 種語言語法突顯與 Mermaid/LaTeX 引擎精準度**，並提升了**原子化檔案儲存、超大檔案讀寫與多語言輸入法（IME）穩定性**。

- **即時預覽完全離線隔離與安全加固**：為 Markdown / HTML / SVG / Mermaid / LaTeX 預覽啟用嚴格的離線安全原則（CSP）、攔截外部網路請求、淨化 HTML 標籤與連結，並為工作清單核取方塊互動加入加密隨機權杖（Nonce）驗證，安全轉義以 `<` 開頭的純文字檔，實現線性時間 `O(N)` 行內強調解析。
- **語法突顯與 Mermaid 圖表引擎精準度提升**：最佳化 Python 三引號與區塊註解閉合識別、包含 CRLF 換行的未閉合 Markdown 程式碼區塊高速突顯、Prolog `-->` 運算子、Batch `REM` 邊界及 Rust/F#/Haskell 單引號解析；增強 Mermaid `stateDiagram-v2` 複合狀態、`block-beta` 巢狀與數值圖表的渲染穩定性。
- **原子化安全儲存、大檔案支援與輸入體驗最佳化**：導入原子化儲存與安全的記憶體映射（`mmap`）重映射機制，支援數 GB 超大檔案分塊儲存，修復 UTF-16 代理對（表情符號與四位元組字元）的輸入法退格刪除問題，並最佳化單色點陣字體渲染與外部修改重新載入提示的多語言在地化。

---

## 🇰🇷 한국어 (Korean)
언제나 **miu**를 이용해 주셔서 감사합니다.
v1.0.51에서는 전 플랫폼에 걸쳐 **실시간 미리보기 완전 오프라인 격리 및 보안 강화**, **65개 언어 구문 강조 및 Mermaid/LaTeX 엔진 정밀도 향상**, 그리고 **원자적(Atomic) 파일 저장·대용량 처리·다국어 IME 입력 안정성 개선**을 진행했습니다.

- **실시간 미리보기 오프라인 격리 및 보안 강화**: Markdown / HTML / SVG / Mermaid / LaTeX 미리보기에 엄격한 오프라인 CSP 및 외부 네트워크 차단, HTML/URL 살균 처리를 적용하고 체크리스트 토글 브리지에 암호화 난스(Nonce) 검증을 도입했습니다. 또한 `<`로 시작하는 일반 텍스트의 안전한 이스케이프 표시 및 선형 시간 `O(N)` 인라인 강조 파싱을 지원합니다.
- **구문 강조 및 Mermaid 다이어그램 엔진 정밀도 향상**: Python 삼중 따옴표 및 블록 주석 종결 인식 개선, CRLF 개행을 포함한 미종결 Markdown 코드 블록의 초고속 강조, Prolog `-->` 연산자·Batch `REM` 경계·Rust/F#/Haskell 작은따옴표 파싱 개선 및 Mermaid `stateDiagram-v2` 복합 상태·`block-beta`·수치 차트 렌더링 안정성을 강화했습니다.
- **원자적 파일 저장·대용량 처리 및 입력 개선**: 원자적 파일 저장 및 안전한 메모리 맵(`mmap`) 재매핑으로 저장 중 데이터 보호를 강화하고 수 GB 대용량 분할 저장을 지원합니다. 또한 UTF-16 서로게이트 페어(이모지·4바이트 문자) IME 삭제 정확도 향상, 모노크롬 글리프 렌더링 개선 및 외부 파일 변경 새로고침 다국어 번역을 보강했습니다.
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

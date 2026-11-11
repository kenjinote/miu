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
v1.0.50 では、全プラットフォーム共通のシンタックスハイライト＆コメント切替エンジンを大幅に拡充し、**全65言語カテゴリ（230種類以上の拡張子・ビルド設定ファイル）**に対応しました。

### ✨ 主な新機能・強化内容
- **Lisp 系言語のフルサポートを追加**:
  - Common Lisp (`.lisp`, `.lsp`, `.cl`, `.asd`, `.ros`)、Emacs Lisp (`.el`, `.emacs`)、Scheme (`.scm`, `.ss`, `.sld`)、Racket (`.rkt`)、Clojure (`.clj`, `.cljs`, `.cljc`, `.edn`)、Hy (`.hy`)、Fennel (`.fnl`)、Janet (`.janet`) のシンタックスハイライト、`;` 行コメント切替、`#| ... |#` ブロックコメントに対応しました。
- **アセンブリ言語（Assembly）の大幅強化**:
  - x86/x64 (SSE/AVX/AVX-512)、ARM/AArch64、RISC-V、MIPS、NASM/MASM/GAS、WebAssembly Text Format (`.wat`, `.wast`)、`.asm`, `.s`, `.inc`, `.nasm`, `.yasm`, `.masm`, `.riscv`, `.mips` 等の命令・レジスタ・ディレクティブのハイライトおよび `;` 行コメント／`/* ... */` ブロックコメントに対応しました。
- **15種類の新規プログラミング言語カテゴリを追加**:
  - **Objective-C / Objective-C++** (`.m`, `.mm`)、**Groovy / Gradle / Jenkinsfile** (`.groovy`, `.gradle`, `Jenkinsfile`)、**OCaml / Standard ML / Reason / ReScript** (`.ml`, `.mli`, `.sml`, `.re`, `.res`)、**Erlang** (`.erl`, `.hrl`)、**Fortran** (`.f90`, `.f`, `.f95`, `.f03`, `.f08`)、**COBOL** (`.cob`, `.cbl`, `.cpy`)、**Prisma** (`.prisma`)、**Solidity** (`.sol`)、**Nim** (`.nim`, `.nims`)、**D言語** (`.d`, `.di`)、**Pascal / Delphi** (`.pas`, `.dpr`, `.lpr`)、**Ada** (`.adb`, `.ads`)、**Prolog** (`.pro`, `.prolog`)、**Vimscript** (`.vim`, `.vimrc`, `_vimrc`)、**Nix** (`.nix`) に新たに対応しました。
- **Apple Metal シェーダー (`.metal`) や多数のビルド定義・ドットファイルへの対応拡大**:
  - `.metal`, `.mts`, `.cts`, `.jsonl`, `.ndjson`, `.pyx`, `.bzl`, `BUILD`, `WORKSPACE`, `Gemfile`, `Rakefile`, `Podfile`, `Fastfile`, `Justfile`, `.gitignore`, `.editorconfig`, `.gitconfig`, `.fish`, `.bib`, `.sty`, `.cls`, `.sbt` など 70種類以上の拡張子・設定ファイルを追加しました。

---

## 🇺🇸 English
Thank you for using **miu**.
Version 1.0.50 significantly expands the cross-platform syntax highlighting and comment toggling engine to support **65 language categories (over 230 file extensions, build files, and dotfiles)**.

### ✨ Key New Features & Enhancements
- **Full Lisp Family Language Support**:
  - Added syntax highlighting, `;` line comment toggling, and `#| ... |#` block comments for Common Lisp (`.lisp`, `.lsp`, `.cl`, `.asd`, `.ros`), Emacs Lisp (`.el`, `.emacs`), Scheme (`.scm`, `.ss`, `.sld`), Racket (`.rkt`), Clojure (`.clj`, `.cljs`, `.cljc`, `.edn`), Hy (`.hy`), Fennel (`.fnl`), and Janet (`.janet`).
- **Major Assembly Language Enhancements**:
  - Expanded support for x86/x64 (SSE/AVX/AVX-512), ARM/AArch64, RISC-V, MIPS, NASM/MASM/GAS directives, and WebAssembly Text Format (`.wat`, `.wast`) across `.asm`, `.s`, `.inc`, `.nasm`, `.yasm`, `.masm`, `.riscv`, and `.mips`, with `;` line comments and `/* ... */` block comments.
- **15 New Programming Language Categories**:
  - Added **Objective-C / Objective-C++** (`.m`, `.mm`), **Groovy / Gradle / Jenkinsfile** (`.groovy`, `.gradle`, `Jenkinsfile`), **OCaml / Standard ML / Reason / ReScript** (`.ml`, `.mli`, `.sml`, `.re`, `.res`), **Erlang** (`.erl`, `.hrl`), **Fortran** (`.f90`, `.f`, `.f95`, `.f03`, `.f08`), **COBOL** (`.cob`, `.cbl`, `.cpy`), **Prisma** (`.prisma`), **Solidity** (`.sol`), **Nim** (`.nim`, `.nims`), **D** (`.d`, `.di`), **Pascal / Delphi** (`.pas`, `.dpr`, `.lpr`), **Ada** (`.adb`, `.ads`), **Prolog** (`.pro`, `.prolog`), **Vimscript** (`.vim`, `.vimrc`, `_vimrc`), and **Nix** (`.nix`).
- **Apple Metal Shaders (`.metal`) & 70+ Additional Extensions / Dotfiles**:
  - Added support for `.metal`, `.mts`, `.cts`, `.jsonl`, `.ndjson`, `.pyx`, `.bzl`, `BUILD`, `WORKSPACE`, `Gemfile`, `Rakefile`, `Podfile`, `Fastfile`, `Justfile`, `.gitignore`, `.editorconfig`, `.gitconfig`, `.fish`, `.bib`, `.sty`, `.cls`, `.sbt`, and more.

---

## 🇨🇳 简体中文 (Simplified Chinese)
感谢您使用 **miu**。
v1.0.50 大幅扩展了全平台语法高亮与注释切换引擎，现已支持 **65 种语言类别（超过 230 种扩展名与构建/配置文件）**。

- **新增 Lisp 系列语言完整支持**：支持 Common Lisp (`.lisp`, `.lsp`, `.cl`)、Emacs Lisp (`.el`)、Scheme (`.scm`)、Racket (`.rkt`)、Clojure (`.clj`, `.cljs`, `.edn`)、Hy、Fennel、Janet 的语法高亮、`;` 行注释切换与 `#| ... |#` 块注释。
- **全面强化汇编语言 (Assembly) 支持**：涵盖 x86/x64 (SSE/AVX/AVX-512)、ARM/AArch64、RISC-V、MIPS、NASM/MASM/GAS 以及 WebAssembly 文本格式 (`.wat`, `.wast`)。
- **新增 15 种编程语言类别**：新增 Objective-C/C++ (`.m`, `.mm`)、Groovy/Gradle/Jenkinsfile、OCaml/SML/ReScript、Erlang、Fortran、COBOL、Prisma、Solidity、Nim、D 语言、Pascal/Delphi、Ada、Prolog、Vimscript (`.vim`, `.vimrc`) 与 Nix (`.nix`)。
- **新增 Apple Metal 着色器 (`.metal`) 及 70 余种扩展名与配置文件**：支持 `.metal`, `.mts`, `.cts`, `.jsonl`, `.pyx`, `.bzl`, `BUILD`, `WORKSPACE`, `Gemfile`, `Rakefile`, `Podfile`, `.gitignore`, `.editorconfig`, `.gitconfig`, `.fish`, `.bib`, `.sbt` 等。

---

## 🇹🇼 繁體中文 (Traditional Chinese)
感謝您使用 **miu**。
v1.0.50 大幅擴充了全平台語法突顯與註解切換引擎，現已支援 **65 種語言類別（超過 230 種副檔名與建置／設定檔）**。

- **新增 Lisp 系列語言完整支援**：支援 Common Lisp (`.lisp`, `.lsp`, `.cl`)、Emacs Lisp (`.el`)、Scheme (`.scm`)、Racket (`.rkt`)、Clojure (`.clj`, `.cljs`, `.edn`)、Hy、Fennel、Janet 的語法突顯、`;` 行註解切換與 `#| ... |#` 區塊註解。
- **全面強化組合語言 (Assembly) 支援**：涵蓋 x86/x64 (SSE/AVX/AVX-512)、ARM/AArch64、RISC-V、MIPS、NASM/MASM/GAS 以及 WebAssembly 文字格式 (`.wat`, `.wast`)。
- **新增 15 種程式語言類別**：新增 Objective-C/C++ (`.m`, `.mm`)、Groovy/Gradle/Jenkinsfile、OCaml/SML/ReScript、Erlang、Fortran、COBOL、Prisma、Solidity、Nim、D 語言、Pascal/Delphi、Ada、Prolog、Vimscript (`.vim`, `.vimrc`) 與 Nix (`.nix`)。
- **新增 Apple Metal 著色器 (`.metal`) 及 70 餘種副檔名與設定檔**：支援 `.metal`, `.mts`, `.cts`, `.jsonl`, `.pyx`, `.bzl`, `BUILD`, `WORKSPACE`, `Gemfile`, `Rakefile`, `Podfile`, `.gitignore`, `.editorconfig`, `.gitconfig`, `.fish`, `.bib`, `.sbt` 等。

---

## 🇰🇷 한국어 (Korean)
언제나 **miu**를 이용해 주셔서 감사합니다.
v1.0.50에서는 전 플랫폼 공통 구문 강조 및 주석 토글 엔진을 대폭 확장하여 **총 65개 언어 카테고리(230종 이상의 확장자 및 빌드·설정 파일)**를 지원합니다.

- **Lisp 계열 언어 완벽 지원 추가**: Common Lisp(`.lisp`, `.lsp`, `.cl`), Emacs Lisp(`.el`), Scheme(`.scm`), Racket(`.rkt`), Clojure(`.clj`, `.cljs`, `.edn`), Hy, Fennel, Janet의 구문 강조 및 `;` 줄 주석 토글, `#| ... |#` 블록 주석을 지원합니다.
- **어셈블리어(Assembly) 대폭 강화**: x86/x64(SSE/AVX/AVX-512), ARM/AArch64, RISC-V, MIPS, NASM/MASM/GAS 및 WebAssembly 텍스트 포맷(`.wat`, `.wast`)을 지원합니다.
- **15종의 신규 프로그래밍 언어 추가**: Objective-C/C++(`.m`, `.mm`), Groovy/Gradle/Jenkinsfile, OCaml/SML/ReScript, Erlang, Fortran, COBOL, Prisma, Solidity, Nim, D 언어, Pascal/Delphi, Ada, Prolog, Vimscript(`.vim`, `.vimrc`), Nix(`.nix`)를 새롭게 지원합니다.
- **Apple Metal 셰이더(`.metal`) 및 70종 이상의 확장자·설정 파일 추가**: `.metal`, `.mts`, `.cts`, `.jsonl`, `.pyx`, `.bzl`, `BUILD`, `WORKSPACE`, `Gemfile`, `Rakefile`, `Podfile`, `.gitignore`, `.editorconfig`, `.gitconfig`, `.fish`, `.bib`, `.sbt` 등을 지원합니다.
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

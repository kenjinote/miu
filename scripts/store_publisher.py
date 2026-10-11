import os
import sys
import json
import time
import zipfile
import subprocess
import requests

DEFAULT_RELEASE_NOTES = {
    "1.0.30": {
        "ja-jp": """miu v1.0.30 リリースノート
✨ 新機能・改善点

ベトナム語・ウクライナ語・インドネシア語の完全対応

・多言語対応の拡大:
  急速に成長するIT開発者大国・新興市場であるベトナム語（vi-vn）、ウクライナ語（uk-ua）、インドネシア語（id-id）のUI翻訳（メニュー・F1ヘルプ・ダイアログ等）をアプリ本体およびストアに完全追加しました。
・美しいDirect2D描画:
  声調記号付きラテン文字（ベトナム語）やキリル文字（ウクライナ語）もDirect2D/DirectWriteにより美しく鮮明に描画されます。
・管理者権限が必要なファイルの保存（UAC昇格保存）:
  Cドライブ直下やシステム保護領域、読み取り専用ファイルの編集・上書き保存に対応。""",

        "en-us": """miu v1.0.30 Release Notes
✨ What's New & Improvements

Full Support for Vietnamese, Ukrainian & Indonesian

- Expanded Multilingual Support:
  Added complete native UI localization (menus, F1 help, shortcuts, dialogs) and Store listings for Vietnamese (vi-vn), Ukrainian (uk-ua), and Indonesian (id-id).
- Crisp Direct2D Typography:
  Tone-marked diacriticals (Vietnamese) and Cyrillic glyphs (Ukrainian) render with pristine clarity via Direct2D/DirectWrite hardware acceleration.
- Save Protected Files (UAC Elevation):
  Save files in protected folders (such as C:\\ root) or read-only files safely with administrator privileges.""",

        "zh-hans": """miu v1.0.30 更新日志
✨ 新功能与改进

全面支持越南语、乌克兰语与印度尼西亚语

- 多语言支持拓展：
  新增对越南语（vi-vn）、乌克兰语（uk-ua）与印度尼西亚语（id-id）的完整本地化支持（涵盖菜单、F1 帮助指南、对话框与商店页面）。
- 高清 Direct2D 文字渲染：
  通过 Direct2D/DirectWrite 硬件加速，清晰锐利地呈现越南语声调符号与乌克兰语西里尔字母。
- 管理员权限保存（UAC 提权保存）：
  支持以管理员权限保存位于 C 盘根目录、系统保护目录或具有只读属性的文件。""",

        "ko": """miu v1.0.30 릴리스 노트
✨ 새로운 기능 및 개선 사항

베트남어, 우크라이나어, 인도네시아어 완전 지원

- 다국어 지원 대폭 확대:
  베트남어(vi-vn), 우크라이나어(uk-ua), 인도네시아어(id-id)에 대한 앱 UI(메뉴, F1 도움말, 대화상자) 및 스토어 완벽 지원이 추가되었습니다.
- 선명한 Direct2D 텍스트 렌더링:
  베트남어 성조 기호 및 우크라이나어 키릴 문자가 Direct2D/DirectWrite 가속을 통해 선명하고 부드럽게 렌더링됩니다.
- 관리자 권한 파일 저장(UAC 승격):
  C 드라이브 루트, 시스템 보호 폴더 및 읽기 전용 속성의 파일 덮어쓰기 저장을 지원합니다."""
    },
    "1.0.29": {
        "ja-jp": """miu v1.0.29 リリースノート
✨ 新機能・改善点

管理者権限が必要なファイルの保存に対応（UAC昇格保存）

・保護されたファイルの上書き保存に対応:
  Cドライブ直下やシステム保護領域、アクセス権が制限されたファイル、読み取り専用ファイルを編集した際、管理者権限での上書き保存が可能になりました。
・安全な確認ダイアログとUAC連携:
  アクセス拒否を検知するとセキュリティシールド付きの確認ダイアログを表示し、ユーザーの許可を経てバックグラウンドで安全にファイルを更新します。
・編集中の内容を保護する安全設計:
  保存確認ダイアログで「いいえ」を選んだ場合やWindowsのUAC画面をキャンセルした場合でも、入力・編集中のテキストやUndo履歴は一切失われず、そのまま編集を継続できます。
・「名前を付けて保存」のパス処理改善:
  Cドライブ直下（C:\\）などのルートディレクトリを対象とした際の初期フォルダ指定を適正化しました。
・多言語対応:
  新ダイアログを日本語を含む全14言語に完全ローカライズしました。""",

        "en-us": """miu v1.0.29 Release Notes
✨ What's New & Improvements

Save Protected Files with Administrator Privileges (UAC Elevation)

- Save Protected & System Files:
  Files located in protected locations (such as C:\\ drive root, system folders, or read-only files) can now be saved safely via administrator elevation.
- Seamless UAC Confirmation:
  When access denial is detected, a security shield dialog prompts for confirmation, launching an elevated helper to update the file upon user approval.
- Edit Preservation Guarantee:
  If the elevation dialog is declined or the UAC prompt is cancelled, in-memory buffer edits, piece table state, and undo history are 100% preserved without any data loss.
- Improved 'Save As' Path Handling:
  Fixed initial directory handling when saving into root drive paths like C:\\.
- Full Multilingual Support:
  Localized prompt messages across all 14 supported languages.""",

        "zh-hans": """miu v1.0.29 更新日志
✨ 新功能与改进

支持以管理员权限保存受保护文件（UAC 提权保存）

- 保存受保护与系统文件：
  现在支持以管理员权限保存位于 C 盘根目录、系统保护目录或具有只读属性的文件。
- 无缝 UAC 提权确认：
  检测到访问被拒绝时，将弹出带有安全盾牌图标的提示对话框，在用户许可后通过后台提权助手安全完成文件保存。
- 绝不丢失编辑内容的保护机制：
  如果在提示框中选择“否”或取消了 Windows UAC 提权弹窗，内存中的文本编辑内容与撤销历史将被 100% 完整保留，绝不丢失任何修改。
- 优化“另存为”根目录路径：
  修复了在 C:\\ 等根目录下另存为时初始文件夹路径的设置问题。
- 多语言全面支持：
  新增提示已完整本地化支持包括中文在内的全部 14 种语言。""",

        "ko": """miu v1.0.29 릴리스 노트
✨ 새로운 기능 및 개선 사항

관리자 권한이 필요한 파일 저장 지원 (UAC 승격 저장)

- 보호된 파일 및 시스템 파일 저장 지원:
  C 드라이브 루트, 시스템 보호 폴더 및 읽기 전용 속성의 파일을 편집한 경우 관리자 권한으로 안전하게 덮어쓸 수 있습니다.
- 원활한 UAC 확인 대화상자:
  액세스 거부 감지 시 보안 쉴드 아이콘이 포함된 확인 창이 표시되며, 사용자 동의 시 백그라운드 관리자 프로세스를 통해 파일을 업데이트합니다.
- 편집 내용 보호 설계:
  저장 확인 창에서 '아니요'를 선택하거나 UAC 승격을 취소하더라도 메모리 상의 편집 내용과 실행 취소(Undo) 기록이 완벽하게 보존됩니다.
- '다른 이름으로 저장' 경로 처리 개선:
  C:\\ 등 루트 디렉터리에 저장할 때 초기 폴더 경로가 올바르게 지정되도록 수정했습니다.
- 다국어 지원:
  새로운 확인 메시지를 한국어를 포함한 14개 언어로 완전 현지화했습니다."""
    },
    "1.0.28": {
        "ja-jp": """miu v1.0.28 リリースノート
✨ 新機能・改善点

テキスト折り返しモード時のカーソル移動を大幅に改善

・右矢印（→）キーの挙動改善:
  折り返し行の行末へ進めた際、表示上の行末にカーソルが確実に表示され、続けて押下した際は次行の2文字目先頭へ直観的に移動するようになりました。
・HOME / END キーのスマートジャンプ:
  1回目のキー押下で表示行の行頭・行末へジャンプし、すでに表示行頭・行末にある状態での再押下で論理行頭・行末へジャンプする2段階移動に対応しました。
・上下（↑ / ↓）キー移動の正確化:
  表示行末にカーソルがある状態からの上下移動時、文字位置がズレることなく自然に直上・直下の行へ移動するよう調整しました。
・スクロール追従の強化:
  長大な折り返し行で Ctrl+END 等により行末へジャンプした際も、カーソルが可視領域に確実に収まるよう自動スクロールを強化しました。

F1 ヘルプ画面のダークモード表示を最適化

・ダークモード時の背景色をより目に優しい暗色半透明スタイルに調整し、テキストの視認性を向上させました。""",

        "en-us": """miu v1.0.28 Release Notes
✨ What's New & Improvements

Refined Cursor Navigation in Word Wrap Mode

- Enhanced Right Arrow (->) Navigation:
  When advancing across wrapped lines, the cursor is now clearly displayed at the visual line end before stepping forward to the next line.
- Smart Home / End Key Jumps:
  Pressing Home/End now first jumps to the visual line start/end. Pressing it again jumps to the logical line beginning/end.
- Accurate Up / Down Arrow Navigation:
  Moving up or down from visual line ends now transitions accurately to the corresponding position on adjacent visual lines.
- Improved Caret Scroll Tracking:
  Jumping to the line end (e.g. via Ctrl+End) on long wrapped lines reliably scrolls the caret into view.

F1 Help Overlay Dark Mode Polish

- Restored and optimized the elegant dark semi-transparent styling for the F1 help overlay in dark mode, ensuring superior readability.""",

        "zh-hans": """miu v1.0.28 更新日志
✨ 新功能与改进

自动换行模式下的光标导航体验全面优化

- 优化右方向键（->）光标移动：
  在换行边界处，光标现在会清晰显示在当前视觉行末，再次右移即可平滑过渡至下一行。
- 智能 Home / End 跳转：
  首次按下 Home/End 键跳转至当前屏幕视觉行的行首/行末，再次按下跳转至逻辑行的首尾。
- 修复上下方向键（↑ / ↓）移动：
  从视觉行末向上或向下移动时，光标能精准定位至相邻视觉行。
- 增强光标可视滚动追踪：
  在超长换行文本中按 Ctrl+End 跳转至行尾时，视图会自动准确滚动并保持光标可见。

F1 帮助界面深色模式显示优化

- 优化了深色模式下的半透明暗色背景与边框效果，文字对比度更高、阅读体验更舒适。""",

        "ko": """miu v1.0.28 릴리스 노트
✨ 새로운 기능 및 개선 사항

자동 줄 바꿈 모드의 커서 이동 대폭 개선

- 오른쪽 화살표(->) 이동 개선:
  줄 바꿈 경계에서 시각적 행 끝에 커서가 정확히 표시된 후 다음 줄로 자연스럽게 이동합니다.
- 스마트 Home / End 키 이동:
  첫 번째 누름 시 화면 표시 행의 시작/끝으로 이동하고, 다시 누르면 논리적 줄의 시작/끝으로 이동하는 2단계 이동을 지원합니다.
- 위/아래 화살표(↑ / ↓) 이동 정밀화:
  시각적 행 끝에서 위/아래 키를 누를 때 인접 행으로 자연스럽고 정확하게 이동하도록 개선했습니다.
- 스크롤 추적 강화:
  긴 줄 바꿈 텍스트에서 Ctrl+End 등으로 줄 끝으로 이동할 때 커서가 항상 화면에 보이도록 자동 스크롤을 개선했습니다.

F1 도움말 다크 모드 표시 개선

- 다크 모드에서 배경을 은은한 어두운 반투명 스타일로 최적화하여 가독性を 높였습니다."""
    },
    "1.0.27": {
        "ja-jp": """miu v1.0.27 リリースノート
✨ 新機能・改善点

各種ダイアログのダークモード対応

検索・置換・指定行へ移動ダイアログが、Windowsのダークモード設定に合わせて自動でダークテーマ表示されるようになりました。

TaskDialog（保存確認・メッセージ画面）の刷新

ファイル保存確認などのダイアログを刷新し、ダークモードに対応しました。
OSの標準テーマAPIからフォント情報を動的取得することで、Windows標準のダイアログと完全に一致する美しい文字配置とレイアウトを実現しています。

F1 ヘルプ画面のライトモード表示を改善

ライトモード時、白背景のエディタ上でも見やすい淡いグレー背景の半透明スタイルに改良し、黒テキストの高い可読性とモダンなデザインを両立させました。""",

        "en-us": """miu v1.0.27 Release Notes
✨ What's New & Improvements

Dark Mode Support for All Dialogs

Find, Replace, and Go to Line dialogs now seamlessly adapt to Windows Dark Mode settings.

Refined TaskDialog & Confirmation Prompts

Save confirmation and message dialogs have been redesigned with full Dark Mode support.
Font metrics and styles are dynamically retrieved from Windows theme APIs, achieving a pixel-perfect match with standard Windows dialogs.

Improved F1 Help in Light Mode

In Light Mode, the help overlay now features a refined semi-transparent soft gray background, ensuring optimal contrast and readability for black text against bright editor backgrounds.""",

        "zh-hans": """miu v1.0.27 更新日志
✨ 新功能与改进

各种对话框支持深色模式

查找、替换和跳转行号对话框现已全面适配 Windows 深色模式设置。

TaskDialog 与保存确认对话框优化

全新重构了文件保存确认等对话框，并完美支持深色模式。
通过系统原生主题 API 动态获取字体与度量信息，实现了与 Windows 原生对话框像素级一致的排版与布局。

F1 帮助菜单浅色模式优化

在浅色模式下，帮助菜单采用更易阅读的淡灰色半透明背景，确保在白色编辑器背景下黑色文字保持清晰易读与现代质感。""",

        "ko": """miu v1.0.27 릴리스 노트
✨ 새로운 기능 및 개선 사항

각종 대화상자 다크 모드 지원

찾기, 바꾸기, 지정 줄로 이동 대화상자가 Windows 다크 모드 설정에 맞춰 자동으로 어두운 테마로 표시됩니다.

TaskDialog(저장 확인 및 메시지 창) 개편

파일 저장 확인 등의 대화상자를 새롭게 개편하여 다크 모드를 완벽 지원합니다.
OS 표준 테마 API로부터 폰트 정보를 동적으로 가져와 Windows 표준 대화상자와 픽셀 단위로 정확하게 일치하는 레이아웃을 구현했습니다.

F1 도움말 라이트 모드 표시 개선

라이트 모드에서 흰색背景의 에디터 위에서도 가독성이 뛰어나도록 은은한 회색 반투명 스타일을 적용하여 선명한 텍스트와 현대적인 디자인을 제공합니다."""
    }
}

def get_store_credentials(root_dir):
    creds_file = os.path.join(root_dir, "scripts", "store_credentials.json")
    if os.path.exists(creds_file):
        with open(creds_file, "r", encoding="utf-8") as f:
            data = json.load(f)
            return (
                data.get("tenant_id"),
                data.get("client_id"),
                data.get("client_secret"),
                data.get("product_id")
            )
    return (
        os.environ.get("STORE_TENANT_ID"),
        os.environ.get("STORE_CLIENT_ID"),
        os.environ.get("STORE_CLIENT_SECRET"),
        os.environ.get("STORE_PRODUCT_ID")
    )

def build_store_package(root_dir, version, msbuild_path):
    print("=== Building Store Package (.appxbundle) ===")
    wapproj_path = os.path.join(root_dir, "package", "package.wapproj")
    cmd = [
        msbuild_path,
        wapproj_path,
        "/p:Configuration=Release",
        "/p:Platform=x64",
        "/p:AppxBundle=Always",
        "/p:UapAppxPackageBuildMode=StoreUpload"
    ]
    res = subprocess.run(cmd)
    if res.returncode != 0:
        raise RuntimeError(f"Building Store package failed with code {res.returncode}")

    bundle_dir = os.path.join(root_dir, "package", "AppPackages", f"package_{version}.0_Test")
    bundle_path = os.path.join(bundle_dir, f"package_{version}.0_x64.appxbundle")
    if not os.path.exists(bundle_path):
        candidates = [
            os.path.join(root_dir, "package", "AppPackages", f"package_{version}.0_x64.appxbundle"),
            os.path.join(bundle_dir, f"package_{version}.0_x64.msixbundle")
        ]
        for c in candidates:
            if os.path.exists(c):
                bundle_path = c
                break
    if not os.path.exists(bundle_path):
        raise FileNotFoundError(f"Store bundle not found at {bundle_path}")

    print(f"Store bundle built: {bundle_path} ({os.path.getsize(bundle_path)} bytes)")
    return bundle_path

def get_oauth_token(tenant_id, client_id, client_secret):
    print("Acquiring Microsoft Store API OAuth token...")
    token_url = f"https://login.microsoftonline.com/{tenant_id}/oauth2/token"
    data = {
        "grant_type": "client_credentials",
        "client_id": client_id,
        "client_secret": client_secret,
        "resource": "https://manage.devcenter.microsoft.com"
    }
    resp = requests.post(token_url, data=data)
    resp.raise_for_status()
    return resp.json()["access_token"]

def get_or_create_submission(headers, product_id):
    app_url = f"https://manage.devcenter.microsoft.com/v1.0/my/applications/{product_id}"
    app_resp = requests.get(app_url, headers=headers)
    app_resp.raise_for_status()
    app_data = app_resp.json()

    pending = app_data.get("pendingApplicationSubmission")
    if pending and pending.get("id"):
        sub_id = pending["id"]
        print(f"Found existing draft submission: {sub_id}")
        sub_url = f"{app_url}/submissions/{sub_id}"
        sub_resp = requests.get(sub_url, headers=headers)
        sub_resp.raise_for_status()
        return sub_resp.json()

    print("Creating new draft submission...")
    create_url = f"{app_url}/submissions"
    create_resp = requests.post(create_url, headers=headers)
    create_resp.raise_for_status()
    new_sub = create_resp.json()
    print(f"Created new draft submission: {new_sub.get('id')}")
    return new_sub

def upload_and_update_submission(headers, product_id, submission_data, bundle_path, version, release_notes=None):
    sub_id = submission_data["id"]
    file_upload_url = submission_data["fileUploadUrl"]
    bundle_filename = os.path.basename(bundle_path)
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    # 1. Create zip package with bundle and all 30 screenshots
    zip_path = os.path.splitext(bundle_path)[0] + ".zip"
    print(f"Packaging bundle and screenshots into zip: {zip_path}...")
    screenshots_dir = os.path.join(root_dir, "package", "screenshots")

    with zipfile.ZipFile(zip_path, 'w', compression=zipfile.ZIP_DEFLATED) as zf:
        zf.write(bundle_path, arcname=bundle_filename)
        if os.path.exists(screenshots_dir):
            for img_file in sorted(os.listdir(screenshots_dir)):
                if img_file.endswith(".png"):
                    full_p = os.path.join(screenshots_dir, img_file)
                    zf.write(full_p, arcname=img_file)

    # 2. Upload zip to SAS URL
    print(f"Uploading zip ({os.path.getsize(zip_path)} bytes) to Store Blob Storage...")
    upload_url = file_upload_url.replace("+", "%2B")
    with open(zip_path, "rb") as f:
        up_resp = requests.put(
            upload_url,
            data=f,
            headers={"x-ms-blob-type": "BlockBlob"}
        )
    up_resp.raise_for_status()
    print("Zip package uploaded successfully.")

    # 3. Update applicationPackages
    existing_packages = submission_data.get("applicationPackages", [])
    pkg_filenames = [p.get("fileName") for p in existing_packages]

    if bundle_filename not in pkg_filenames:
        new_packages = []
        for pkg in existing_packages:
            if pkg.get("id"):  # Only existing packages with an ID can be marked PendingDelete
                pkg_copy = dict(pkg)
                pkg_copy["fileStatus"] = "PendingDelete"
                new_packages.append(pkg_copy)
        new_packages.append({
            "fileName": bundle_filename,
            "fileStatus": "PendingUpload",
            "minimumDirectXVersion": "None",
            "minimumSystemRam": "None"
        })
        submission_data["applicationPackages"] = new_packages

    # 4. Multi-language listings and screenshots
    listings = submission_data.setdefault("listings", {})

    try:
        from .store_metadata import METADATA_10_LANGS
    except ImportError:
        from store_metadata import METADATA_10_LANGS

    for lang, meta in METADATA_10_LANGS.items():
        lang_entry = listings.setdefault(lang, {})
        base = lang_entry.setdefault("baseListing", {})
        base["title"] = meta["title"]
        base["description"] = meta["description"]
        base["features"] = meta["features"]
        base["keywords"] = meta["keywords"]
        base["releaseNotes"] = meta["releaseNotes"]
        base["copyrightAndTrademarkInfo"] = "Copyright (C) 2026 kenjinote"
        base["supportContact"] = "https://github.com/kenjinote/miu"
        base["websiteUrl"] = "https://github.com/kenjinote/miu"
        base["privacyPolicy"] = "https://github.com/kenjinote/miu"
        base["licenseTerms"] = ""

        prefix = meta["prefix"]
        captions = meta.get("image_captions", ["", "", ""])
        img_list = []
        for i in (1, 2, 3):
            fname = f"{prefix}_{i}.png"
            img_list.append({
                "fileName": fname,
                "fileStatus": "PendingUpload",
                "imageType": "Screenshot",
                "description": captions[i-1]
            })
        base["images"] = img_list
        print(f"Configured listing and screenshots for [{lang}]")

    if "ko" in listings and "baseListing" in listings["ko"]:
        if not listings["ko"]["baseListing"].get("title"):
            listings["ko"]["baseListing"]["title"] = "miu"

    # Multi-language release notes for existing default languages
    notes_dict = release_notes
    if not notes_dict:
        notes_dict = DEFAULT_RELEASE_NOTES.get(version)
    if notes_dict:
        for lang, notes in notes_dict.items():
            if lang in listings and "baseListing" in listings[lang]:
                listings[lang]["baseListing"]["releaseNotes"] = notes
                print(f"Updated release notes for [{lang}]")

    # 5. PUT update to Dev Center API
    print(f"Updating submission {sub_id}...")
    put_url = f"https://manage.devcenter.microsoft.com/v1.0/my/applications/{product_id}/submissions/{sub_id}"
    put_resp = requests.put(put_url, headers=headers, json=submission_data)
    if put_resp.status_code != 200:
        raise RuntimeError(f"Failed to update submission: {put_resp.status_code} - {put_resp.text}")
    print("Submission updated successfully.")

def commit_submission(headers, product_id, submission_id):
    print(f"Committing submission {submission_id} to Microsoft Store...")
    commit_url = f"https://manage.devcenter.microsoft.com/v1.0/my/applications/{product_id}/submissions/{submission_id}/commit"
    res = requests.post(commit_url, headers=headers)
    if res.status_code not in (200, 202):
        raise RuntimeError(f"Failed to commit submission: {res.status_code} - {res.text}")
    print("Submission committed! It is now queued for certification.")

def poll_submission_status(headers, product_id, submission_id, max_attempts=15, interval=10):
    status_url = f"https://manage.devcenter.microsoft.com/v1.0/my/applications/{product_id}/submissions/{submission_id}/status"
    for i in range(max_attempts):
        time.sleep(interval)
        res = requests.get(status_url, headers=headers)
        if res.status_code == 200:
            status_data = res.json()
            st = status_data.get("status")
            print(f"Status check ({i+1}/{max_attempts}): {st}")
            if st not in ("CommitStarted", "PendingCommit"):
                return st
    return "CommitProcessing"

def release_to_store(root_dir, version, msbuild_path, skip_build=False, custom_notes=None, do_commit=True):
    tenant_id, client_id, client_secret, product_id = get_store_credentials(root_dir)
    if not all([tenant_id, client_id, client_secret, product_id]):
        print("[Notice] Microsoft Store credentials not found. Skipping Store release.")
        return False

    print(f"\n{'='*50}\nStarting Microsoft Store Release for v{version}\n{'='*50}")

    if not skip_build:
        bundle_path = build_store_package(root_dir, version, msbuild_path)
    else:
        bundle_path = os.path.join(root_dir, "package", "AppPackages", f"package_{version}.0_Test", f"package_{version}.0_x64.appxbundle")
        if not os.path.exists(bundle_path):
            raise FileNotFoundError(f"Bundle not found: {bundle_path}")

    token = get_oauth_token(tenant_id, client_id, client_secret)
    headers = {
        "Authorization": f"Bearer {token}",
        "Content-Type": "application/json"
    }

    sub_data = get_or_create_submission(headers, product_id)
    sub_id = sub_data["id"]

    upload_and_update_submission(headers, product_id, sub_data, bundle_path, version, custom_notes)

    if do_commit:
        commit_submission(headers, product_id, sub_id)
        final_status = poll_submission_status(headers, product_id, sub_id)
        print(f"Microsoft Store submission complete. Current status: {final_status}")
    else:
        print(f"Submission {sub_id} prepared as draft. Commit skipped (--no-store-commit).")

    return True

if __name__ == "__main__":
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    release_to_store(root, "1.0.27", "MSBuild.exe", skip_build=True, do_commit=False)

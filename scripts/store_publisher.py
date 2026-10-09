import os
import sys
import json
import time
import zipfile
import subprocess
import requests

DEFAULT_RELEASE_NOTES = {
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

    # 1. Create zip package
    zip_path = os.path.splitext(bundle_path)[0] + ".zip"
    print(f"Packaging bundle into zip: {zip_path}...")
    with zipfile.ZipFile(zip_path, 'w', compression=zipfile.ZIP_DEFLATED) as zf:
        zf.write(bundle_path, arcname=bundle_filename)

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

    # 4. Multi-language release notes
    notes_dict = release_notes
    if not notes_dict:
        notes_dict = DEFAULT_RELEASE_NOTES.get(version)
    if notes_dict:
        listings = submission_data.get("listings", {})
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

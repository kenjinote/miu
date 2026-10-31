import os
import sys
import json
import time
import argparse
import jwt
import requests

PACKAGE_NAME = "jp.hack.miu"
DEFAULT_AAB_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "Android", "app", "release", "app-release.aab"
)
DEFAULT_KEY_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "google_play_service_account.json"
)

RELEASE_NOTES_44 = [
    {
        "language": "ja-JP",
        "text": "v1.0.44:\n・49言語のシンタックスハイライト、Emmet展開、スマート自動インデントに対応\n・Markdown / HTML / SVG / 24種Mermaid図表 / KaTeX数式の分割ライブプレビューを搭載\n・折り返し表示、マルチカーソル、矩形選択、コメント切替、文字コード自動判別（40種以上）に対応"
    },
    {
        "language": "en-US",
        "text": "v1.0.44:\n- Added syntax highlighting for 49+ languages, Emmet expansion, and smart auto-indent\n- Added split-pane Live Preview for Markdown, HTML, SVG, 24 Mermaid diagram types, and KaTeX math\n- Added Word Wrap, multi-cursor, rectangular selection, comment toggling, and 40+ encoding auto-detection"
    }
]


def get_access_token(service_account_json_path: str) -> str:
    with open(service_account_json_path, "r", encoding="utf-8") as f:
        sa = json.load(f)
    now = int(time.time())
    payload = {
        "iss": sa["client_email"],
        "scope": "https://www.googleapis.com/auth/androidpublisher",
        "aud": sa.get("token_uri", "https://oauth2.googleapis.com/token"),
        "iat": now,
        "exp": now + 3600,
    }
    assertion = jwt.encode(
        payload,
        sa["private_key"],
        algorithm="RS256",
        headers={"kid": sa.get("private_key_id")},
    )
    resp = requests.post(
        sa.get("token_uri", "https://oauth2.googleapis.com/token"),
        data={
            "grant_type": "urn:ietf:params:oauth:grant-type:jwt-bearer",
            "assertion": assertion,
        },
        timeout=30,
    )
    resp.raise_for_status()
    return resp.json()["access_token"]


def upload_to_play_store(aab_path: str, key_path: str, track: str = "internal", status: str = "completed"):
    if not os.path.exists(aab_path):
        raise FileNotFoundError(f"AAB file not found: {aab_path}")
    if not os.path.exists(key_path):
        raise FileNotFoundError(
            f"Service account JSON key not found: {key_path}\n"
            "Place your Google Play Service Account JSON key at scripts/google_play_service_account.json "
            "or pass --key <path>."
        )

    print(f"Authenticating with service account: {key_path}")
    token = get_access_token(key_path)
    headers = {"Authorization": f"Bearer {token}"}
    base_url = f"https://androidpublisher.googleapis.com/androidpublisher/v3/applications/{PACKAGE_NAME}"
    upload_base_url = f"https://androidpublisher.googleapis.com/upload/androidpublisher/v3/applications/{PACKAGE_NAME}"

    # 1. Create a new edit
    print(f"Creating edit for {PACKAGE_NAME}...")
    r_edit = requests.post(f"{base_url}/edits", headers=headers, json={}, timeout=30)
    r_edit.raise_for_status()
    edit_id = r_edit.json()["id"]
    print(f"Created edit ID: {edit_id}")

    # 2. Upload AAB bundle
    aab_size = os.path.getsize(aab_path)
    print(f"Uploading {aab_path} ({aab_size:,} bytes)...")
    with open(aab_path, "rb") as f:
        aab_bytes = f.read()

    r_upload = requests.post(
        f"{upload_base_url}/edits/{edit_id}/bundles?uploadType=media",
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/octet-stream",
        },
        data=aab_bytes,
        timeout=600,
    )
    r_upload.raise_for_status()
    bundle_info = r_upload.json()
    version_code = bundle_info["versionCode"]
    print(f"Uploaded bundle versionCode: {version_code} (sha256: {bundle_info.get('sha256')})")

    # 3. Assign bundle to track (default: internal)
    print(f"Assigning versionCode {version_code} to track '{track}' (status={status})...")
    release_notes = RELEASE_NOTES_44
    try:
        from playstore_metadata_screenshots_sync import ANDROID_RELEASE_NOTES
        release_notes = [
            {"language": lang, "text": text}
            for lang, text in sorted(ANDROID_RELEASE_NOTES.items())
        ]
    except Exception:
        pass
    track_body = {
        "track": track,
        "releases": [
            {
                "name": "1.0.44",
                "versionCodes": [str(version_code)],
                "status": status,
                "releaseNotes": release_notes,
            }
        ],
    }
    r_track = requests.put(
        f"{base_url}/edits/{edit_id}/tracks/{track}",
        headers={**headers, "Content-Type": "application/json"},
        json=track_body,
        timeout=60,
    )
    r_track.raise_for_status()

    # 4. Commit edit
    print(f"Committing edit {edit_id}...")
    r_commit = requests.post(f"{base_url}/edits/{edit_id}:commit", headers=headers, timeout=60)
    r_commit.raise_for_status()
    print(f"Successfully published versionCode {version_code} to Google Play '{track}' track!")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Upload Android App Bundle (.aab) to Google Play Console")
    parser.add_argument("--aab", default=DEFAULT_AAB_PATH, help="Path to app-release.aab")
    parser.add_argument("--key", default=DEFAULT_KEY_PATH, help="Path to Google Play Service Account JSON key")
    parser.add_argument("--track", default="internal", choices=["internal", "alpha", "beta", "production"], help="Target track")
    parser.add_argument("--draft", action="store_true", help="Upload as draft release instead of completed")
    args = parser.parse_args()

    upload_to_play_store(
        aab_path=args.aab,
        key_path=args.key,
        track=args.track,
        status="draft" if args.draft else "completed",
    )

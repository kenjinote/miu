import os
import re
import sys
import json
import time
import struct
import zipfile
import argparse

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

PACKAGE_NAME = "jp.hack.miu"
DEFAULT_AAB_PATH = os.path.join(
    REPO_ROOT,
    "Android", "app", "build", "outputs", "bundle", "release", "app-release.aab"
)
DEFAULT_GRADLE_KTS_PATH = os.path.join(
    REPO_ROOT,
    "Android", "app", "build.gradle.kts"
)
DEFAULT_KEY_PATH = os.path.join(
    SCRIPT_DIR,
    "google_play_service_account.json"
)

REQUIRED_ABIS = ("arm64-v8a", "armeabi-v7a", "x86", "x86_64")
ELF64_ABIS = ("arm64-v8a", "x86_64")
ANDROID_ATTR_RESOURCE_IDS = {
    0x01010003: "name",
    0x0101000E: "enabled",
    0x01010010: "exported",
    0x01010202: "targetActivity",
    0x0101021B: "versionCode",
    0x0101021C: "versionName",
}


def _pb_read_varint(buf: bytes, pos: int):
    result = 0
    shift = 0
    n = len(buf)
    while pos < n:
        b = buf[pos]
        pos += 1
        result |= (b & 0x7F) << shift
        if not (b & 0x80):
            return result, pos
        shift += 7
        if shift > 70:
            raise RuntimeError("Corrupt protobuf varint in AndroidManifest.xml")
    raise RuntimeError("Truncated protobuf varint in AndroidManifest.xml")


def _pb_iter_fields(buf: bytes):
    pos = 0
    n = len(buf)
    while pos < n:
        tag, pos = _pb_read_varint(buf, pos)
        field_no = tag >> 3
        wire_type = tag & 0x07
        if field_no == 0:
            raise RuntimeError("Invalid protobuf field number 0 in AndroidManifest.xml")
        if wire_type == 0:
            val, pos = _pb_read_varint(buf, pos)
            yield field_no, wire_type, val
        elif wire_type == 1:
            if pos + 8 > n:
                raise RuntimeError("Truncated fixed64 field in AndroidManifest.xml")
            yield field_no, wire_type, buf[pos:pos + 8]
            pos += 8
        elif wire_type == 2:
            length, pos = _pb_read_varint(buf, pos)
            if pos + length > n:
                raise RuntimeError("Truncated length-delimited field in AndroidManifest.xml")
            yield field_no, wire_type, buf[pos:pos + length]
            pos += length
        elif wire_type == 5:
            if pos + 4 > n:
                raise RuntimeError("Truncated fixed32 field in AndroidManifest.xml")
            yield field_no, wire_type, buf[pos:pos + 4]
            pos += 4
        else:
            raise RuntimeError(f"Unsupported protobuf wire_type {wire_type} in AndroidManifest.xml")


def _parse_aapt2_xml_attribute(attr_bytes: bytes):
    attr_name = ""
    str_val = None
    res_id = 0
    prim_varint = None
    for fno, wt, val in _pb_iter_fields(attr_bytes):
        if fno == 2 and wt == 2:
            attr_name = val.decode("utf-8", errors="replace")
        elif fno == 3 and wt == 2:
            str_val = val.decode("utf-8", errors="replace")
        elif fno == 5 and wt == 0:
            res_id = val & 0xFFFFFFFF
        elif fno == 6 and wt == 2:
            # aapt.pb.Item -> field 7 is Primitive (fields 6=int_dec, 7=int_hex, 8=bool)
            for ifno, iwt, ival in _pb_iter_fields(val):
                if ifno == 7 and iwt == 2:
                    for pfno, pwt, pval in _pb_iter_fields(ival):
                        if pwt == 0 and pfno in (6, 7, 8):
                            prim_varint = pval
    if not attr_name and res_id in ANDROID_ATTR_RESOURCE_IDS:
        attr_name = ANDROID_ATTR_RESOURCE_IDS[res_id]
    return attr_name, str_val, prim_varint


def _parse_aapt2_xml_node(node_bytes: bytes):
    elem_bytes = None
    for fno, wt, val in _pb_iter_fields(node_bytes):
        if fno == 1 and wt == 2:
            elem_bytes = val
            break
    if elem_bytes is None:
        return None
    tag_name = ""
    attrs = {}
    children = []
    for fno, wt, val in _pb_iter_fields(elem_bytes):
        if fno == 3 and wt == 2:
            tag_name = val.decode("utf-8", errors="replace")
        elif fno == 4 and wt == 2:
            aname, sval, pval = _parse_aapt2_xml_attribute(val)
            if aname:
                attrs[aname] = (sval, pval)
        elif fno == 5 and wt == 2:
            child = _parse_aapt2_xml_node(val)
            if child is not None:
                children.append(child)
    return {"tag": tag_name, "attrs": attrs, "children": children}


def _attr_as_str(attrs: dict, key: str) -> str:
    if key not in attrs:
        return ""
    sval, pval = attrs[key]
    if sval is not None and sval != "":
        return sval
    if pval is not None:
        return str(pval & 0xFFFFFFFF)
    return ""


def _attr_as_int(attrs: dict, key: str):
    if key not in attrs:
        return None
    sval, pval = attrs[key]
    if pval is not None:
        u32 = pval & 0xFFFFFFFF
        return u32 - 0x100000000 if (u32 & 0x80000000) else u32
    if sval is not None and sval != "":
        try:
            return int(sval, 0)
        except ValueError:
            return None
    return None


def _attr_as_bool(attrs: dict, key: str, default: bool = False) -> bool:
    if key not in attrs:
        return default
    sval, pval = attrs[key]
    if pval is not None:
        return (pval & 0xFFFFFFFF) != 0
    if sval is not None:
        low = sval.strip().lower()
        if low in ("true", "1", "-1"):
            return True
        if low in ("false", "0"):
            return False
    return default


def parse_gradle_version(gradle_kts_path: str = DEFAULT_GRADLE_KTS_PATH):
    if not os.path.isfile(gradle_kts_path):
        raise RuntimeError(f"build.gradle.kts not found: {gradle_kts_path}")
    with open(gradle_kts_path, "r", encoding="utf-8") as f:
        text = f.read()
    m_pkg = re.search(r'applicationId\s*=\s*"([^"]+)"', text)
    m_vc = re.search(r'versionCode\s*=\s*(\d+)', text)
    m_vn = re.search(r'versionName\s*=\s*"([^"]+)"', text)
    if not (m_pkg and m_vc and m_vn):
        raise RuntimeError(f"Could not extract applicationId/versionCode/versionName from {gradle_kts_path}")
    return m_pkg.group(1), int(m_vc.group(1)), m_vn.group(1)


def _resolve_component_name(pkg: str, raw_name: str) -> str:
    if not raw_name:
        return ""
    if raw_name.startswith("."):
        return pkg + raw_name
    if "." not in raw_name:
        return f"{pkg}.{raw_name}"
    return raw_name


def _verify_elf_alignment(elf_bytes: bytes, abi: str, so_label: str = "libmiu.so") -> int:
    if len(elf_bytes) < 64 or elf_bytes[:4] != b"\x7fELF":
        raise RuntimeError(f"Invalid ELF binary for ABI '{abi}' ({so_label})")
    ei_class = elf_bytes[4]
    ei_data = elf_bytes[5]
    endian = "<" if ei_data == 1 else (">" if ei_data == 2 else "")
    if not endian:
        raise RuntimeError(f"Invalid ELF endianness ({ei_data}) for ABI '{abi}' ({so_label})")

    expect_64 = abi in ELF64_ABIS
    if expect_64 and ei_class != 2:
        raise RuntimeError(f"Expected ELF64 for ABI '{abi}' ({so_label}), got ei_class={ei_class}")
    if not expect_64 and ei_class != 1:
        raise RuntimeError(f"Expected ELF32 for ABI '{abi}' ({so_label}), got ei_class={ei_class}")

    pt_load_aligns = []
    if ei_class == 2:
        e_phoff = struct.unpack_from(endian + "Q", elf_bytes, 32)[0]
        e_phentsize, e_phnum = struct.unpack_from(endian + "HH", elf_bytes, 54)
        if e_phentsize < 56:
            raise RuntimeError(f"Invalid ELF64 e_phentsize={e_phentsize} (< 56) in '{so_label}'")
        for i in range(e_phnum):
            off = e_phoff + i * e_phentsize
            if off + e_phentsize > len(elf_bytes):
                raise RuntimeError(f"Truncated ELF64 program header in '{so_label}'")
            p_type, _p_flags, p_offset, p_vaddr, _p_paddr, _p_filesz, _p_memsz, p_align = struct.unpack_from(
                endian + "IIQQQQQQ", elf_bytes, off
            )
            if p_type == 1:  # PT_LOAD
                pt_load_aligns.append(p_align)
                if p_align < 0x4000 or (p_align & (p_align - 1)) != 0:
                    raise RuntimeError(
                        f"ABI '{abi}' {so_label} PT_LOAD segment has invalid p_align=0x{p_align:x} "
                        "(must be power-of-two and >= 0x4000 / 16KB required by Google Play)"
                    )
                if (p_vaddr % p_align) != (p_offset % p_align):
                    raise RuntimeError(
                        f"ABI '{abi}' {so_label} PT_LOAD p_vaddr (0x{p_vaddr:x}) and p_offset (0x{p_offset:x}) "
                        f"are not congruent modulo p_align (0x{p_align:x})"
                    )
    else:
        e_phoff = struct.unpack_from(endian + "I", elf_bytes, 28)[0]
        e_phentsize, e_phnum = struct.unpack_from(endian + "HH", elf_bytes, 42)
        if e_phentsize < 32:
            raise RuntimeError(f"Invalid ELF32 e_phentsize={e_phentsize} (< 32) in '{so_label}'")
        for i in range(e_phnum):
            off = e_phoff + i * e_phentsize
            if off + e_phentsize > len(elf_bytes):
                raise RuntimeError(f"Truncated ELF32 program header in '{so_label}'")
            p_type, p_offset, p_vaddr, _p_paddr, _p_filesz, _p_memsz, _p_flags, p_align = struct.unpack_from(
                endian + "IIIIIIII", elf_bytes, off
            )
            if p_type == 1:  # PT_LOAD
                pt_load_aligns.append(p_align)
                if p_align < 0x1000 or (p_align & (p_align - 1)) != 0:
                    raise RuntimeError(
                        f"ABI '{abi}' {so_label} PT_LOAD segment has invalid p_align=0x{p_align:x} "
                        "(must be power-of-two and >= 0x1000)"
                    )
                if (p_vaddr % p_align) != (p_offset % p_align):
                    raise RuntimeError(
                        f"ABI '{abi}' {so_label} PT_LOAD p_vaddr (0x{p_vaddr:x}) and p_offset (0x{p_offset:x}) "
                        f"are not congruent modulo p_align (0x{p_align:x})"
                    )

    if not pt_load_aligns:
        raise RuntimeError(f"ABI '{abi}' {so_label} contains zero PT_LOAD segments")
    return min(pt_load_aligns)


def verify_aab_preflight(aab_path: str, gradle_kts_path: str = DEFAULT_GRADLE_KTS_PATH) -> dict:
    """
    Validate an Android App Bundle (.aab) before uploading to Google Play:
      1. base/manifest/AndroidManifest.xml matches applicationId, versionCode, and versionName in build.gradle.kts.
      2. No <category> mistakenly uses 'android.intent.action.LAUNCHER' (prevents v1.0.51 launch bug).
      3. At least one enabled, exported <activity> / <activity-alias> declares an <intent-filter>
         with BOTH <action android:name="android.intent.action.MAIN"/> AND
         <category android:name="android.intent.category.LAUNCHER"/>.
      4. All 4 ABIs (arm64-v8a, armeabi-v7a, x86, x86_64) include base/lib/<abi>/libmiu.so, and
         64-bit ABIs satisfy 16KB ELF PT_LOAD alignment (p_align >= 0x4000).
    Raises RuntimeError on any failure.
    """
    if not os.path.isfile(aab_path):
        raise RuntimeError(f"AAB file not found: {aab_path}")

    expected_pkg, expected_vc, expected_vn = parse_gradle_version(gradle_kts_path)
    if expected_pkg != PACKAGE_NAME:
        raise RuntimeError(f"build.gradle.kts applicationId '{expected_pkg}' != expected '{PACKAGE_NAME}'")

    try:
        zf = zipfile.ZipFile(aab_path, "r")
    except Exception as e:
        raise RuntimeError(f"Invalid AAB zip archive '{aab_path}': {e}") from e

    with zf:
        names = set(zf.namelist())
        manifest_entry = "base/manifest/AndroidManifest.xml"
        if manifest_entry not in names:
            raise RuntimeError(f"Missing '{manifest_entry}' inside AAB '{aab_path}'")

        manifest_root = _parse_aapt2_xml_node(zf.read(manifest_entry))
        if not manifest_root or manifest_root["tag"] != "manifest":
            raise RuntimeError("Root element of base/manifest/AndroidManifest.xml is not <manifest>")

        pkg = _attr_as_str(manifest_root["attrs"], "package")
        vc = _attr_as_int(manifest_root["attrs"], "versionCode")
        vn = _attr_as_str(manifest_root["attrs"], "versionName")

        if pkg != expected_pkg:
            raise RuntimeError(f"AAB manifest package '{pkg}' != '{expected_pkg}'")
        if vc != expected_vc:
            raise RuntimeError(
                f"AAB manifest versionCode ({vc}) != build.gradle.kts versionCode ({expected_vc}). "
                "Rebuild bundleRelease before uploading!"
            )
        if vn != expected_vn:
            raise RuntimeError(
                f"AAB manifest versionName ('{vn}') != build.gradle.kts versionName ('{expected_vn}'). "
                "Rebuild bundleRelease before uploading!"
            )

        # Inspect <application> and all <category> elements recursively
        def _scan_bad_categories(node):
            if node["tag"] == "category":
                cname = _attr_as_str(node["attrs"], "name")
                if cname == "android.intent.action.LAUNCHER":
                    raise RuntimeError(
                        "CRITICAL MANIFEST BUG DETECTED: <category android:name=\"android.intent.action.LAUNCHER\"/> "
                        "(must be android.intent.category.LAUNCHER)!"
                    )
            for ch in node["children"]:
                _scan_bad_categories(ch)

        _scan_bad_categories(manifest_root)

        launcher_activities = []
        for app_node in manifest_root["children"]:
            if app_node["tag"] != "application":
                continue
            if not _attr_as_bool(app_node["attrs"], "enabled", default=True):
                raise RuntimeError("<application> has android:enabled=\"false\"")
            activities_by_fqcn = {}
            for comp in app_node["children"]:
                if comp["tag"] == "activity":
                    raw_act_name = _attr_as_str(comp["attrs"], "name")
                    act_fqcn = _resolve_component_name(pkg, raw_act_name)
                    if act_fqcn:
                        activities_by_fqcn[act_fqcn] = _attr_as_bool(comp["attrs"], "enabled", default=True)

            for comp in app_node["children"]:
                if comp["tag"] not in ("activity", "activity-alias"):
                    continue
                comp_name = _attr_as_str(comp["attrs"], "name") or "<unnamed>"
                enabled = _attr_as_bool(comp["attrs"], "enabled", default=True)
                exported = _attr_as_bool(comp["attrs"], "exported", default=False)
                for child in comp["children"]:
                    if child["tag"] != "intent-filter":
                        continue
                    actions = {
                        _attr_as_str(sub["attrs"], "name")
                        for sub in child["children"]
                        if sub["tag"] == "action"
                    }
                    categories = {
                        _attr_as_str(sub["attrs"], "name")
                        for sub in child["children"]
                        if sub["tag"] == "category"
                    }
                    if "android.intent.action.MAIN" in actions and "android.intent.category.LAUNCHER" in categories:
                        if not enabled:
                            raise RuntimeError(f"Launcher activity '{comp_name}' has android:enabled=false")
                        if not exported:
                            raise RuntimeError(f"Launcher activity '{comp_name}' must have android:exported=true")
                        if comp["tag"] == "activity-alias":
                            raw_target = _attr_as_str(comp["attrs"], "targetActivity")
                            target_fqcn = _resolve_component_name(pkg, raw_target)
                            if not target_fqcn or target_fqcn not in activities_by_fqcn:
                                raise RuntimeError(
                                    f"Launcher <activity-alias> '{comp_name}' points to missing targetActivity '{raw_target}'"
                                )
                            if not activities_by_fqcn[target_fqcn]:
                                raise RuntimeError(
                                    f"Launcher <activity-alias> '{comp_name}' points to disabled targetActivity '{raw_target}'"
                                )
                        launcher_activities.append(comp_name)

        if not launcher_activities:
            raise RuntimeError(
                "No exported & enabled <activity> in AndroidManifest.xml declares an <intent-filter> with both "
                "android.intent.action.MAIN and android.intent.category.LAUNCHER!"
            )

        abi_alignments = {}
        for abi in REQUIRED_ABIS:
            abi_prefix = f"base/lib/{abi}/"
            required_so = f"{abi_prefix}libmiu.so"
            if required_so not in names:
                raise RuntimeError(f"Missing native library '{required_so}' in AAB bundle")
            so_entries = sorted(n for n in names if n.startswith(abi_prefix) and n.endswith(".so"))
            abi_min = None
            for so_entry in so_entries:
                align = _verify_elf_alignment(zf.read(so_entry), abi, so_entry)
                if abi_min is None or align < abi_min:
                    abi_min = align
            abi_alignments[abi] = f"0x{abi_min:x}"

        for entry_name in sorted(names):
            parts = entry_name.split("/")
            if len(parts) >= 4 and parts[1] == "lib" and entry_name.endswith(".so"):
                entry_abi = parts[2]
                if entry_abi in REQUIRED_ABIS and not entry_name.startswith(f"base/lib/{entry_abi}/"):
                    _verify_elf_alignment(zf.read(entry_name), entry_abi, entry_name)

    return {
        "package": pkg,
        "versionCode": vc,
        "versionName": vn,
        "launcherActivities": launcher_activities,
        "abiMinPtLoadAlign": abi_alignments,
    }


def load_localized_release_notes(expected_version_name: str) -> list:
    from playstore_metadata_screenshots_sync import ANDROID_RELEASE_NOTES

    if not ANDROID_RELEASE_NOTES:
        raise RuntimeError("ANDROID_RELEASE_NOTES in playstore_metadata_screenshots_sync.py is empty")

    for check_loc in ("ja-JP", "en-US"):
        txt = ANDROID_RELEASE_NOTES.get(check_loc, "")
        if expected_version_name not in txt:
            raise RuntimeError(
                f"ANDROID_RELEASE_NOTES['{check_loc}'] does not mention current versionName '{expected_version_name}'. "
                "Update ANDROID_RELEASE_NOTES before publishing!"
            )

    notes = [
        {"language": lang, "text": text}
        for lang, text in sorted(ANDROID_RELEASE_NOTES.items())
    ]
    if not notes:
        raise RuntimeError("Resolved localized releaseNotes list is empty")
    return notes


def get_access_token(service_account_json_path: str) -> str:
    import jwt
    import requests

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


def upload_to_play_store(
    aab_path: str,
    key_path: str,
    track: str = "internal",
    status: str = "completed",
    gradle_kts_path: str = DEFAULT_GRADLE_KTS_PATH,
):
    import requests

    preflight = verify_aab_preflight(aab_path, gradle_kts_path)
    version_name = preflight["versionName"]
    expected_vc = preflight["versionCode"]
    print(
        f"[Preflight OK] {preflight['package']} v{version_name} (versionCode={expected_vc}), "
        f"launcher={preflight['launcherActivities']}, ELF PT_LOAD align={preflight['abiMinPtLoadAlign']}"
    )

    release_notes = load_localized_release_notes(version_name)

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

    # 2. Upload AAB bundle (fail immediately if versionCode was already used — never silently republish stale bundles)
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
    if r_upload.status_code in (400, 403, 409) and "already been used" in r_upload.text:
        raise RuntimeError(
            f"AAB versionCode {expected_vc} has already been uploaded to Google Play ({r_upload.status_code}). "
            "Increment versionCode in Android/app/build.gradle.kts and rebuild bundleRelease instead of reusing an old bundle."
        )
    r_upload.raise_for_status()
    bundle_info = r_upload.json()
    version_code = int(bundle_info["versionCode"])
    if version_code != expected_vc:
        raise RuntimeError(f"Uploaded bundle versionCode ({version_code}) != preflight versionCode ({expected_vc})")
    print(f"Uploaded bundle versionCode: {version_code} (sha256: {bundle_info.get('sha256')})")

    # 3. Filter releaseNotes to active Store Listings and assign bundle to track
    r_listings = requests.get(f"{base_url}/edits/{edit_id}/listings", headers=headers, timeout=30)
    if r_listings.status_code == 200:
        active_langs = {
            item.get("language")
            for item in r_listings.json().get("listings", [])
            if item.get("language")
        }
        if active_langs:
            filtered_notes = [n for n in release_notes if n["language"] in active_langs]
            if filtered_notes:
                if len(filtered_notes) != len(release_notes):
                    skipped = sorted({n["language"] for n in release_notes} - active_langs)
                    print(
                        f"Filtered releaseNotes from {len(release_notes)} to {len(filtered_notes)} "
                        f"active Play Store listing locale(s) (skipped: {', '.join(skipped)})"
                    )
                release_notes = filtered_notes

    tracks_to_update = [track]
    if track == "production" and "internal" not in tracks_to_update:
        tracks_to_update.append("internal")

    print(f"Using {len(release_notes)} localized v{version_name} releaseNotes for '{track}' track.")

    for target_track in tracks_to_update:
        rel_name = f"{version_code} ({version_name})" if target_track == "production" else version_name
        print(f"Assigning versionCode {version_code} ({rel_name}) to track '{target_track}' (status={status})...")
        track_body = {
            "track": target_track,
            "releases": [
                {
                    "name": rel_name,
                    "versionCodes": [str(version_code)],
                    "status": status,
                    "releaseNotes": release_notes,
                }
            ],
        }
        r_track = requests.put(
            f"{base_url}/edits/{edit_id}/tracks/{target_track}",
            headers={**headers, "Content-Type": "application/json"},
            json=track_body,
            timeout=60,
        )
        r_track.raise_for_status()

    # Sync 'alpha' and 'beta' tracks if they hold superseded lower versionCodes
    if track == "production":
        for stale_track in ("beta", "alpha"):
            r_tr_get = requests.get(f"{base_url}/edits/{edit_id}/tracks/{stale_track}", headers=headers, timeout=30)
            if r_tr_get.status_code == 200:
                existing_rels = r_tr_get.json().get("releases", [])
                stale_codes = [
                    vc
                    for rel in existing_rels
                    for vc in rel.get("versionCodes", [])
                    if int(vc) < int(version_code)
                ]
                if stale_codes:
                    print(f"Updating superseded release(s) {stale_codes} on '{stale_track}' track to {version_code} ({version_name})...")
                    r_clr = requests.put(
                        f"{base_url}/edits/{edit_id}/tracks/{stale_track}",
                        headers={**headers, "Content-Type": "application/json"},
                        json={
                            "track": stale_track,
                            "releases": [
                                {
                                    "name": f"{version_code} ({version_name})",
                                    "versionCodes": [str(version_code)],
                                    "status": status,
                                    "releaseNotes": release_notes,
                                }
                            ],
                        },
                        timeout=60,
                    )
                    r_clr.raise_for_status()

    # 4. Commit edit (automatically fall back to changesNotSentForReview=true if Play Console has an in-review/restricted state)
    print(f"Committing edit {edit_id}...")
    r_commit = requests.post(f"{base_url}/edits/{edit_id}:commit", headers=headers, timeout=60)
    if r_commit.status_code == 400 and "changesNotSentForReview" in r_commit.text:
        print("Play Console requested changesNotSentForReview=true; retrying commit without altering active review...")
        r_commit = requests.post(
            f"{base_url}/edits/{edit_id}:commit",
            params={"changesNotSentForReview": "true"},
            headers=headers,
            timeout=60,
        )
    r_commit.raise_for_status()
    print(f"Successfully published versionCode {version_code} to Google Play '{track}' track!")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Validate & upload Android App Bundle (.aab) to Google Play Console")
    parser.add_argument("--aab", default=DEFAULT_AAB_PATH, help="Path to app-release.aab")
    parser.add_argument("--gradle-kts", default=DEFAULT_GRADLE_KTS_PATH, help="Path to Android/app/build.gradle.kts")
    parser.add_argument("--key", default=DEFAULT_KEY_PATH, help="Path to Google Play Service Account JSON key")
    parser.add_argument("--track", default="internal", choices=["internal", "alpha", "beta", "production"], help="Target track")
    parser.add_argument("--draft", action="store_true", help="Upload as draft release instead of completed")
    parser.add_argument("--verify-only", action="store_true", help="Run AAB preflight & release-notes checks only (no network upload)")
    args = parser.parse_args()

    if args.verify_only:
        info = verify_aab_preflight(args.aab, args.gradle_kts)
        notes = load_localized_release_notes(info["versionName"])
        print(
            f"[VERIFY-ONLY PASS] package={info['package']} versionCode={info['versionCode']} "
            f"versionName={info['versionName']} launcher={info['launcherActivities']} "
            f"abiAlign={info['abiMinPtLoadAlign']} localizedReleaseNotes={len(notes)}"
        )
        sys.exit(0)

    upload_to_play_store(
        aab_path=args.aab,
        key_path=args.key,
        track=args.track,
        status="draft" if args.draft else "completed",
        gradle_kts_path=args.gradle_kts,
    )

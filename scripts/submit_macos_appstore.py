#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Create/update the macOS App Store Connect version (default: 1.0.54),
synchronize 34-locale release notes & metadata, attach the verified
TestFlight build (Build 58), and submit MAC_OS ONLY for App Store Review.
"""

import argparse
import os
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

import requests  # noqa: E402
from appstore_metadata_sync import (  # noqa: E402
    ASC_KEYWORDS,
    ASC_LOCALIZATIONS,
    ASC_PROMOTIONAL_TEXT,
    BASE_URL,
    SUPPORT_URL,
    get_app_id,
    get_headers,
)

# Concise, polite 34-locale release notes for macOS miu v1.0.54
MACOS_WHATS_NEW_V1_0_54 = {
    "ja": (
        "miu v1.0.54 アップデート内容\n"
        "・macOS版でライブプレビューが表示されないことがある問題を修正\n"
        "・日本語のライブ変換（未確定文字）入力中も Option+P などのプレビュー切替ショートカットが動作するよう改善"
    ),
    "en-US": (
        "What's New in miu v1.0.54\n"
        "• Fixed an issue on macOS where Live Preview could appear blank under App Sandbox.\n"
        "• Improved Option+P preview shortcut handling during live IME text composition."
    ),
    "en-GB": (
        "What's New in miu v1.0.54\n"
        "• Fixed an issue on macOS where Live Preview could appear blank under App Sandbox.\n"
        "• Improved Option+P preview shortcut handling during live IME text composition."
    ),
    "en-AU": (
        "What's New in miu v1.0.54\n"
        "• Fixed an issue on macOS where Live Preview could appear blank under App Sandbox.\n"
        "• Improved Option+P preview shortcut handling during live IME text composition."
    ),
    "en-CA": (
        "What's New in miu v1.0.54\n"
        "• Fixed an issue on macOS where Live Preview could appear blank under App Sandbox.\n"
        "• Improved Option+P preview shortcut handling during live IME text composition."
    ),
    "zh-Hans": (
        "miu v1.0.54 更新内容\n"
        "・修复了 macOS 版在 App Sandbox 环境下实时预览可能显示为空白的问题\n"
        "・改进了输入法候选词输入期间使用 Option+P 切换预览的快捷键响应"
    ),
    "zh-Hant": (
        "miu v1.0.54 更新內容\n"
        "・修正了 macOS 版在 App Sandbox 環境下即時預覽可能顯示為空白的問題\n"
        "・改善了輸入法組字期間使用 Option+P 切換預覽的快捷鍵體驗"
    ),
    "ko": (
        "miu v1.0.54 업데이트 내용\n"
        "・macOS 버전에서 실시간 미리보기가 빈 화면으로 표시될 수 있던 문제 수정\n"
        "・입력기(IME) 조합 중에도 Option+P 미리보기 전환 단축키가 원활히 작동하도록 개선"
    ),
    "de-DE": (
        "Neu in miu v1.0.54\n"
        "• Ein Problem unter macOS wurde behoben, bei dem die Live-Vorschau in der App-Sandbox leer bleiben konnte.\n"
        "• Das Kurzbefehlverhalten (Option+P) während aktiver IME-Texteingabe wurde verbessert."
    ),
    "fr-FR": (
        "Nouveautés de miu v1.0.54\n"
        "• Correction d’un problème sur macOS où l’aperçu en direct pouvait rester vide dans l’App Sandbox.\n"
        "• Amélioration du raccourci Option+P pendant la saisie avec méthode d’entrée (IME)."
    ),
    "fr-CA": (
        "Nouveautés de miu v1.0.54\n"
        "• Correction d’un problème sur macOS où l’aperçu en direct pouvait rester vide dans l’App Sandbox.\n"
        "• Amélioration du raccourci Option+P pendant la saisie avec méthode d’entrée (IME)."
    ),
    "es-ES": (
        "Novedades de miu v1.0.54\n"
        "• Se corrigió un problema en macOS por el que la vista previa en vivo podía mostrarse en blanco bajo App Sandbox.\n"
        "• Se mejoró la respuesta del atajo Option+P durante la composición de texto con IME."
    ),
    "es-MX": (
        "Novedades de miu v1.0.54\n"
        "• Se corrigió un problema en macOS por el que la vista previa en vivo podía mostrarse en blanco bajo App Sandbox.\n"
        "• Se mejoró la respuesta del atajo Option+P durante la composición de texto con IME."
    ),
    "it": (
        "Novità di miu v1.0.54\n"
        "• Risolto un problema su macOS per cui l’anteprima live poteva apparire vuota in App Sandbox.\n"
        "• Migliorata la gestione della scorciatoia Option+P durante la composizione del testo con IME."
    ),
    "pt-BR": (
        "Novidades do miu v1.0.54\n"
        "• Corrigido um problema no macOS em que a pré-visualização ao vivo podia aparecer em branco no App Sandbox.\n"
        "• Melhorado o funcionamento do atalho Option+P durante a composição de texto via IME."
    ),
    "pt-PT": (
        "Novidades do miu v1.0.54\n"
        "• Corrigido um problema no macOS em que a pré-visualização ao vivo podia surgir em branco no App Sandbox.\n"
        "• Melhorado o funcionamento do atalho Option+P durante a composição de texto com IME."
    ),
    "ru": (
        "Что нового в miu v1.0.54\n"
        "• Исправлена ошибка в версии для macOS, из-за которой предпросмотр мог отображаться пустым в среде App Sandbox.\n"
        "• Улучшена работа сочетания клавиш Option+P во время набора текста через IME."
    ),
    "nl-NL": (
        "Nieuw in miu v1.0.54\n"
        "• Probleem op macOS verholpen waarbij de live-weergave leeg kon blijven onder App Sandbox.\n"
        "• Verbeterde werking van de sneltoets Option+P tijdens actieve IME-tekstinvoer."
    ),
    "sv": (
        "Nyheter i miu v1.0.54\n"
        "• Åtgärdade ett problem på macOS där live-förhandsgranskningen kunde visas tom i App Sandbox.\n"
        "• Förbättrade kortkommandot Option+P under pågående IME-textinmatning."
    ),
    "da": (
        "Nyheder i miu v1.0.54\n"
        "• Rettede en fejl på macOS, hvor live-forhåndsvisning kunne vises som en tom side i App Sandbox.\n"
        "• Forbedrede genvejen Option+P under aktiv IME-tekstindtastning."
    ),
    "no": (
        "Nyheter i miu v1.0.54\n"
        "• Rettet en feil på macOS der live-forhåndsvisning kunne vises som tom i App Sandbox.\n"
        "• Forbedret hurtigtasten Option+P under aktiv IME-tekstinndata."
    ),
    "fi": (
        "Uutta versiossa miu v1.0.54\n"
        "• Korjattiin macOS-ongelma, jossa reaaliaikainen esikatselu saattoi näkyä tyhjänä App Sandbox -tilassa.\n"
        "• Parannettiin Option+P-pikanäppäimen toimintaa IME-tekstinsyötön aikana."
    ),
    "pl": (
        "Co nowego w miu v1.0.54\n"
        "• Naprawiono błąd w systemie macOS, przez który podgląd na żywo mógł być pusty w środowisku App Sandbox.\n"
        "• Usprawniono działanie skrótu Option+P podczas wprowadzania tekstu przez IME."
    ),
    "cs": (
        "Co je nového v miu v1.0.54\n"
        "• Opraven problém v macOS, kdy se živý náhled v prostředí App Sandbox mohl zobrazovat prázdný.\n"
        "• Vylepšena odezva zkratky Option+P během psaní textu přes IME."
    ),
    "hu": (
        "Újdonságok a miu v1.0.54 verzióban\n"
        "• Javítottuk a hibát macOS-en, amely miatt az élő előnézet üres maradhatott App Sandbox alatt.\n"
        "• Fejlesztettük az Option+P gyorsbillentyű működését IME szövegbevitel közben."
    ),
    "ro": (
        "Noutăți în miu v1.0.54\n"
        "• S-a remediat o problemă pe macOS în care previzualizarea live putea apărea goală în App Sandbox.\n"
        "• S-a îmbunătățit funcționarea scurtăturii Option+P în timpul introducerii textului prin IME."
    ),
    "el": (
        "Τι νέο υπάρχει στο miu v1.0.54\n"
        "• Διορθώθηκε ζήτημα στο macOS όπου η ζωντανή προεπισκόπηση μπορούσε να εμφανίζεται κενή στο App Sandbox.\n"
        "• Βελτιώθηκε η απόκριση της συντόμευσης Option+P κατά τη σύνθεση κειμένου με IME."
    ),
    "tr": (
        "miu v1.0.54 Yenilikleri\n"
        "• macOS'te Canlı Önizleme'nin App Sandbox altında boş görünmesine neden olabilen sorun giderildi.\n"
        "• IME ile metin yazımı sırasında Option+P önizleme kısayolunun çalışması iyileştirildi."
    ),
    "uk": (
        "Що нового в miu v1.0.54\n"
        "• Виправлено помилку на macOS, через яку попередній перегляд міг відображатися порожнім у середовищі App Sandbox.\n"
        "• Покращено роботу комбінації клавіш Option+P під час набору тексту через IME."
    ),
    "vi": (
        "Có gì mới trong miu v1.0.54\n"
        "• Khắc phục sự cố trên macOS khiến Xem trước trực tiếp có thể hiển thị trống trong môi trường App Sandbox.\n"
        "• Cải thiện phản hồi phím tắt Option+P khi đang gõ chữ bằng bộ gõ IME."
    ),
    "th": (
        "มีอะไรใหม่ใน miu v1.0.54\n"
        "• แก้ไขปัญหาบน macOS ที่ทำให้หน้าต่างพรีวิวแบบเรียลไทม์อาจแสดงเป็นหน้าว่างภายใต้ App Sandbox\n"
        "• ปรับปรุงการทำงานของปุ่มลัด Option+P ระหว่างพิมพ์ข้อความด้วย IME"
    ),
    "id": (
        "Yang Baru di miu v1.0.54\n"
        "• Memperbaiki masalah di macOS yang menyebabkan Pratinjau Langsung dapat tampil kosong di bawah App Sandbox.\n"
        "• Meningkatkan respons pintasan Option+P saat mengetik dengan komposisi IME."
    ),
    "ms": (
        "Baharu dalam miu v1.0.54\n"
        "• Membaiki isu pada macOS yang menyebabkan Pratonton Langsung kadangkala kosong di bawah App Sandbox.\n"
        "• Menambah baik tindak balas pintasan Option+P semasa menaip dengan komposisi IME."
    ),
    "hi": (
        "miu v1.0.54 में नया क्या है\n"
        "• macOS पर App Sandbox के अंतर्गत लाइव प्रीव्यू के खाली दिखने की समस्या को ठीक किया गया।\n"
        "• IME टेक्स्ट इनपुट के दौरान Option+P प्रीव्यू शॉर्टकट की कार्यप्रणाली में सुधार किया गया।"
    ),
}

assert set(MACOS_WHATS_NEW_V1_0_54.keys()) == set(ASC_LOCALIZATIONS.keys()), (
    f"Locale mismatch: {set(MACOS_WHATS_NEW_V1_0_54.keys()) ^ set(ASC_LOCALIZATIONS.keys())}"
)
for _loc, _wn in MACOS_WHATS_NEW_V1_0_54.items():
    assert len(_wn) <= 4000, f"whatsNew for {_loc} exceeds 4000 chars ({len(_wn)})"


def get_or_create_macos_version(app_id: str, target_version: str, allow_cancel_review: bool):
    headers = get_headers()
    r = requests.get(
        f"{BASE_URL}/apps/{app_id}/appStoreVersions?filter[platform]=MAC_OS&limit=50",
        headers=headers,
        timeout=60,
    )
    if r.status_code != 200:
        raise RuntimeError(f"Failed to query MAC_OS appStoreVersions: HTTP {r.status_code} {r.text}")

    versions = r.json().get("data", [])
    editable_states = {
        "PREPARE_FOR_SUBMISSION",
        "DEVELOPER_REJECTED",
        "REJECTED",
        "METADATA_REJECTED",
        "WAITING_FOR_REVIEW",
        "IN_REVIEW",
        "INVALID_BINARY",
    }
    live_states = {
        "READY_FOR_SALE",
        "PENDING_DEVELOPER_RELEASE",
        "PROCESSING_FOR_APP_STORE",
        "PREORDER_READY_FOR_SALE",
    }

    live_v = None
    editable_v = None
    for v in versions:
        st = v.get("attributes", {}).get("appStoreState", "")
        v_str = v.get("attributes", {}).get("versionString", "")
        print(f"[MAC_OS] Existing version: {v_str} (id={v['id']}, state={st})")
        if st in live_states and live_v is None:
            live_v = v
        if st in editable_states and editable_v is None:
            editable_v = v

    prev_copyright = ""
    prev_release_type = ""
    if live_v:
        prev_attrs = live_v.get("attributes", {})
        prev_copyright = (prev_attrs.get("copyright") or "").strip()
        prev_release_type = (prev_attrs.get("releaseType") or "").strip()
        print(f"[MAC_OS] Live baseline {prev_attrs.get('versionString')}: copyright='{prev_copyright}', releaseType='{prev_release_type}'")

    if editable_v:
        v_id = editable_v["id"]
        attrs = editable_v.get("attributes", {})
        v_str = attrs.get("versionString", "")
        v_state = attrs.get("appStoreState", "")
        print(f"[MAC_OS] Found editable version {v_str} (id={v_id}, state={v_state})")

        if v_state in ("WAITING_FOR_REVIEW", "IN_REVIEW") and v_str == target_version and not allow_cancel_review:
            return v_id, v_str, v_state

        if v_state in ("WAITING_FOR_REVIEW", "IN_REVIEW") and allow_cancel_review:
            print(f"[MAC_OS] Version {v_str} is already in {v_state}; checking active reviewSubmission...")
            return v_id, v_str, v_state

        if v_str != target_version:
            requests.patch(
                f"{BASE_URL}/appStoreVersions/{v_id}/relationships/build",
                headers=headers,
                json={"data": None},
                timeout=60,
            )
            pr = requests.patch(
                f"{BASE_URL}/appStoreVersions/{v_id}",
                headers=headers,
                json={
                    "data": {
                        "type": "appStoreVersions",
                        "id": v_id,
                        "attributes": {"versionString": target_version},
                    }
                },
                timeout=60,
            )
            if pr.status_code != 200:
                raise RuntimeError(f"Failed to update MAC_OS versionString {v_str} -> {target_version}: HTTP {pr.status_code} {pr.text}")
            v_str = target_version
            print(f"[MAC_OS] Updated editable versionString -> {target_version}")
    else:
        print(f"[MAC_OS] Creating new MAC_OS appStoreVersion {target_version}...")
        create_attrs = {
            "platform": "MAC_OS",
            "versionString": target_version,
        }
        if prev_copyright:
            create_attrs["copyright"] = prev_copyright
        if prev_release_type:
            create_attrs["releaseType"] = prev_release_type

        create_payload = {
            "data": {
                "type": "appStoreVersions",
                "attributes": create_attrs,
                "relationships": {
                    "app": {"data": {"type": "apps", "id": app_id}}
                },
            }
        }
        res = requests.post(f"{BASE_URL}/appStoreVersions", headers=headers, json=create_payload, timeout=60)
        if res.status_code not in (200, 201):
            # Fallback to minimal create attributes
            fallback_payload = {
                "data": {
                    "type": "appStoreVersions",
                    "attributes": {"platform": "MAC_OS", "versionString": target_version},
                    "relationships": {"app": {"data": {"type": "apps", "id": app_id}}},
                }
            }
            res = requests.post(f"{BASE_URL}/appStoreVersions", headers=headers, json=fallback_payload, timeout=60)
            if res.status_code not in (200, 201):
                raise RuntimeError(f"Failed to create MAC_OS version {target_version}: HTTP {res.status_code} {res.text}")
        new_v = res.json()["data"]
        v_id = new_v["id"]
        v_str = new_v.get("attributes", {}).get("versionString", target_version)
        v_state = new_v.get("attributes", {}).get("appStoreState", "PREPARE_FOR_SUBMISSION")
        print(f"[MAC_OS] Created MAC_OS version {v_str} (id={v_id}, state={v_state})")
        time.sleep(3)

    # Ensure copyright and appStoreReviewDetail are present if inherited baseline had them
    r_cur = requests.get(f"{BASE_URL}/appStoreVersions/{v_id}", headers=headers, timeout=60)
    if r_cur.status_code == 200:
        cur_attrs = r_cur.json().get("data", {}).get("attributes", {})
        cur_copyright = (cur_attrs.get("copyright") or "").strip()
        if not cur_copyright and (prev_copyright or "© 2026"):
            patch_cr = prev_copyright or "2026 Hack Inc."
            rc = requests.patch(
                f"{BASE_URL}/appStoreVersions/{v_id}",
                headers=headers,
                json={"data": {"type": "appStoreVersions", "id": v_id, "attributes": {"copyright": patch_cr}}},
                timeout=60,
            )
            print(f"[MAC_OS] Ensured copyright='{patch_cr}' on {v_str}: HTTP {rc.status_code}")

    if live_v:
        r_rd = requests.get(f"{BASE_URL}/appStoreVersions/{v_id}/appStoreReviewDetail", headers=headers, timeout=60)
        if r_rd.status_code == 404:
            r_prev_rd = requests.get(f"{BASE_URL}/appStoreVersions/{live_v['id']}/appStoreReviewDetail", headers=headers, timeout=60)
            if r_prev_rd.status_code == 200:
                prev_rd_attrs = r_prev_rd.json().get("data", {}).get("attributes", {})
                rd_attrs = {
                    k: prev_rd_attrs[k]
                    for k in (
                        "contactFirstName",
                        "contactLastName",
                        "contactPhone",
                        "contactEmail",
                        "demoAccountName",
                        "demoAccountPassword",
                        "demoAccountRequired",
                        "notes",
                    )
                    if prev_rd_attrs.get(k) is not None
                }
                if rd_attrs:
                    r_crd = requests.post(
                        f"{BASE_URL}/appStoreReviewDetails",
                        headers=headers,
                        json={
                            "data": {
                                "type": "appStoreReviewDetails",
                                "attributes": rd_attrs,
                                "relationships": {
                                    "appStoreVersion": {"data": {"type": "appStoreVersions", "id": v_id}}
                                },
                            }
                        },
                        timeout=60,
                    )
                    print(f"[MAC_OS] Cloned appStoreReviewDetail from {live_v['id']}: HTTP {r_crd.status_code}")
        elif r_rd.status_code == 200:
            print(f"[MAC_OS] Verified appStoreReviewDetail is present on {v_str}.")

    return v_id, v_str, v_state


def sync_macos_localizations(version_id: str, version_str: str):
    headers = get_headers()
    print(f"\n=== Syncing 34 Locales for MAC_OS v{version_str} (version_id={version_id}) ===")
    r = requests.get(
        f"{BASE_URL}/appStoreVersions/{version_id}/appStoreVersionLocalizations?limit=200",
        headers=headers,
        timeout=60,
    )
    if r.status_code != 200:
        raise RuntimeError(f"Failed to fetch MAC_OS localizations: HTTP {r.status_code} {r.text}")

    existing_locs = {
        loc.get("attributes", {}).get("locale"): loc["id"]
        for loc in r.json().get("data", [])
    }
    print(f"[MAC_OS] Existing locales ({len(existing_locs)}): {', '.join(sorted(existing_locs.keys()))}")

    updated = 0
    created = 0
    failed = 0

    for locale, (desc, asc_whats_new) in ASC_LOCALIZATIONS.items():
        if version_str == "1.0.54":
            whats_new = MACOS_WHATS_NEW_V1_0_54[locale]
        else:
            whats_new = asc_whats_new
        kw = ASC_KEYWORDS[locale]
        promo = ASC_PROMOTIONAL_TEXT[locale]

        if locale in existing_locs:
            loc_id = existing_locs[locale]
            payload = {
                "data": {
                    "type": "appStoreVersionLocalizations",
                    "id": loc_id,
                    "attributes": {
                        "description": desc,
                        "whatsNew": whats_new,
                        "promotionalText": promo,
                        "keywords": kw,
                        "supportUrl": SUPPORT_URL,
                    },
                }
            }
            res = requests.patch(
                f"{BASE_URL}/appStoreVersionLocalizations/{loc_id}",
                headers=headers,
                json=payload,
                timeout=60,
            )
            if res.status_code == 200:
                updated += 1
                print(f"  [OK] Updated {locale}")
            else:
                # Fallback to whatsNew + promotionalText + supportUrl if version is locked to post-submission fields
                payload_wn = {
                    "data": {
                        "type": "appStoreVersionLocalizations",
                        "id": loc_id,
                        "attributes": {
                            "whatsNew": whats_new,
                            "promotionalText": promo,
                            "supportUrl": SUPPORT_URL,
                        },
                    }
                }
                res2 = requests.patch(
                    f"{BASE_URL}/appStoreVersionLocalizations/{loc_id}",
                    headers=headers,
                    json=payload_wn,
                    timeout=60,
                )
                if res2.status_code == 200:
                    updated += 1
                    print(f"  [OK] Updated {locale} (whatsNew + promotionalText + supportUrl)")
                else:
                    failed += 1
                    print(f"  [ERR] Failed {locale}: HTTP {res.status_code} {res.text[:240]}")
        else:
            payload = {
                "data": {
                    "type": "appStoreVersionLocalizations",
                    "attributes": {
                        "locale": locale,
                        "description": desc,
                        "whatsNew": whats_new,
                        "promotionalText": promo,
                        "keywords": kw,
                        "supportUrl": SUPPORT_URL,
                    },
                    "relationships": {
                        "appStoreVersion": {
                            "data": {"type": "appStoreVersions", "id": version_id}
                        }
                    },
                }
            }
            res = requests.post(
                f"{BASE_URL}/appStoreVersionLocalizations",
                headers=headers,
                json=payload,
                timeout=60,
            )
            if res.status_code in (200, 201):
                created += 1
                print(f"  [NEW] Created {locale}")
            else:
                failed += 1
                print(f"  [ERR] Failed to create {locale}: HTTP {res.status_code} {res.text[:240]}")

    print(f"[MAC_OS] Localization Summary: {updated} updated, {created} created, {failed} failed.")
    if failed > 0:
        raise RuntimeError(f"{failed} locale(s) failed to synchronize on MAC_OS v{version_str}.")


def attach_macos_build_and_submit(app_id: str, v_id: str, v_str: str, expected_build: str = ""):
    headers = get_headers()
    print(f"\n=== Attaching VALID MAC_OS Build & Submitting v{v_str} for App Store Review ===")
    r_b = requests.get(
        f"{BASE_URL}/builds?filter[app]={app_id}&filter[preReleaseVersion.platform]=MAC_OS"
        f"&filter[preReleaseVersion.version]={v_str}&sort=-uploadedDate&limit=10",
        headers=headers,
        timeout=60,
    )
    if r_b.status_code != 200:
        raise RuntimeError(f"Failed to query MAC_OS builds: HTTP {r_b.status_code} {r_b.text}")

    builds = r_b.json().get("data", [])
    chosen_build_id = None
    chosen_build_ver = None
    for b in builds:
        b_id = b["id"]
        b_ver = b.get("attributes", {}).get("version", "")
        b_state = b.get("attributes", {}).get("processingState", "")
        print(f"[MAC_OS] Candidate build: version={b_ver} (id={b_id}, state={b_state})")
        if expected_build and b_ver != expected_build:
            continue
        if b_state == "VALID":
            chosen_build_id = b_id
            chosen_build_ver = b_ver
            break

    if not chosen_build_id:
        raise RuntimeError(f"No VALID MAC_OS build found for v{v_str} (expected_build='{expected_build}').")

    # Optional usesNonExemptEncryption patch (409 is benign when Info.plist already sets ITSAppUsesNonExemptEncryption=NO)
    requests.patch(
        f"{BASE_URL}/builds/{chosen_build_id}",
        headers=headers,
        json={
            "data": {
                "type": "builds",
                "id": chosen_build_id,
                "attributes": {"usesNonExemptEncryption": False},
            }
        },
        timeout=60,
    )

    r_rel = requests.patch(
        f"{BASE_URL}/appStoreVersions/{v_id}/relationships/build",
        headers=headers,
        json={"data": {"type": "builds", "id": chosen_build_id}},
        timeout=60,
    )
    if r_rel.status_code not in (200, 204):
        raise RuntimeError(f"Failed to attach build {chosen_build_ver} ({chosen_build_id}): HTTP {r_rel.status_code} {r_rel.text}")
    print(f"[MAC_OS] Attached VALID build {chosen_build_ver} ({chosen_build_id}) to MAC_OS v{v_str} ({v_id}).")

    # Verify attached build relationship
    r_vb = requests.get(f"{BASE_URL}/appStoreVersions/{v_id}/build", headers=headers, timeout=60)
    if r_vb.status_code == 200 and r_vb.json().get("data"):
        vb = r_vb.json()["data"]
        print(f"[MAC_OS] Verified attached build on version {v_str}: build={vb.get('attributes', {}).get('version')} (id={vb['id']})")

    # Check existing MAC_OS reviewSubmissions
    r_subs = requests.get(
        f"{BASE_URL}/apps/{app_id}/reviewSubmissions?filter[platform]=MAC_OS&limit=20",
        headers=headers,
        timeout=60,
    )
    sub_id = None
    if r_subs.status_code == 200:
        for s in r_subs.json().get("data", []):
            st = s.get("attributes", {}).get("state", "")
            print(f"[MAC_OS] Existing reviewSubmission {s['id']}: state={st}")
            if st in ("READY_FOR_REVIEW", "UNRESOLVED_ISSUES"):
                sub_id = s["id"]
                break
            if st in ("WAITING_FOR_REVIEW", "IN_REVIEW"):
                print(f"[MAC_OS] Already in {st} (reviewSubmission {s['id']})!")
                return

    if not sub_id:
        r_cs = requests.post(
            f"{BASE_URL}/reviewSubmissions",
            headers=headers,
            json={
                "data": {
                    "type": "reviewSubmissions",
                    "attributes": {"platform": "MAC_OS"},
                    "relationships": {"app": {"data": {"type": "apps", "id": app_id}}},
                }
            },
            timeout=60,
        )
        if r_cs.status_code not in (200, 201):
            raise RuntimeError(f"Failed to create MAC_OS reviewSubmission: HTTP {r_cs.status_code} {r_cs.text}")
        sub_id = r_cs.json()["data"]["id"]
        print(f"[MAC_OS] Created reviewSubmission {sub_id}")

    r_item = requests.post(
        f"{BASE_URL}/reviewSubmissionItems",
        headers=headers,
        json={
            "data": {
                "type": "reviewSubmissionItems",
                "relationships": {
                    "reviewSubmission": {"data": {"type": "reviewSubmissions", "id": sub_id}},
                    "appStoreVersion": {"data": {"type": "appStoreVersions", "id": v_id}},
                },
            }
        },
        timeout=60,
    )
    if r_item.status_code not in (200, 201, 409):
        raise RuntimeError(f"Failed to add MAC_OS appStoreVersion {v_id} to reviewSubmission {sub_id}: HTTP {r_item.status_code} {r_item.text}")
    print(f"[MAC_OS] Added appStoreVersion {v_id} to reviewSubmission {sub_id} (HTTP {r_item.status_code})")

    r_submit = requests.patch(
        f"{BASE_URL}/reviewSubmissions/{sub_id}",
        headers=headers,
        json={
            "data": {
                "type": "reviewSubmissions",
                "id": sub_id,
                "attributes": {"submitted": True},
            }
        },
        timeout=60,
    )
    if r_submit.status_code != 200:
        raise RuntimeError(
            f"Failed to submit MAC_OS reviewSubmission {sub_id}: HTTP {r_submit.status_code} {r_submit.text}"
        )

    sub_state = r_submit.json().get("data", {}).get("attributes", {}).get("state", "")
    print(f"[MAC_OS] reviewSubmission {sub_id} submitted! Submission state: {sub_state}")

    time.sleep(3)
    r_ver_final = requests.get(f"{BASE_URL}/appStoreVersions/{v_id}", headers=headers, timeout=60)
    ver_state = ""
    if r_ver_final.status_code == 200:
        ver_state = r_ver_final.json().get("data", {}).get("attributes", {}).get("appStoreState", "")
        print(f"[MAC_OS] Final MAC_OS v{v_str} appStoreState: {ver_state}")

    if sub_state not in ("WAITING_FOR_REVIEW", "IN_REVIEW") and ver_state not in ("WAITING_FOR_REVIEW", "IN_REVIEW"):
        raise RuntimeError(
            f"Unexpected post-submit state for MAC_OS v{v_str}: submission={sub_state}, version={ver_state}"
        )


def main():
    parser = argparse.ArgumentParser(description="Submit macOS miu release on App Store Connect.")
    parser.add_argument("--version", default="1.0.55", help="Target MAC_OS marketing version (default: 1.0.55)")
    parser.add_argument("--build", default="", help="Optional exact CFBundleVersion build number (e.g. 58)")
    parser.add_argument("--submit", action="store_true", help="Attach build and submit MAC_OS for App Store Review")
    args = parser.parse_args()

    print(f"=== App Store Connect MAC_OS Release Submitter for miu v{args.version} ===")
    headers = get_headers()
    app_id, _app_name = get_app_id(headers)

    v_id, v_str, v_state = get_or_create_macos_version(app_id, args.version, allow_cancel_review=args.submit)
    if v_state not in ("WAITING_FOR_REVIEW", "IN_REVIEW"):
        sync_macos_localizations(v_id, v_str)
    else:
        print(f"[MAC_OS] Version {v_str} is already in {v_state}; skipping localization overwrite.")

    if args.submit:
        attach_macos_build_and_submit(app_id, v_id, v_str, expected_build=args.build)
    else:
        print("[Info] Metadata synced only (--submit not passed).")

    print(f"\n=== Completed MAC_OS v{v_str} App Store Review workflow successfully ===")


if __name__ == "__main__":
    main()

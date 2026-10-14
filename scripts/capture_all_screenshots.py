# -*- coding: utf-8 -*-
import os
import sys
import time
import struct
import subprocess
import ctypes
from ctypes import wintypes
from PIL import Image

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE_PATH = os.path.join(REPO_ROOT, "x64", "Release", "miu.exe")
OUT_DIR = os.path.join(REPO_ROOT, "package", "screenshots")
SAMPLES_DIR = os.path.join(REPO_ROOT, "package", "screenshots_samples")

os.makedirs(OUT_DIR, exist_ok=True)

LANG_CONFIGS = [
    # Tier 1
    ("zh-tw", "sample_zh_tw.md", "zh_tw"),
    ("de-de", "sample_de.md", "de"),
    ("fr-fr", "sample_fr.md", "fr"),
    ("es-es", "sample_es.md", "es"),
    ("pt-br", "sample_pt_br.md", "pt_br"),
    # Tier 2
    ("ru-ru", "sample_ru.md", "ru"),
    ("it-it", "sample_it.md", "it"),
    ("pl-pl", "sample_pl.md", "pl"),
    ("nl-nl", "sample_nl.md", "nl"),
    ("tr-tr", "sample_tr.md", "tr"),
    # Tier 3
    ("vi-vn", "sample_vi.md", "vi"),
    ("uk-ua", "sample_uk.md", "uk"),
    ("id-id", "sample_id.md", "id"),
    # Tier 4 & 5 (11 new languages)
    ("cs-cz", "sample_cs.md", "cs"),
    ("sv-se", "sample_sv.md", "sv"),
    ("th-th", "sample_th.md", "th"),
    ("hi-in", "sample_hi.md", "hi"),
    ("ro-ro", "sample_ro.md", "ro"),
    ("hu-hu", "sample_hu.md", "hu"),
    ("el-gr", "sample_el.md", "el"),
    ("da-dk", "sample_da.md", "da"),
    ("fi-fi", "sample_fi.md", "fi"),
    ("nb-no", "sample_nb.md", "nb"),
    ("ms-my", "sample_ms.md", "ms"),
]

def capture_window_to_file(hwnd, out_path, width=1280, height=720):
    rect = wintypes.RECT()
    ctypes.windll.user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top

    hwnd_dc = ctypes.windll.user32.GetWindowDC(hwnd)
    mfc_dc = ctypes.windll.gdi32.CreateCompatibleDC(hwnd_dc)
    save_bit_map = ctypes.windll.gdi32.CreateCompatibleBitmap(hwnd_dc, w, h)
    ctypes.windll.gdi32.SelectObject(mfc_dc, save_bit_map)

    # PrintWindow PW_RENDERFULLCONTENT = 2
    res = ctypes.windll.user32.PrintWindow(hwnd, mfc_dc, 2)
    if not res:
        print(f"[Warn] PrintWindow returned {res} for hwnd {hwnd}")

    bmpinfo = bytearray(40)
    struct.pack_into('<IiiHHIIIIII', bmpinfo, 0, 40, w, -h, 1, 32, 0, w * h * 4, 0, 0, 0, 0)
    buf = bytearray(w * h * 4)
    ctypes.windll.gdi32.GetDIBits(mfc_dc, save_bit_map, 0, h, (ctypes.c_char * len(buf)).from_buffer(buf), (ctypes.c_char * 40).from_buffer(bmpinfo), 0)

    im = Image.frombuffer('RGBA', (w, h), buf, 'raw', 'BGRA', 0, 1)

    ctypes.windll.gdi32.DeleteObject(save_bit_map)
    ctypes.windll.gdi32.DeleteDC(mfc_dc)
    ctypes.windll.user32.ReleaseDC(hwnd, hwnd_dc)

    # Ensure RGB / opaque
    rgb_im = Image.new('RGB', (w, h), (0, 0, 0))
    rgb_im.paste(im, mask=None)
    rgb_im.save(out_path, "PNG")
    print(f"Captured: {out_path} ({w}x{h})")

def capture_scenario(lang_code, theme, file_path, is_f1, out_file):
    cmd = [
        EXE_PATH,
        "--lang", lang_code,
        "--theme", theme,
        "--new-window",
        "--width", "1280",
        "--height", "720"
    ]
    if is_f1:
        cmd.append("--f1")
    if file_path:
        cmd.append(file_path)

    p = subprocess.Popen(cmd)
    time.sleep(1.2)

    hwnd = ctypes.windll.user32.FindWindowW("miu", None)
    if not hwnd:
        time.sleep(1.0)
        hwnd = ctypes.windll.user32.FindWindowW("miu", None)

    if not hwnd:
        p.kill()
        raise RuntimeError(f"Could not find miu window for {lang_code} {theme}")

    try:
        capture_window_to_file(hwnd, out_file)
    finally:
        ctypes.windll.user32.PostMessageW(hwnd, 0x0010, 0, 0) # WM_CLOSE
        try:
            p.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            p.kill()
    time.sleep(0.3)

def main():
    if not os.path.exists(EXE_PATH):
        raise FileNotFoundError(f"Binary not found: {EXE_PATH}")

    filter_prefixes = set(sys.argv[1:]) if len(sys.argv) > 1 else None
    configs = [c for c in LANG_CONFIGS if not filter_prefixes or c[2] in filter_prefixes or c[0] in filter_prefixes]

    total = len(configs) * 3
    count = 0
    print(f"=== Starting screenshot capture for {len(configs)} languages (Total: {total} images) ===")

    for lang_code, sample_file, prefix in configs:
        sample_path = os.path.join(SAMPLES_DIR, sample_file)
        if not os.path.exists(sample_path):
            raise FileNotFoundError(f"Sample not found: {sample_path}")

        print(f"\nProcessing [{lang_code}]...")
        # 1. Light mode
        out1 = os.path.join(OUT_DIR, f"{prefix}_1.png")
        capture_scenario(lang_code, "light", sample_path, False, out1)
        count += 1

        # 2. Dark mode
        out2 = os.path.join(OUT_DIR, f"{prefix}_2.png")
        capture_scenario(lang_code, "dark", sample_path, False, out2)
        count += 1

        # 3. F1 Help
        out3 = os.path.join(OUT_DIR, f"{prefix}_3.png")
        capture_scenario(lang_code, "dark", "", True, out3)
        count += 1

    print("\n" + "=" * 50)
    print(f"Successfully generated all {count}/{total} screenshots in {OUT_DIR}!")
    print("=" * 50)

if __name__ == "__main__":
    main()

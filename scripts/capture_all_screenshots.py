# -*- coding: utf-8 -*-
"""
Generate all 28-language × 5-slide (140 PNGs) 1:1 Retina (2880×1620, 16:9 @2x)
minimalist Windows 11 Microsoft Store screenshots into package/screenshots/.
"""

import os
import sys
import time
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from appstore_screenshots_sync import LANG_DECKS, parse_localizable_strings  # noqa: E402
import store_minimal_renderer as smr  # noqa: E402

OUT_DIR = os.path.join(REPO_ROOT, "package", "screenshots")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    filter_prefixes = set(sys.argv[1:]) if len(sys.argv) > 1 else None
    all_prefixes = sorted(LANG_DECKS.keys())
    target_prefixes = [p for p in all_prefixes if not filter_prefixes or p in filter_prefixes]
    if not target_prefixes:
        raise ValueError(f"No matching prefixes for filter: {filter_prefixes}")

    # When rendering the full 28-language set, purge obsolete *_6.png, *_7.png, and *.mp4 files
    if filter_prefixes is None:
        removed = 0
        for fname in os.listdir(OUT_DIR):
            if fname.endswith((".png", ".mp4")):
                os.remove(os.path.join(OUT_DIR, fname))
                removed += 1
        print(f"Cleaned {removed} previous asset(s) from {OUT_DIR}")

    ui_strings_by_prefix = {
        pfx: parse_localizable_strings(LANG_DECKS[pfx]["lproj"]) for pfx in target_prefixes
    }

    print(
        f"=== Rendering {len(target_prefixes)} language(s) × 5 slides "
        f"({len(target_prefixes) * 5} Retina 2880×1620 PNGs) into {OUT_DIR} ==="
    )
    t0 = time.time()
    rendered = smr.render_minimal_batch(
        target_prefixes,
        ["WINDOWS_DESKTOP"],
        OUT_DIR,
        ui_strings_by_prefix,
        workers=4,
        filename_pattern="{pfx}_{s_idx}.png",
    )

    # Verify all generated PNGs
    verified = 0
    for pfx in target_prefixes:
        for i in range(1, 6):
            fpath = os.path.join(OUT_DIR, f"{pfx}_{i}.png")
            if not os.path.isfile(fpath):
                raise RuntimeError(f"Missing expected screenshot: {fpath}")
            with Image.open(fpath) as im:
                if im.size != (2880, 1620) or im.mode != "RGB":
                    raise RuntimeError(f"Invalid screenshot {fpath}: size={im.size}, mode={im.mode}")
            verified += 1

    elapsed = time.time() - t0
    print(f"[OK] Rendered and verified {verified} Retina (2880×1620 RGB) screenshots in {elapsed:.2f}s")
    return rendered


if __name__ == "__main__":
    main()

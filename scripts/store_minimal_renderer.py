# -*- coding: utf-8 -*-
"""
1:1 Native Retina Minimalist Screenshot Renderer for miu (Windows & Linux/macOS).
Uses:
  1. Real C++ MiuSyntax + MiuPreview + Pele exporter (scripts/export_store_engine.cpp)
  2. Headless Chromium / Google Chrome / Microsoft Edge + pure-stdlib CDP
     with integer @3x / @2x Retina deviceScaleFactor (zero bitmap upscaling, zero color bleed).
"""

import base64
import html as html_mod
import io
import json
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from PIL import Image

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent

from store_minimal_i18n import LANG_PACKS, get_lang_pack  # noqa: E402


# ============================================================================
# Browser & C++ Compiler Discovery (Windows & Linux/macOS)
# ============================================================================
def find_headless_browser() -> str:
    env_bin = os.environ.get("MIU_BROWSER_BIN", "").strip()
    if env_bin and os.path.isfile(env_bin):
        return env_bin

    if os.name == "nt":
        win_candidates = [
            r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
            r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
            r"C:\Program Files\Google\Chrome\Application\chrome.exe",
            r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
        ]
        for p in win_candidates:
            if os.path.isfile(p):
                return p

    for cmd in (
        "google-chrome",
        "google-chrome-stable",
        "microsoft-edge",
        "microsoft-edge-stable",
        "chromium",
        "chromium-browser",
        "msedge",
        "chrome",
    ):
        found = shutil.which(cmd)
        if found:
            return found

    mac_candidates = [
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
        "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
    ]
    for p in mac_candidates:
        if os.path.isfile(p):
            return p

    raise RuntimeError("No headless Chromium/Chrome/Edge browser found.")


def _find_vcvars64() -> str:
    candidates = [
        r"C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    vswhere = r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if os.path.isfile(vswhere):
        res = subprocess.run(
            [vswhere, "-latest", "-find", r"VC\Auxiliary\Build\vcvars64.bat"],
            capture_output=True,
            text=True,
            timeout=20,
        )
        for line in res.stdout.splitlines():
            p = line.strip()
            if os.path.isfile(p):
                return p
    raise RuntimeError("vcvars64.bat not found on Windows.")


def build_and_run_engine_exporter(prefixes: list[str], work_dir: Path) -> dict:
    """Compile scripts/export_store_engine.cpp and export real MiuSyntax + MiuPreview JSON."""
    work_dir.mkdir(parents=True, exist_ok=True)
    cpp_src = SCRIPT_DIR / "export_store_engine.cpp"
    inc_dir = REPO_ROOT / "include"
    assert cpp_src.is_file(), f"Missing {cpp_src}"

    exe_path = work_dir / ("export_store_engine.exe" if os.name == "nt" else "export_store_engine")
    if not exe_path.is_file() or exe_path.stat().st_mtime < cpp_src.stat().st_mtime:
        if os.name == "nt":
            vcvars = _find_vcvars64()
            bat_path = work_dir / "build_exporter.bat"
            obj_path = work_dir / "export_store_engine.obj"
            bat_path.write_text(
                f'@echo off\r\n'
                f'call "{vcvars}" >nul\r\n'
                f'if errorlevel 1 exit /b 1\r\n'
                f'cl /nologo /bigobj /std:c++20 /EHsc /utf-8 /O2 /I "{inc_dir}" "{cpp_src}" /Fo:"{obj_path}" /Fe:"{exe_path}"\r\n',
                encoding="utf-8",
            )
            res = subprocess.run(["cmd.exe", "/c", str(bat_path)], capture_output=True, text=True, timeout=180)
            if res.returncode != 0:
                raise RuntimeError(f"MSVC build of export_store_engine.cpp failed:\n{res.stdout}\n{res.stderr}")
        else:
            cxx = shutil.which("g++") or shutil.which("clang++") or "c++"
            res = subprocess.run(
                [cxx, "-std=c++20", "-O2", f"-I{inc_dir}", str(cpp_src), "-o", str(exe_path)],
                capture_output=True,
                text=True,
                timeout=180,
            )
            if res.returncode != 0:
                raise RuntimeError(f"{cxx} build of export_store_engine.cpp failed:\n{res.stdout}\n{res.stderr}")

    # Write binary record bundle for all requested prefixes
    slide_defs = [
        ("s1", "md", 1, 1),
        ("s2", "md", 0, 1),
        ("s3", "md", 1, 1),
        ("s4", "log", 0, 0),
        ("s5", "ts", 1, 0),
    ]
    records = []
    for pfx in prefixes:
        pack = get_lang_pack(pfx)
        for key, ext, is_dark, has_prev in slide_defs:
            raw_bytes = pack[key].encode("utf-8")
            records.append((pfx, key, ext, is_dark, has_prev, raw_bytes))

    in_bin = work_dir / "slides_input.bin"
    out_json = work_dir / "engine_renders.json"
    with open(in_bin, "wb") as f:
        f.write(f"{len(records)}\n".encode("ascii"))
        for pfx, key, ext, is_dark, has_prev, raw_bytes in records:
            meta = f"{pfx}\t{key}\t{ext}\t{is_dark}\t{has_prev}\t{len(raw_bytes)}\n".encode("utf-8")
            f.write(meta)
            f.write(raw_bytes)
            f.write(b"\n")

    run_res = subprocess.run([str(exe_path), str(in_bin), str(out_json)], capture_output=True, text=True, timeout=60)
    if run_res.returncode != 0:
        raise RuntimeError(f"export_store_engine failed:\n{run_res.stdout}\n{run_res.stderr}")

    return json.loads(out_json.read_text(encoding="utf-8"))


# ============================================================================
# Minimal Pure-Stdlib CDP WebSocket Client
# ============================================================================
class SimpleCDP:
    def __init__(self, ws_url: str):
        m = re.match(r"ws://([^/:]+):(\d+)(/.*)", ws_url)
        assert m, f"Invalid ws_url: {ws_url}"
        host, port, path = m.group(1), int(m.group(2)), m.group(3)
        self.sock = socket.create_connection((host, port), timeout=45)
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        req = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {host}:{port}\r\n"
            f"Upgrade: websocket\r\n"
            f"Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            f"Sec-WebSocket-Version: 13\r\n\r\n"
        )
        self.sock.sendall(req.encode("ascii"))
        buf = b""
        while b"\r\n\r\n" not in buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RuntimeError("WebSocket handshake EOF")
            buf += chunk
        header, self.extra = buf.split(b"\r\n\r\n", 1)
        assert b"101" in header.splitlines()[0], f"WS handshake failed: {header!r}"
        self._msg_id = 0

    def _recv_exact(self, n: int) -> bytes:
        out = bytearray()
        if self.extra:
            take = min(len(self.extra), n)
            out.extend(self.extra[:take])
            self.extra = self.extra[take:]
        while len(out) < n:
            chunk = self.sock.recv(n - len(out))
            if not chunk:
                raise RuntimeError("WebSocket unexpected EOF")
            out.extend(chunk)
        return bytes(out)

    def _send_frame(self, payload: bytes):
        mask_key = os.urandom(4)
        header = bytearray([0x81])
        n = len(payload)
        if n < 126:
            header.append(0x80 | n)
        elif n < 65536:
            header.append(0x80 | 126)
            header.extend(struct.pack("!H", n))
        else:
            header.append(0x80 | 127)
            header.extend(struct.pack("!Q", n))
        header.extend(mask_key)
        masked = bytes(b ^ mask_key[i & 3] for i, b in enumerate(payload))
        self.sock.sendall(bytes(header) + masked)

    def _recv_frame(self) -> str:
        while True:
            b0, b1 = self._recv_exact(2)
            opcode = b0 & 0x0F
            masked = (b1 & 0x80) != 0
            length = b1 & 0x7F
            if length == 126:
                length = struct.unpack("!H", self._recv_exact(2))[0]
            elif length == 127:
                length = struct.unpack("!Q", self._recv_exact(8))[0]
            mask_key = self._recv_exact(4) if masked else b""
            data = self._recv_exact(length)
            if masked:
                data = bytes(b ^ mask_key[i & 3] for i, b in enumerate(data))
            if opcode == 0x8:
                raise RuntimeError("WebSocket closed")
            if opcode == 0x9:
                pong = bytearray([0x8A, 0x80, 0, 0, 0, 0])
                self.sock.sendall(bytes(pong))
                continue
            if opcode in (0x1, 0x2):
                return data.decode("utf-8")

    def call(self, method: str, params: dict = None) -> dict:
        self._msg_id += 1
        mid = self._msg_id
        msg = json.dumps({"id": mid, "method": method, "params": params or {}}).encode("utf-8")
        self._send_frame(msg)
        while True:
            raw = self._recv_frame()
            obj = json.loads(raw)
            if obj.get("id") == mid:
                if "error" in obj:
                    raise RuntimeError(f"CDP error in {method}: {obj['error']}")
                return obj.get("result", {})

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass


# ============================================================================
# HTML / SVG Chrome & Slide Layout Builder
# ============================================================================
SLIDE_SPEC_ORDER = [
    (1, "s1", "README.md", "split"),
    (2, "s2", "architecture.md", "split"),
    (3, "s3", "math_notes.md", "split"),
    (4, "s4", "access_audit.log", "regex"),
    (5, "s5", "theme_engine.ts", "multicursor"),
]


def _esc(s: str) -> str:
    return html_mod.escape(s, quote=True)


def decorate_special_lines(lines_html: list, mode: str) -> list:
    out = []
    first_warn = True
    for lh in lines_html:
        if mode == "regex" and "WARN" in lh:
            pos_w = lh.find("WARN")
            pos_rb = lh.find("]", pos_w)
            if pos_rb != -1:
                cls = "rx-hit rx-active" if first_warn else "rx-hit"
                first_warn = False
                open_sp = lh.rfind("<span", 0, pos_rb)
                if open_sp != -1 and lh.find("</span>", open_sp, pos_rb) == -1:
                    open_gt = lh.find(">", open_sp)
                    span_open_tag = lh[open_sp : open_gt + 1]
                    lh = (
                        f'<mark class="{cls}">'
                        + lh[: pos_rb + 1]
                        + "</span></mark>"
                        + span_open_tag
                        + lh[pos_rb + 1 :]
                    )
                else:
                    lh = f'<mark class="{cls}">' + lh[: pos_rb + 1] + "</mark>" + lh[pos_rb + 1 :]
        elif mode == "multicursor":
            lh = lh.replace(
                "renderFrame",
                '<span class="mc-sel">renderFrame<span class="mc-caret"></span></span>',
            )
        out.append(lh)
    return out


def ios_status_icons_svg(fg: str) -> str:
    return f"""
    <div class="ios-status-right">
      <svg width="18" height="12" viewBox="0 0 18 12" fill="none">
        <rect x="0.5" y="7.5" width="3" height="4" rx="1" fill="{fg}"/>
        <rect x="5" y="5.5" width="3" height="6" rx="1" fill="{fg}"/>
        <rect x="9.5" y="3" width="3" height="8.5" rx="1" fill="{fg}"/>
        <rect x="14" y="0.5" width="3" height="11" rx="1" fill="{fg}"/>
      </svg>
      <svg width="16" height="12" viewBox="0 0 16 12" fill="none">
        <path d="M8 9.6a1.4 1.4 0 1 0 0 2.8 1.4 1.4 0 0 0 0-2.8Zm-3.3-1.9a4.8 4.8 0 0 1 6.6 0l1.2-1.2a6.5 6.5 0 0 0-9 0l1.2 1.2Zm-2.6-2.6a8.5 8.5 0 0 1 11.8 0l1.2-1.2a10.2 10.2 0 0 0-14.2 0l1.2 1.2Z" fill="{fg}"/>
      </svg>
      <svg width="25" height="12" viewBox="0 0 25 12" fill="none">
        <rect x="0.75" y="1" width="21" height="10" rx="3" stroke="{fg}" stroke-opacity="0.45" stroke-width="1.2"/>
        <rect x="2.5" y="2.75" width="17.5" height="6.5" rx="1.6" fill="{fg}"/>
        <path d="M23 4.2v3.6a1.8 1.8 0 0 0 0-3.6Z" fill="{fg}" fill-opacity="0.45"/>
      </svg>
    </div>
    """


def android_status_icons_svg(fg: str) -> str:
    return f"""
    <div class="ios-status-right">
      <svg width="15" height="12" viewBox="0 0 16 12" fill="none">
        <path d="M8 11.2 15.2 2.4C13.2 1.0 10.7 0.2 8 0.2S2.8 1.0 0.8 2.4L8 11.2Z" fill="{fg}"/>
      </svg>
      <svg width="13" height="12" viewBox="0 0 13 12" fill="none">
        <path d="M1 11.2H12V0.8L1 11.2Z" fill="{fg}"/>
      </svg>
      <svg width="9" height="13" viewBox="0 0 10 14" fill="none">
        <rect x="3" y="0.5" width="4" height="1.5" rx="0.6" fill="{fg}"/>
        <rect x="1.2" y="2" width="7.6" height="11" rx="1.4" fill="{fg}"/>
      </svg>
    </div>
    """


def build_ios_pill_bar_html(ui_strings: dict, active_cat: str, compact: bool = False) -> str:
    cats = [
        (_esc(ui_strings.get("File", "File")), "file"),
        (_esc(ui_strings.get("Edit", "Edit")), "edit"),
        (_esc(ui_strings.get("Find", "Find")), "find"),
        (_esc(ui_strings.get("View", "View")), "view"),
    ]
    pills = []
    for label, key in cats:
        cls = "pill-item active" if key == active_cat else "pill-item"
        pills.append(
            f'<div class="{cls}"><span>{label}</span>'
            f'<svg width="9" height="6" viewBox="0 0 9 6" fill="none"><path d="M1.5 1.5L4.5 4.5L7.5 1.5" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>'
            f'</div>'
        )
    tab_lbl = _esc(ui_strings.get("Tab", "Tab"))
    extra = "" if compact else f'<div class="pill-item"><span>{tab_lbl}</span></div>'
    return f"""
    <div class="ios-pillbar-wrap">
      <div class="ios-pillbar {'compact' if compact else ''}">
        <div class="pill-group">
          {''.join(pills)}
        </div>
        <div class="pill-sep"></div>
        <div class="pill-actions">
          <div class="pill-icon" aria-label="Undo">
            <svg width="14" height="14" viewBox="0 0 14 14" fill="none"><path d="M4 5H9a3 3 0 1 1 0 6H6.5M4 5l2.2-2.2M4 5l2.2 2.2" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>
          </div>
          <div class="pill-icon" aria-label="Redo">
            <svg width="14" height="14" viewBox="0 0 14 14" fill="none"><path d="M10 5H5a3 3 0 1 0 0 6h2.5M10 5L7.8 2.8M10 5L7.8 7.2" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>
          </div>
          {extra}
        </div>
      </div>
    </div>
    """


def build_macos_replace_sheet_html(ui_strings: dict, repl_val: str) -> str:
    t_replace = _esc(ui_strings.get("Replace", "Replace"))
    t_find_l = _esc(ui_strings.get("Find:", "Find:"))
    t_repl_l = _esc(ui_strings.get("Replace:", "Replace:"))
    t_mc = _esc(ui_strings.get("Match Case", "Match Case"))
    t_rx = _esc(ui_strings.get("Regex", "Regex"))
    t_ra = _esc(ui_strings.get("Replace All", "Replace All"))
    t_fn = _esc(ui_strings.get("Find Next", "Find Next"))
    return f"""
    <div class="mac-replace-sheet">
      <div class="mac-sheet-tb">
        <div class="mac-dots small">
          <span class="dot r"></span><span class="dot y"></span><span class="dot g off"></span>
        </div>
        <div class="mac-sheet-title">{t_replace}</div>
      </div>
      <div class="mac-sheet-body">
        <div class="mac-fields-grid">
          <label>{t_find_l}</label>
          <div class="mac-input focus">(\\d{{4}})/(\\d{{2}})/(\\d{{2}}) \\[WARN\\]</div>
          <label>{t_repl_l}</label>
          <div class="mac-input">{_esc(repl_val)}</div>
        </div>
        <div class="mac-checks">
          <span class="chk"><span class="box"></span>{t_mc}</span>
          <span class="chk on"><span class="box checked">✓</span>{t_rx}</span>
        </div>
        <div class="mac-btns">
          <span class="mbtn">{t_ra}</span>
          <span class="mbtn">{t_replace}</span>
          <span class="mbtn primary">{t_fn}</span>
        </div>
      </div>
    </div>
    """


def build_ios_replace_bar_html(ui_strings: dict, repl_val: str) -> str:
    t_done = _esc(ui_strings.get("Done", "Done"))
    t_replace = _esc(ui_strings.get("Replace", "Replace"))
    t_all = _esc(ui_strings.get("All", "All"))
    t_mc = _esc(ui_strings.get("Match Case", "Match Case"))
    t_ww = _esc(ui_strings.get("Whole Word", "Whole Word"))
    t_rx = _esc(ui_strings.get("Regex", "Regex"))
    return f"""
    <div class="ios-find-bar">
      <div class="ifb-row">
        <div class="ifb-input focus">(\\d{{4}})/(\\d{{2}})/(\\d{{2}}) \\[WARN\\]</div>
        <div class="ifb-nav"><span>‹</span><span>›</span><span class="done">{t_done}</span></div>
      </div>
      <div class="ifb-row">
        <div class="ifb-input">{_esc(repl_val)}</div>
        <div class="ifb-nav"><span class="act">{t_replace}</span><span class="act">{t_all}</span></div>
      </div>
      <div class="ifb-pills">
        <span class="ifb-opt">{t_mc}</span>
        <span class="ifb-opt">{t_ww}</span>
        <span class="ifb-opt selected">{t_rx}</span>
      </div>
    </div>
    """


def build_editor_html(lines_html: list, font_px: float, line_h: float, max_lines: int, start_line: int = 1) -> str:
    rows = []
    for i, lh in enumerate(lines_html[:max_lines], start=start_line):
        content = lh if lh else "&nbsp;"
        rows.append(
            f'<div class="ed-line">'
            f'<span class="ed-ln">{i}</span>'
            f'<span class="ed-code">{content}</span>'
            f'</div>'
        )
    return f'<div class="miu-editor" style="font-size:{font_px}px;line-height:{line_h};">{"".join(rows)}</div>'


JS_AUTO_FIT = """
(() => {
  const hl = document.querySelector('.headline');
  if (hl) {
    const maxW = document.body.clientWidth - 48;
    let maxRowW = 0;
    hl.querySelectorAll('.hl-row').forEach(r => {
      if (r.scrollWidth > maxRowW) maxRowW = r.scrollWidth;
    });
    if (maxRowW > maxW && maxRowW > 0) {
      const cur = parseFloat(getComputedStyle(hl).fontSize);
      hl.style.fontSize = Math.max(18, Math.floor(cur * (maxW / maxRowW) * 10) / 10) + 'px';
    }
  }
  document.querySelectorAll('.ios-pillbar-wrap').forEach(wrap => {
    const bar = wrap.querySelector('.ios-pillbar');
    const avail = wrap.clientWidth - 16;
    if (bar && avail > 0 && bar.scrollWidth > avail) {
      const scale = Math.max(0.70, avail / bar.scrollWidth);
      bar.style.transform = 'scale(' + scale.toFixed(3) + ')';
      bar.style.transformOrigin = 'center center';
    }
  });
  document.querySelectorAll('.ifb-pills').forEach(pills => {
    const par = pills.parentElement;
    if (par && pills.scrollWidth > par.clientWidth - 20) {
      const scale = Math.max(0.72, (par.clientWidth - 20) / pills.scrollWidth);
      pills.style.transform = 'scale(' + scale.toFixed(3) + ')';
      pills.style.transformOrigin = 'left center';
    }
  });
  document.querySelectorAll('.mac-replace-sheet').forEach(sh => {
    const par = sh.parentElement;
    if (par && sh.offsetWidth > par.clientWidth * 0.48) {
      const scale = Math.max(0.80, (par.clientWidth * 0.48) / sh.offsetWidth);
      sh.style.transform = 'scale(' + scale.toFixed(3) + ')';
      sh.style.transformOrigin = 'top right';
    }
  });
  document.querySelectorAll('.pane-top, .iphone-card .single-pane .miu-editor, .android-phone-card .single-pane .miu-editor').forEach(box => {
    const boxBottom = box.getBoundingClientRect().bottom - 3;
    const lines = Array.from(box.querySelectorAll('.ed-line'));
    let cutFrom = -1;
    for (let i = 0; i < lines.length; i++) {
      if (lines[i].getBoundingClientRect().bottom > boxBottom) {
        cutFrom = i;
        break;
      }
    }
    if (cutFrom !== -1) {
      for (let j = cutFrom; j < lines.length; j++) {
        lines[j].style.display = 'none';
      }
    }
  });
  document.querySelectorAll('.pane-bot').forEach(bot => {
    const root = bot.querySelector('#__miu_root');
    if (root && root.scrollHeight > bot.clientHeight) {
      const z = Math.max(0.82, (bot.clientHeight - 4) / root.scrollHeight);
      root.style.zoom = z.toFixed(3);
    }
  });
  return true;
})();
"""


def build_slide_html(
    display_type: str,
    prefix: str,
    slide_idx: int,
    slide_key: str,
    doc_title: str,
    mode: str,
    ui_strings: dict,
    engine_data: dict,
    duo_target_size: tuple[int, int] = (2853, 2007),
) -> tuple[str, int, int, int]:
    pack = get_lang_pack(prefix)
    lang_renders = engine_data["languages"].get(prefix) or engine_data["languages"]["en"]
    slide = lang_renders[slide_key]
    is_dark = slide["isDark"]
    md_css = engine_data["mdThemeCss"]
    lines_decorated = decorate_special_lines(slide["lines"], mode)
    preview_inner = slide["previewHtml"]
    active_cat = "view" if mode == "split" else ("find" if mode == "regex" else "edit")
    hl_land, hl_port = pack["headlines"][slide_key]

    is_rtl = bool(pack.get("rtl", False))
    hl_dir_attr = ' dir="rtl"' if is_rtl else ""
    prev_dir_attr = ' dir="rtl"' if is_rtl else ""

    if display_type in ("APP_IPHONE_67", "APP_IPHONE_65"):
        if display_type == "APP_IPHONE_67":
            vw, vh, dpr = 430, 932, 3
            card_w, card_h = 368, 684
        else:
            vw, vh, dpr = 428, 926, 3
            card_w, card_h = 366, 678

        headline_html = "".join(
            '<div class="hl-row">'
            + " ".join(f'<span class="{"hl-em" if em else "hl-sub"}">{_esc(txt)}</span>' for txt, em in row)
            + "</div>"
            for row in hl_port
        )
        # For CJK languages without spaces between spans inside a single row, omit space
        if prefix in ("ja", "zh_cn", "zh_tw"):
            headline_html = "".join(
                '<div class="hl-row">'
                + "".join(f'<span class="{"hl-em" if em else "hl-sub"}">{_esc(txt)}</span>' for txt, em in row)
                + "</div>"
                for row in hl_port
            )

        fg_stat = "#F5F5F7" if is_dark else "#1D1D1F"
        iphone_max_by_key = {"s1": 13, "s2": 11, "s3": 9, "s4": 13, "s5": 26}
        ed_max = iphone_max_by_key.get(slide_key, 15)
        ed_html = build_editor_html(lines_decorated, 11, 1.56, ed_max)

        if mode == "split":
            body_html = f"""
            <div class="split-vertical">
              <div class="pane-top">{ed_html}</div>
              <div class="v-splitter"><span class="v-grabber"></span></div>
              <div class="pane-bot"><div id="__miu_root" class="preview-Zoom-iphone">{preview_inner}</div></div>
            </div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=True)}
            """
        elif mode == "regex":
            body_html = f"""
            <div class="single-pane">
              {ed_html}
              {build_ios_replace_bar_html(ui_strings, pack['repl_val'])}
            </div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=True)}
            """
        else:
            body_html = f"""
            <div class="single-pane">{ed_html}</div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=True)}
            """

        device_html = f"""
        <div class="device-card iphone-card {'dark' if is_dark else 'light'}" style="width:{card_w}px;height:{card_h}px;">
          <div class="ios-statusbar">
            <span class="ios-time">9:41</span>
            {ios_status_icons_svg(fg_stat)}
          </div>
          <div class="ios-docbar">{_esc(doc_title)}</div>
          <div class="card-content">{body_html}</div>
        </div>
        """
        header_cls = "header-portrait"

    elif display_type in ("APP_IPAD_PRO_3GEN_129", "APP_IPAD_PRO_129"):
        vw, vh, dpr = 1366, 1024, 2
        headline_html = (
            '<span class="hl-row">'
            + "".join(f'<span class="{"hl-em" if em else "hl-sub"}">{_esc(txt)}</span>' for txt, em in hl_land)
            + "</span>"
        )
        fg_stat = "#F5F5F7" if is_dark else "#1D1D1F"
        ed_html = build_editor_html(lines_decorated, 14, 1.66, 28)

        if mode == "split":
            body_html = f"""
            <div class="split-horizontal">
              <div class="pane-left">{ed_html}</div>
              <div class="h-splitter"></div>
              <div class="pane-right"><div id="__miu_root" class="preview-pad">{preview_inner}</div></div>
            </div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=False)}
            """
        elif mode == "regex":
            body_html = f"""
            <div class="single-pane">
              {ed_html}
              <div class="ipad-find-wrap">{build_ios_replace_bar_html(ui_strings, pack['repl_val'])}</div>
            </div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=False)}
            """
        else:
            body_html = f"""
            <div class="single-pane">{ed_html}</div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=False)}
            """

        device_html = f"""
        <div class="device-card ipad-card {'dark' if is_dark else 'light'}">
          <div class="ios-statusbar ipad-sb">
            <span class="ios-time">9:41 &nbsp;{_esc(pack['ipad_date'])}</span>
            <span class="ipad-doc">{_esc(doc_title)}</span>
            {ios_status_icons_svg(fg_stat)}
          </div>
          <div class="card-content">{body_html}</div>
        </div>
        """
        header_cls = "header-landscape-lg"

    elif "DUO" in display_type or "FOLD" in display_type or display_type == "APP_IPHONE_DUO":
        if duo_target_size == (2034, 1398):
            vw, vh, dpr = 678, 466, 3
            book_w, book_h = 598, 366
        else:
            vw, vh, dpr = 951, 669, 3
            book_w, book_h = 836, 522

        headline_html = (
            '<span class="hl-row">'
            + "".join(f'<span class="{"hl-em" if em else "hl-sub"}">{_esc(txt)}</span>' for txt, em in hl_land)
            + "</span>"
        )
        fg_stat = "#F5F5F7" if is_dark else "#1D1D1F"
        ed_html = build_editor_html(lines_decorated, 11, 1.60, 24)

        if mode == "split":
            left_pane_body = f"""
            <div class="duo-pane-inner">{ed_html}</div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=True)}
            """
            right_pane_body = f"""
            <div class="duo-pane-inner"><div id="__miu_root" class="preview-duo">{preview_inner}</div></div>
            <div class="duo-status-foot">Ln 1, Col 1 &nbsp;·&nbsp; UTF-8 &nbsp;·&nbsp; LF</div>
            """
        elif mode == "regex":
            ed_left = build_editor_html(lines_decorated[:10], 11, 1.58, 10, start_line=1)
            ed_right = build_editor_html(lines_decorated[10:], 11, 1.58, 8, start_line=11)
            left_pane_body = f"""
            <div class="duo-pane-inner">{ed_left}</div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=True)}
            """
            right_pane_body = f"""
            <div class="duo-pane-inner" style="position:relative;">
              {ed_right}
              {build_ios_replace_bar_html(ui_strings, pack['repl_val'])}
            </div>
            """
        else:
            ed_left = build_editor_html(lines_decorated[:13], 11, 1.62, 13, start_line=1)
            ed_right = build_editor_html(lines_decorated[13:], 11, 1.62, 12, start_line=14)
            left_pane_body = f"""
            <div class="duo-pane-inner">{ed_left}</div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=True)}
            """
            right_pane_body = f"""
            <div class="duo-pane-inner">{ed_right}</div>
            <div class="duo-status-foot">{_esc(pack['duo_mc_status'])}</div>
            """

        device_html = f"""
        <div class="duo-book {'dark' if is_dark else 'light'}" style="width:{book_w}px;height:{book_h}px;">
          <div class="duo-half left-half">
            <div class="ios-statusbar duo-sb">
              <span class="ios-time">9:41</span>
              <span class="duo-doc">{_esc(doc_title)}</span>
            </div>
            <div class="card-content">{left_pane_body}</div>
          </div>
          <div class="duo-hinge"></div>
          <div class="duo-half right-half">
            <div class="ios-statusbar duo-sb">
              <span class="duo-doc">{'Live Preview' if mode == 'split' else 'miu — Dual Screen'}</span>
              {ios_status_icons_svg(fg_stat)}
            </div>
            <div class="card-content">{right_pane_body}</div>
          </div>
        </div>
        """
        header_cls = "header-landscape-duo"

    elif display_type == "APP_DESKTOP":
        vw, vh, dpr = 1440, 900, 2
        headline_html = (
            '<span class="hl-row">'
            + "".join(f'<span class="{"hl-em" if em else "hl-sub"}">{_esc(txt)}</span>' for txt, em in hl_land)
            + "</span>"
        )
        ed_html = build_editor_html(lines_decorated, 14, 1.66, 26)

        if mode == "split":
            body_html = f"""
            <div class="split-horizontal">
              <div class="pane-left">{ed_html}</div>
              <div class="h-splitter"></div>
              <div class="pane-right"><div id="__miu_root" class="preview-mac">{preview_inner}</div></div>
            </div>
            """
        elif mode == "regex":
            body_html = f"""
            <div class="single-pane">
              {ed_html}
              {build_macos_replace_sheet_html(ui_strings, pack['repl_val'])}
            </div>
            """
        else:
            body_html = f"""
            <div class="single-pane">{ed_html}</div>
            """

        device_html = f"""
        <div class="device-card mac-card {'dark' if is_dark else 'light'}">
          <div class="mac-titlebar">
            <div class="mac-dots">
              <span class="dot r"></span><span class="dot y"></span><span class="dot g"></span>
            </div>
            <div class="mac-wintitle">{_esc(doc_title)} — miu</div>
          </div>
          <div class="card-content">{body_html}</div>
        </div>
        """
        header_cls = "header-landscape-mac"

    elif display_type == "ANDROID_PHONE":
        vw, vh, dpr = 432, 768, 3
        card_w, card_h = 376, 616
        if prefix in ("ja", "zh_cn", "zh_tw"):
            headline_html = "".join(
                '<div class="hl-row">'
                + "".join(f'<span class="{"hl-em" if em else "hl-sub"}">{_esc(txt)}</span>' for txt, em in row)
                + "</div>"
                for row in hl_port
            )
        else:
            headline_html = "".join(
                '<div class="hl-row">'
                + " ".join(f'<span class="{"hl-em" if em else "hl-sub"}">{_esc(txt)}</span>' for txt, em in row)
                + "</div>"
                for row in hl_port
            )
        fg_stat = "#F5F5F7" if is_dark else "#1D1D1F"
        aphone_max = {"s1": 13, "s2": 11, "s3": 9, "s4": 12, "s5": 25}
        ed_font = 10.3 if mode == "split" else 10.8
        ed_lh = 1.48 if mode == "split" else 1.52
        ed_html = build_editor_html(lines_decorated, ed_font, ed_lh, aphone_max.get(slide_key, 13))

        if mode == "split":
            body_html = f"""
            <div class="split-vertical">
              <div class="pane-top">{ed_html}</div>
              <div class="v-splitter"><span class="v-grabber"></span></div>
              <div class="pane-bot"><div id="__miu_root" class="preview-Zoom-iphone preview-Zoom-android"{prev_dir_attr}>{preview_inner}</div></div>
            </div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=True)}
            """
        elif mode == "regex":
            body_html = f"""
            <div class="single-pane">
              {ed_html}
              {build_ios_replace_bar_html(ui_strings, pack['repl_val'])}
            </div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=True)}
            """
        else:
            body_html = f"""
            <div class="single-pane">{ed_html}</div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=True)}
            """

        device_html = f"""
        <div class="device-card android-phone-card {'dark' if is_dark else 'light'}" style="width:{card_w}px;height:{card_h}px;">
          <div class="ios-statusbar android-sb">
            <span class="ios-time">9:30</span>
            {android_status_icons_svg(fg_stat)}
          </div>
          <div class="ios-docbar">{_esc(doc_title)}</div>
          <div class="card-content">{body_html}</div>
        </div>
        """
        header_cls = "header-portrait-android"

    elif display_type == "ANDROID_TABLET":
        vw, vh, dpr = 1280, 800, 2
        headline_html = (
            '<span class="hl-row">'
            + "".join(f'<span class="{"hl-em" if em else "hl-sub"}">{_esc(txt)}</span>' for txt, em in hl_land)
            + "</span>"
        )
        fg_stat = "#F5F5F7" if is_dark else "#1D1D1F"
        ed_html = build_editor_html(lines_decorated, 13, 1.62, 24)

        if mode == "split":
            body_html = f"""
            <div class="split-horizontal">
              <div class="pane-left">{ed_html}</div>
              <div class="h-splitter"></div>
              <div class="pane-right"><div id="__miu_root" class="preview-pad"{prev_dir_attr}>{preview_inner}</div></div>
            </div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=False)}
            """
        elif mode == "regex":
            body_html = f"""
            <div class="single-pane">
              {ed_html}
              <div class="ipad-find-wrap">{build_ios_replace_bar_html(ui_strings, pack['repl_val'])}</div>
            </div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=False)}
            """
        else:
            body_html = f"""
            <div class="single-pane">{ed_html}</div>
            {build_ios_pill_bar_html(ui_strings, active_cat, compact=False)}
            """

        device_html = f"""
        <div class="device-card android-tablet-card {'dark' if is_dark else 'light'}">
          <div class="ios-statusbar ipad-sb">
            <span class="ios-time">9:30 &nbsp;{_esc(pack['ipad_date'])}</span>
            <span class="ipad-doc">{_esc(doc_title)}</span>
            {android_status_icons_svg(fg_stat)}
          </div>
          <div class="card-content">{body_html}</div>
        </div>
        """
        header_cls = "header-landscape-android"
    else:
        raise ValueError(f"Unsupported display_type: {display_type}")

    ui_font_stack = (
        "-apple-system, 'Segoe UI', 'Noto Sans JP', 'Noto Sans CJK JP', 'Noto Sans CJK SC', "
        "'Noto Sans CJK TC', 'Noto Sans CJK KR', 'Microsoft YaHei UI', 'Microsoft JhengHei UI', "
        "'Malgun Gothic', 'Noto Sans Thai', 'Leelawadee UI', 'Noto Sans Devanagari', 'Nirmala UI', "
        "'Noto Sans', sans-serif"
    )
    code_font_stack = (
        "'Consolas', 'Noto Sans Mono', 'DejaVu Sans Mono', 'BIZ UDGothic', 'Noto Sans JP', "
        "'Noto Sans CJK JP', 'Noto Sans CJK SC', 'Noto Sans CJK TC', 'Noto Sans CJK KR', "
        "'Microsoft YaHei UI', 'Microsoft JhengHei UI', 'Malgun Gothic', 'Noto Sans Thai', "
        "'Leelawadee UI', 'Noto Sans Devanagari', 'Nirmala UI', 'Segoe UI', 'Noto Sans', monospace"
    )

    html = f"""<!DOCTYPE html>
<html lang="{pack['html_lang']}" data-theme="{'dark' if is_dark else 'light'}">
<head>
<meta charset="utf-8">
{md_css}
<style>
  * {{ box-sizing: border-box; -webkit-font-smoothing: antialiased; text-rendering: geometricPrecision; }}
  html, body {{
    margin: 0;
    padding: 0;
    width: {vw}px;
    height: {vh}px;
    overflow: hidden;
    background: #F2F2F7;
    font-family: {ui_font_stack};
  }}

  .stage {{
    position: relative;
    width: {vw}px;
    height: {vh}px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: space-between;
    background: #F2F2F7;
  }}

  .headline {{
    text-align: center;
    font-feature-settings: "palt" 1;
    letter-spacing: -0.02em;
    margin: 0;
    padding: 0 24px;
    user-select: none;
  }}
  .hl-row {{
    white-space: nowrap;
  }}
  .hl-em {{
    color: #1D1D1F;
    font-weight: 700;
  }}
  .hl-sub {{
    color: #6E6E73;
    font-weight: 600;
  }}
  .header-portrait {{
    padding-top: 86px;
    font-size: 30px;
    line-height: 1.40;
  }}
  .header-portrait-android {{
    padding-top: 44px;
    font-size: 28px;
    line-height: 1.38;
  }}
  .header-landscape-lg {{
    padding-top: 96px;
    font-size: 46px;
    line-height: 1.25;
  }}
  .header-landscape-lg .hl-em + .hl-sub,
  .header-landscape-lg .hl-sub + .hl-em {{ margin-left: 0.22em; }}
  .header-landscape-mac {{
    padding-top: 82px;
    font-size: 44px;
    line-height: 1.25;
  }}
  .header-landscape-mac .hl-em + .hl-sub,
  .header-landscape-mac .hl-sub + .hl-em {{ margin-left: 0.22em; }}
  .header-landscape-android {{
    padding-top: 62px;
    font-size: 40px;
    line-height: 1.25;
  }}
  .header-landscape-android .hl-em + .hl-sub,
  .header-landscape-android .hl-sub + .hl-em {{ margin-left: 0.22em; }}
  .headline[dir="rtl"] .hl-em + .hl-sub,
  .headline[dir="rtl"] .hl-sub + .hl-em {{ margin-left: 0; margin-right: 0.24em; }}
  #__miu_root[dir="rtl"] math {{ direction: ltr; unicode-bidi: isolate; }}
  #__miu_root[dir="rtl"] .pele-diagram {{ direction: ltr; }}
  #__miu_root[dir="rtl"] ul, #__miu_root[dir="rtl"] ol {{ padding-right: 1.8em !important; padding-left: 0 !important; }}
  #__miu_root[dir="rtl"] input[type="checkbox"] {{ margin: 0 -1.4em 0.2em 0.5em !important; }}
  #__miu_root[dir="rtl"] .markdown-alert {{ border-left: none !important; border-right: 4px solid #2ea043 !important; }}
  .header-landscape-duo {{
    padding-top: 56px;
    font-size: 31px;
    line-height: 1.25;
  }}
  .header-landscape-duo .hl-em + .hl-sub,
  .header-landscape-duo .hl-sub + .hl-em {{ margin-left: 0.22em; }}

  .device-card {{
    display: flex;
    flex-direction: column;
    overflow: hidden;
  }}
  .device-card.dark {{
    background: #141416;
    color: #E6EDF3;
    border: 1px solid rgba(0, 0, 0, 0.22);
    border-bottom: none;
  }}
  .device-card.light {{
    background: #FFFFFF;
    color: #1F2328;
    border: 1px solid #D2D2D7;
    border-bottom: none;
  }}

  .iphone-card {{
    border-radius: 40px 40px 0 0;
  }}
  .android-phone-card {{
    border-radius: 32px 32px 0 0;
  }}
  .ipad-card {{
    width: 1164px;
    height: 786px;
    border-radius: 22px 22px 0 0;
  }}
  .android-tablet-card {{
    width: 1104px;
    height: 626px;
    border-radius: 20px 20px 0 0;
  }}
  .android-sb {{
    height: 32px;
    padding: 4px 22px 0 24px;
  }}
  .mac-card {{
    width: 1184px;
    height: 682px;
    border-radius: 12px 12px 0 0;
  }}

  /* iPhone Duo dual-screen book card (continuous flat top edge, flush 1px center seam) */
  .duo-book {{
    display: flex;
    flex-direction: row;
    align-items: stretch;
    border-radius: 24px 24px 0 0;
    overflow: hidden;
  }}
  .duo-book.dark {{
    background: #141416;
    border: 1px solid rgba(0, 0, 0, 0.24);
    border-bottom: none;
  }}
  .duo-book.light {{
    background: #FFFFFF;
    border: 1px solid #D2D2D7;
    border-bottom: none;
  }}
  .duo-half {{
    flex: 1;
    min-width: 0;
    display: flex;
    flex-direction: column;
    overflow: hidden;
  }}
  .duo-book.dark .duo-half {{
    background: #141416;
    color: #E6EDF3;
  }}
  .duo-book.light .duo-half {{
    background: #FFFFFF;
    color: #1F2328;
  }}
  .duo-hinge {{
    width: 1px;
    flex-shrink: 0;
    background: #E2E2E7;
  }}
  .duo-book.dark .duo-hinge {{
    background: #2C2C2E;
  }}

  .mac-titlebar {{
    height: 36px;
    flex-shrink: 0;
    display: flex;
    align-items: center;
    position: relative;
    padding: 0 16px;
  }}
  .dark .mac-titlebar {{
    background: #1C1C1E;
    border-bottom: 1px solid #2C2C2E;
  }}
  .light .mac-titlebar {{
    background: #F6F6F8;
    border-bottom: 1px solid #E5E5EA;
  }}
  .mac-dots {{
    display: flex;
    gap: 8px;
    align-items: center;
  }}
  .mac-dots.small {{ gap: 6px; }}
  .dot {{
    width: 12px;
    height: 12px;
    border-radius: 50%;
    display: inline-block;
  }}
  .mac-dots.small .dot {{ width: 10px; height: 10px; }}
  .dot.r {{ background: #FF5F57; }}
  .dot.y {{ background: #FEBC2E; }}
  .dot.g {{ background: #28C840; }}
  .dot.g.off {{ background: #D1D1D6; }}
  .mac-wintitle {{
    position: absolute;
    left: 0;
    right: 0;
    text-align: center;
    font-size: 13px;
    font-weight: 600;
    pointer-events: none;
  }}
  .dark .mac-wintitle {{ color: #AEAEB2; }}
  .light .mac-wintitle {{ color: #48484A; }}

  .ios-statusbar {{
    height: 38px;
    padding: 8px 24px 0 26px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    flex-shrink: 0;
    font-size: 13px;
    font-weight: 600;
  }}
  .ipad-sb {{
    height: 38px;
    padding: 0 22px;
    border-bottom: 1px solid rgba(128, 128, 136, 0.18);
  }}
  .duo-sb {{
    height: 34px;
    padding: 2px 18px 0 20px;
    font-size: 11.5px;
    border-bottom: 1px solid rgba(128, 128, 136, 0.18);
  }}
  .dark .ipad-sb, .dark .duo-sb {{ background: #1C1C1E; }}
  .light .ipad-sb, .light .duo-sb {{ background: #FAFAFC; }}
  .ios-status-right {{
    display: flex;
    align-items: center;
    gap: 6px;
  }}
  .ios-docbar {{
    height: 28px;
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 11.5px;
    font-weight: 600;
    flex-shrink: 0;
  }}
  .dark .ios-docbar {{
    color: #98989D;
    border-bottom: 1px solid #2C2C2E;
  }}
  .light .ios-docbar {{
    color: #636366;
    border-bottom: 1px solid #E5E5EA;
  }}
  .ipad-doc, .duo-doc {{
    font-size: 12px;
    font-weight: 600;
    opacity: 0.72;
  }}

  .card-content {{
    flex: 1;
    min-height: 0;
    position: relative;
    display: flex;
    flex-direction: column;
  }}
  .split-horizontal {{
    flex: 1;
    min-height: 0;
    display: flex;
    flex-direction: row;
  }}
  .pane-left, .pane-right {{
    flex: 1;
    min-width: 0;
    overflow: hidden;
  }}
  .h-splitter {{
    width: 1px;
    flex-shrink: 0;
  }}
  .dark .h-splitter {{ background: #2C2C2E; }}
  .light .h-splitter {{ background: #E5E5EA; }}

  .split-vertical {{
    flex: 1;
    min-height: 0;
    display: flex;
    flex-direction: column;
  }}
  .pane-top {{
    height: 45%;
    overflow: hidden;
  }}
  .pane-bot {{
    flex: 1;
    min-height: 0;
    overflow: hidden;
  }}
  .v-splitter {{
    height: 9px;
    flex-shrink: 0;
    display: flex;
    align-items: center;
    justify-content: center;
  }}
  .dark .v-splitter {{
    background: #1C1C1E;
    border-top: 1px solid #2C2C2E;
    border-bottom: 1px solid #2C2C2E;
  }}
  .light .v-splitter {{
    background: #F5F5F7;
    border-top: 1px solid #E5E5EA;
    border-bottom: 1px solid #E5E5EA;
  }}
  .v-grabber {{
    width: 34px;
    height: 3.5px;
    border-radius: 2px;
    background: rgba(142, 142, 147, 0.55);
  }}

  .single-pane, .duo-pane-inner {{
    flex: 1;
    min-height: 0;
    position: relative;
    overflow: hidden;
  }}

  .miu-editor {{
    padding: 12px 16px 12px 0;
    font-family: {code_font_stack};
    font-variant-ligatures: none;
    white-space: pre-wrap;
    overflow-wrap: anywhere;
  }}
  .ed-line {{
    display: flex;
    align-items: flex-start;
  }}
  .ed-ln {{
    width: 40px;
    padding-right: 12px;
    text-align: right;
    flex-shrink: 0;
    user-select: none;
    font-size: 0.90em;
  }}
  .dark .ed-ln {{ color: #636366; }}
  .light .ed-ln {{ color: #8E8E93; }}
  .ed-code {{
    flex: 1;
    min-width: 0;
    overflow: hidden;
  }}

  .dark .pane-right, .dark .pane-bot, .dark .right-half .duo-pane-inner {{
    background: #0D1117;
  }}
  .light .pane-right, .light .pane-bot, .light .right-half .duo-pane-inner {{
    background: #FFFFFF;
  }}
  #__miu_root {{
    font-family: {ui_font_stack};
    font-feature-settings: "palt" 1;
  }}
  #__miu_root .pele-diagram svg text {{
    font-family: {ui_font_stack} !important;
  }}
  .preview-mac {{
    padding: 20px 26px !important;
    font-size: 14.5px !important;
  }}
  .preview-pad {{
    padding: 20px 26px !important;
    font-size: 14.5px !important;
  }}
  .preview-duo {{
    padding: 14px 18px !important;
    font-size: 11.8px !important;
  }}
  .preview-Zoom-iphone {{
    padding: 9px 15px !important;
    font-size: 11.0px !important;
    line-height: 1.48 !important;
  }}
  .preview-Zoom-iphone h1 {{ font-size: 1.28em !important; margin-top: 0 !important; padding-bottom: 4px !important; margin-bottom: 7px !important; }}
  .preview-Zoom-iphone h2 {{ font-size: 1.08em !important; margin-top: 9px !important; padding-bottom: 3px !important; margin-bottom: 5px !important; }}
  .preview-Zoom-iphone table {{ font-size: 0.92em !important; margin: 6px 0 !important; width: 100%; }}
  .preview-Zoom-iphone th, .preview-Zoom-iphone td {{ padding: 3.5px 7px !important; }}
  .preview-Zoom-iphone th:first-child, .preview-Zoom-iphone td:first-child,
  .preview-Zoom-iphone th:last-child, .preview-Zoom-iphone td:last-child {{ white-space: nowrap; }}
  .preview-Zoom-iphone .markdown-alert {{ padding: 6px 10px !important; margin: 6px 0 !important; }}
  .preview-Zoom-iphone ul {{ margin: 4px 0 !important; }}
  .preview-Zoom-iphone li {{ margin: 2px 0 !important; }}
  .preview-Zoom-iphone .pele-diagram {{ padding: 8px !important; margin: 6px 0 !important; }}
  .preview-duo h1, .preview-mac h1, .preview-pad h1 {{ margin-top: 0 !important; }}
  .preview-duo th:first-child, .preview-duo td:first-child, .preview-duo th:last-child, .preview-duo td:last-child,
  .preview-mac th, .preview-mac td, .preview-pad th, .preview-pad td {{
    white-space: nowrap;
  }}
  #__miu_root .pele-diagram svg {{
    max-height: 360px;
    width: auto;
    max-width: 100%;
    display: block;
    margin: 6px auto;
  }}
  .preview-Zoom-iphone .pele-diagram svg {{
    max-height: 138px !important;
  }}
  .preview-Zoom-android .pele-diagram svg {{
    max-height: 118px !important;
  }}
  .preview-duo .pele-diagram svg {{
    max-height: 290px !important;
  }}

  mark.rx-hit {{
    background: rgba(255, 204, 0, 0.30);
    border-radius: 3px;
    padding: 1px 0;
    color: inherit;
  }}
  mark.rx-hit.rx-active {{
    background: rgba(0, 122, 255, 0.20);
    outline: 1.5px solid #007AFF;
  }}
  .mc-sel {{
    background: rgba(56, 139, 253, 0.32);
    border-radius: 2px;
    position: relative;
  }}
  .mc-caret {{
    display: inline-block;
    width: 2px;
    height: 1.15em;
    background: #58A6FF;
    vertical-align: text-bottom;
    margin-left: 0.5px;
  }}

  .ios-pillbar-wrap {{
    height: 48px;
    flex-shrink: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    pointer-events: none;
  }}
  .dark .ios-pillbar-wrap {{
    background: #141416;
    border-top: 1px solid #242428;
  }}
  .light .ios-pillbar-wrap {{
    background: #FFFFFF;
    border-top: 1px solid #ECECF0;
  }}
  .ipad-card .ios-pillbar-wrap, .android-tablet-card .ios-pillbar-wrap {{
    position: absolute;
    left: 0;
    right: 0;
    bottom: 14px;
    height: auto;
    background: transparent !important;
    border-top: none !important;
  }}
  .ios-pillbar {{
    display: flex;
    align-items: center;
    gap: 8px;
    height: 38px;
    padding: 0 10px;
    border-radius: 19px;
    white-space: nowrap;
  }}
  .ios-pillbar.compact {{
    height: 34px;
    padding: 0 8px;
    gap: 5px;
    border-radius: 17px;
  }}
  .dark .ios-pillbar {{
    background: #222226;
    border: 1px solid #38383D;
    color: #F2F2F7;
  }}
  .light .ios-pillbar {{
    background: #F7F7FA;
    border: 1px solid #D8D8DF;
    color: #1D1D1F;
  }}
  .pill-group, .pill-actions {{
    display: flex;
    align-items: center;
    gap: 4px;
  }}
  .pill-item {{
    display: flex;
    align-items: center;
    gap: 3px;
    padding: 4px 9px;
    border-radius: 12px;
    font-size: 11.5px;
    font-weight: 600;
    white-space: nowrap;
  }}
  .compact .pill-item {{
    padding: 3px 7px;
    font-size: 10.5px;
  }}
  .dark .pill-item.active {{
    background: rgba(10, 132, 255, 0.22);
    color: #409CFF;
  }}
  .light .pill-item.active {{
    background: rgba(0, 122, 255, 0.12);
    color: #007AFF;
  }}
  .pill-sep {{
    width: 1px;
    height: 16px;
    background: rgba(142, 142, 147, 0.35);
  }}
  .pill-icon {{
    width: 26px;
    height: 26px;
    border-radius: 13px;
    display: flex;
    align-items: center;
    justify-content: center;
  }}

  .duo-status-foot {{
    height: 32px;
    padding: 0 18px;
    display: flex;
    align-items: center;
    font-size: 10.5px;
    font-weight: 500;
    opacity: 0.65;
    border-top: 1px solid rgba(128, 128, 136, 0.18);
    white-space: nowrap;
  }}

  .mac-replace-sheet {{
    position: absolute;
    top: 18px;
    right: 22px;
    min-width: 396px;
    max-width: 510px;
    width: max-content;
    background: #F9F9FB;
    border: 1px solid #D2D2D7;
    border-radius: 10px;
    overflow: hidden;
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.08);
    color: #1D1D1F;
  }}
  .mac-sheet-tb {{
    height: 28px;
    background: #EFEFF4;
    border-bottom: 1px solid #DFDFE6;
    display: flex;
    align-items: center;
    padding: 0 12px;
    position: relative;
  }}
  .mac-sheet-title {{
    position: absolute;
    left: 0; right: 0;
    text-align: center;
    font-size: 12px;
    font-weight: 600;
    color: #3A3A3C;
  }}
  .mac-sheet-body {{
    padding: 12px 14px 14px;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }}
  .mac-fields-grid {{
    display: grid;
    grid-template-columns: max-content minmax(256px, 1fr);
    gap: 8px 10px;
    align-items: center;
  }}
  .mac-fields-grid label {{
    font-size: 12px;
    font-weight: 500;
    color: #48484A;
    white-space: nowrap;
  }}
  .mac-input {{
    background: #FFFFFF;
    border: 1px solid #C7C7CC;
    border-radius: 6px;
    padding: 5px 9px;
    font-family: {code_font_stack};
    font-size: 12px;
    color: #1D1D1F;
    white-space: nowrap;
  }}
  .mac-input.focus {{
    border: 1.5px solid #007AFF;
  }}
  .mac-checks {{
    display: flex;
    gap: 16px;
    padding-top: 2px;
    font-size: 11.5px;
    color: #48484A;
    white-space: nowrap;
  }}
  .chk {{ display: flex; align-items: center; gap: 5px; white-space: nowrap; }}
  .chk.on {{ color: #007AFF; font-weight: 600; }}
  .chk .box {{
    width: 13px; height: 13px;
    border-radius: 3px;
    border: 1px solid #AEAEB2;
    background: #FFFFFF;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    font-size: 10px;
    color: #FFFFFF;
    flex-shrink: 0;
  }}
  .chk .box.checked {{
    background: #007AFF;
    border-color: #007AFF;
  }}
  .mac-btns {{
    display: flex;
    justify-content: flex-end;
    gap: 8px;
    padding-top: 4px;
    white-space: nowrap;
  }}
  .mbtn {{
    padding: 4px 12px;
    border-radius: 6px;
    font-size: 11.5px;
    font-weight: 600;
    background: #FFFFFF;
    border: 1px solid #C7C7CC;
    color: #1D1D1F;
    white-space: nowrap;
  }}
  .mbtn.primary {{
    background: #007AFF;
    border-color: #007AFF;
    color: #FFFFFF;
  }}

  .iphone-card .single-pane, .android-phone-card .single-pane {{
    display: flex;
    flex-direction: column;
  }}
  .iphone-card .single-pane .miu-editor, .android-phone-card .single-pane .miu-editor {{
    flex: 1;
    min-height: 0;
    overflow: hidden;
  }}
  .ios-find-bar {{
    position: absolute;
    left: 10px;
    right: 10px;
    bottom: 12px;
    background: #F7F7FA;
    border: 1px solid #D2D2D7;
    border-radius: 14px;
    padding: 9px 10px;
    display: flex;
    flex-direction: column;
    gap: 6px;
    color: #1D1D1F;
  }}
  .iphone-card .single-pane .ios-find-bar, .android-phone-card .single-pane .ios-find-bar {{
    position: relative;
    left: auto;
    right: auto;
    bottom: auto;
    margin: 4px 10px 10px;
    flex-shrink: 0;
  }}
  .ipad-find-wrap .ios-find-bar {{
    width: 540px;
    left: auto;
    right: 20px;
    bottom: 64px;
  }}
  .duo-pane-inner .ios-find-bar {{
    bottom: 14px;
  }}
  .ifb-row {{
    display: flex;
    align-items: center;
    gap: 8px;
  }}
  .ifb-input {{
    flex: 1;
    min-width: 0;
    background: #FFFFFF;
    border: 1px solid #C7C7CC;
    border-radius: 7px;
    padding: 4px 8px;
    font-family: {code_font_stack};
    font-size: 11px;
    color: #1D1D1F;
    white-space: nowrap;
    overflow: hidden;
  }}
  .ifb-input.focus {{
    border: 1.5px solid #007AFF;
  }}
  .ifb-nav {{
    display: flex;
    align-items: center;
    gap: 8px;
    font-size: 11.5px;
    font-weight: 600;
    color: #007AFF;
    flex-shrink: 0;
    white-space: nowrap;
  }}
  .ifb-pills {{
    display: flex;
    gap: 6px;
    white-space: nowrap;
  }}
  .ifb-opt {{
    padding: 2px 8px;
    border-radius: 6px;
    font-size: 10.5px;
    font-weight: 600;
    background: #FFFFFF;
    border: 1px solid #D2D2D7;
    color: #48484A;
    white-space: nowrap;
  }}
  .ifb-opt.selected {{
    background: #007AFF;
    border-color: #007AFF;
    color: #FFFFFF;
  }}
</style>
</head>
<body data-page-token="{_esc(f'{prefix}_{display_type}_{slide_idx}')}">
  <div class="stage">
    <div class="headline {header_cls}"{hl_dir_attr}>{headline_html}</div>
    {device_html}
  </div>
</body>
</html>
"""
    return html, vw, vh, dpr


def _find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def render_minimal_batch(
    prefixes: list[str],
    display_types: list[str],
    out_dir: str | Path,
    ui_strings_by_prefix: dict[str, dict],
    duo_target_size: tuple[int, int] = (2853, 2007),
    workers: int = 4,
) -> dict[tuple[str, str], list[str]]:
    """
    Render all 5 slides for every (prefix, display_type) in parallel using
    real C++ MiuSyntax/MiuPreview + multi-tab Headless Chromium/Edge CDP.
    Returns {(prefix, display_type): [path_1, ..., path_5]}.
    """
    out_path = Path(out_dir)
    out_path.mkdir(parents=True, exist_ok=True)
    work_dir = Path(tempfile.gettempdir()) / "miu_store_minimal_work"
    work_dir.mkdir(parents=True, exist_ok=True)
    html_dir = work_dir / "html_pages"
    html_dir.mkdir(parents=True, exist_ok=True)

    engine_data = build_and_run_engine_exporter(prefixes, work_dir)
    browser_exe = find_headless_browser()
    port = _find_free_port()
    profile_dir = work_dir / f"browser_profile_{port}"
    profile_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        browser_exe,
        "--headless=new",
        f"--remote-debugging-port={port}",
        "--remote-allow-origins=*",
        f"--user-data-dir={profile_dir}",
        "--disable-gpu",
        "--hide-scrollbars",
        "--no-first-run",
        "--no-default-browser-check",
        "--disable-dev-shm-usage",
        "--allow-file-access-from-files",
    ]
    if os.name != "nt":
        cmd.extend(["--no-sandbox", "--disable-setuid-sandbox"])
    cmd.append("about:blank")

    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        root_ws = None
        for _ in range(60):
            time.sleep(0.15)
            try:
                raw = urllib.request.urlopen(f"http://127.0.0.1:{port}/json/list", timeout=2).read()
                pages = json.loads(raw.decode("utf-8"))
                for p in pages:
                    if p.get("type") == "page" and p.get("webSocketDebuggerUrl"):
                        root_ws = p["webSocketDebuggerUrl"]
                        break
                if root_ws:
                    break
            except Exception:
                pass
        if not root_ws:
            raise RuntimeError(f"Failed to connect to headless browser ({browser_exe}) on port {port}")

        # Build job list: each job is (prefix, display_type) -> 5 slides
        jobs = [(pfx, dt) for pfx in prefixes for dt in display_types]
        n_workers = max(1, min(workers, len(jobs)))

        # Create n_workers page targets so each worker thread owns a dedicated CDP WebSocket
        worker_ws_urls = [root_ws]
        if n_workers > 1:
            ctrl = SimpleCDP(root_ws)
            try:
                for _ in range(n_workers - 1):
                    res = ctrl.call("Target.createTarget", {"url": "about:blank"})
                    tid = res["targetId"]
                    worker_ws_urls.append(f"ws://127.0.0.1:{port}/devtools/page/{tid}")
            finally:
                ctrl.close()

        # Partition jobs evenly across workers
        buckets = [[] for _ in range(n_workers)]
        for idx, job in enumerate(jobs):
            buckets[idx % n_workers].append(job)

        def _run_bucket(ws_url: str, bucket_jobs: list[tuple[str, str]]) -> dict[tuple[str, str], list[str]]:
            local_res = {}
            cdp = SimpleCDP(ws_url)
            try:
                cdp.call("Page.enable")
                last_metrics = None
                for pfx, dt in bucket_jobs:
                    ui_strings = ui_strings_by_prefix.get(pfx, {})
                    slide_paths = []
                    for s_idx, s_key, doc_title, mode in SLIDE_SPEC_ORDER:
                        page_html, vw, vh, dpr = build_slide_html(
                            dt, pfx, s_idx, s_key, doc_title, mode, ui_strings, engine_data, duo_target_size
                        )
                        tmp_html = html_dir / f"p_{pfx}_{dt}_{s_idx}.html"
                        tmp_html.write_text(page_html, encoding="utf-8")

                        cur_metrics = (vw, vh, dpr)
                        if cur_metrics != last_metrics:
                            cdp.call(
                                "Emulation.setDeviceMetricsOverride",
                                {"width": vw, "height": vh, "deviceScaleFactor": dpr, "mobile": False},
                            )
                            last_metrics = cur_metrics

                        cdp.call("Page.navigate", {"url": tmp_html.as_uri()})
                        expected_tok = f"{pfx}_{dt}_{s_idx}"
                        ready_js = f"""
                        (() => {{
                          if (!document.body || document.body.dataset.pageToken !== {json.dumps(expected_tok)} || document.readyState !== 'complete') {{
                            return false;
                          }}
                          return document.fonts.ready.then(() => {{
                            void document.body.offsetHeight;
                            {JS_AUTO_FIT}
                            void document.body.offsetHeight;
                            return true;
                          }});
                        }})()
                        """
                        for _ in range(80):
                            try:
                                ev = cdp.call("Runtime.evaluate", {"expression": ready_js, "awaitPromise": True, "returnByValue": True})
                                if ev.get("result", {}).get("value") is True:
                                    break
                            except RuntimeError:
                                pass
                            time.sleep(0.025)
                        else:
                            raise RuntimeError(f"Timed out waiting for page render: {expected_tok}")

                        shot = cdp.call(
                            "Page.captureScreenshot",
                            {
                                "format": "png",
                                "captureBeyondViewport": False,
                                "clip": {"x": 0, "y": 0, "width": vw, "height": vh, "scale": 1},
                            },
                        )
                        png_bytes = base64.b64decode(shot["data"])
                        fname = f"miu_{pfx}_{dt}_{s_idx}.png"
                        fpath = out_path / fname

                        # Apple App Store Connect strictly forbids PNG alpha channels (RGBA -> 24-bit RGB)
                        with Image.open(io.BytesIO(png_bytes)) as im:
                            exp_size = (vw * dpr, vh * dpr)
                            if im.size != exp_size:
                                raise RuntimeError(f"Dimension mismatch on {fname}: got {im.size}, expected {exp_size}")
                            rgb = im.convert("RGB")
                            rgb.save(fpath, format="PNG", optimize=False)
                        slide_paths.append(str(fpath))
                    local_res[(pfx, dt)] = slide_paths
            finally:
                cdp.close()
            return local_res

        results: dict[tuple[str, str], list[str]] = {}
        with ThreadPoolExecutor(max_workers=n_workers) as pool:
            futs = [pool.submit(_run_bucket, worker_ws_urls[i], buckets[i]) for i in range(n_workers) if buckets[i]]
            for fut in as_completed(futs):
                results.update(fut.result())

        return results
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except Exception:
            proc.kill()
        shutil.rmtree(profile_dir, ignore_errors=True)

#!/usr/bin/env python3
"""Windows screenshot compositor and resumable, explicitly scoped ASC publisher.

Only MAC_OS / APP_DESKTOP is supported. Source pixels are never recolored.
Browser typography supplies shaping for Indic/Thai scripts on Windows.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import html
import io
import json
import os
from pathlib import Path
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

import requests
from PIL import Image
import numpy as np

import appstore_screenshots_sync as legacy

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT / 'build_screenshots_v2'
BASE = legacy.BASE_URL
DISPLAY = 'APP_DESKTOP'
EDITABLE = {'PREPARE_FOR_SUBMISSION', 'DEVELOPER_REJECTED', 'REJECTED', 'METADATA_REJECTED', 'INVALID_BINARY'}
FEATURES = ['Markdown', 'Mermaid', 'LaTeX', 'Regex', 'Multi-cursor']
SOURCE_MAP = Path(__file__).with_name('store_screenshots_sources.json')


def save_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix('.tmp')
    temp.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')
    temp.replace(path)


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def checksum(path, kind='sha256'):
    return hashlib.new(kind, Path(path).read_bytes()).hexdigest()


def source_image(path):
    """Remove only fully black, uniform outer padding, never inner pixels."""
    im = Image.open(path).convert('RGB')
    arr = np.asarray(im)
    left, top, right, bottom = 0, 0, im.width, im.height
    for _ in range(12):
        if np.all(arr[top:bottom, left] == 0): left += 1
        else: break
    for _ in range(12):
        if np.all(arr[top:bottom, right - 1] == 0): right -= 1
        else: break
    for _ in range(12):
        if np.all(arr[top, left:right] == 0): top += 1
        else: break
    for _ in range(12):
        if np.all(arr[bottom - 1, left:right] == 0): bottom -= 1
        else: break
    box = (left, top, right, bottom)
    stream = io.BytesIO()
    im.crop(box).save(stream, 'PNG')
    return 'data:image/png;base64,' + base64.b64encode(stream.getvalue()).decode(), box


def make_html(prefix, spec):
    uri, crop = source_image(spec['panel_path'])
    dark = spec['is_dark']
    bg = '#111827' if dark else '#edf1f7'
    fg = '#f4f7fb' if dark else '#152438'
    muted = '#94a5bc' if dark else '#53657a'
    accent = '#9bbcf5' if dark else '#3b5e94'
    bar = '#272d39' if dark else '#f6f7f9'
    border = '#414958' if dark else '#cbd2dc'
    families = {'ja':'Yu Gothic', 'zh_cn':'Microsoft YaHei', 'zh_tw':'Microsoft JhengHei',
                'ko':'Malgun Gothic', 'hi':'Nirmala UI', 'th':'Leelawadee UI'}
    family = families.get(prefix, 'Segoe UI')
    joiner = '' if prefix in ('ja', 'zh_cn', 'zh_tw') else ' '
    title = joiner.join(spec['title'])
    lang = {'zh_cn':'zh-Hans', 'zh_tw':'zh-Hant', 'pt_br':'pt-BR', 'nb':'no'}.get(prefix,prefix)
    content = f'''<!doctype html><html lang="{lang}"><meta charset="utf-8">
<style>
* {{box-sizing:border-box}} body {{margin:0;width:1440px;height:900px;overflow:hidden;
background:{bg};color:{fg};font-family:"{family}","Segoe UI",sans-serif}}
.brand {{position:absolute;left:64px;top:35px;font-size:24px;font-weight:650;letter-spacing:-.4px}}
.feature {{position:absolute;right:64px;top:39px;color:{muted};font-size:15px;letter-spacing:1px}}
.heading {{position:absolute;left:100px;top:83px;width:1240px;height:106px;display:flex;
align-items:center;justify-content:center;text-align:center}}
h1 {{margin:0;font-size:44px;line-height:1.2;font-weight:650;letter-spacing:-.7px;overflow-wrap:anywhere}}
.window {{position:absolute;top:211px;left:220px;width:1000px;border:1px solid {border};
border-radius:11px;overflow:hidden;background:{bar};box-shadow:0 20px 42px #00000038}}
.titlebar {{height:32px;display:flex;align-items:center;justify-content:center;
background:{bar};color:{muted};font:12px "Segoe UI",sans-serif;position:relative}}
.controls {{position:absolute;left:13px;top:10px;display:flex;gap:7px}}
.controls span {{width:11px;height:11px;border-radius:50%;box-shadow:inset 0 0 0 1px #00000015}}
img {{width:100%;height:auto;display:block}}
.brand span {{color:{accent}}}
</style><div class="brand">miu<span> /</span></div>
<div class="feature">{spec['idx']:02d} / 05 &nbsp; {FEATURES[spec['idx']-1]}</div>
<div class="heading"><h1>{html.escape(title)}</h1></div>
<div class="window"><div class="titlebar"><div class="controls">
<span style="background:#ff6058"></span><span style="background:#febc2e"></span>
<span style="background:#28c840"></span></div>{html.escape(spec['doc_title'])}</div>
<img src="{uri}"></div></html>'''
    return content, crop


def build(args):
    from playwright.sync_api import sync_playwright
    out = args.out_dir
    out.mkdir(parents=True, exist_ok=True)
    prefixes = sorted(legacy.LANG_DECKS) if args.all_locales else args.prefixes.split(',')
    source_map = read_json(SOURCE_MAP)
    manifest = {'schema': 1, 'platform': 'MAC_OS', 'display_type': DISPLAY,
                'bundle_id': legacy.BUNDLE_ID, 'size': [2880,1800],
                'locale_map': legacy.ASC_LOCALE_TO_PREFIX, 'images': {}, 'notes': [
                    'Existing source pixels retained; synthetic outer titlebar, not a new macOS capture.',
                    'Mermaid/Math use English document samples outside ja/en/zh/ko.',
                    'Regex/Multi-cursor use English document samples outside ja/en.',
                    'Markdown uses English except ja: other old sources have blank previews or Windows-only copy.',
                    'Region variants share their declared language deck; no implicit locale fallback.',
                    'Low resolution source assets are proportionally resampled, not detail-enhanced.'
                ]}
    with sync_playwright() as p:
        browser = p.chromium.launch(channel='msedge', headless=True)
        page = browser.new_page(viewport={'width':1440,'height':900}, device_scale_factor=2)
        for prefix in prefixes:
            rows = []
            for spec in legacy.get_slide_specs(prefix):
                spec['panel_path'] = str(Path(legacy.ASSETS_DIR) / source_map[prefix][spec['idx']-1])
                document, crop = make_html(prefix, spec)
                page.set_content(document)
                page.evaluate('document.fonts.ready')
                page.locator('img').evaluate('(im) => im.decode()')
                metrics = page.evaluate('''() => {
                    const title=document.querySelector('h1'), container=document.querySelector('.heading');
                    let size=44;
                    while(title.getBoundingClientRect().height>container.clientHeight && size>30) {
                      title.style.fontSize=(--size)+'px';
                    }
                    const t=title.getBoundingClientRect(), w=document.querySelector('.window').getBoundingClientRect();
                    return {fontSize:size,titleHeight:t.height,titleWidth:t.width,titleBottom:t.bottom,
                            windowTop:w.top,windowBottom:w.bottom,windowWidth:w.width};
                }''')
                if metrics['titleHeight'] > 106 or metrics['titleWidth'] > 1241 or metrics['windowBottom'] > 878:
                    raise RuntimeError(f'Layout overflow: {prefix}/{spec["idx"]}: {metrics}')
                filename = f'{prefix}_{spec["idx"]:02d}.png'
                target = out / filename
                page.screenshot(path=str(target))
                with Image.open(target) as im:
                    im.convert('RGB').save(target, 'PNG')
                source = Path(spec['panel_path'])
                source_lang = source.stem.split('_',1)[1]
                rows.append({'file':filename, 'sha256':checksum(target), 'md5':checksum(target,'md5'),
                             'source':str(source.relative_to(ROOT)).replace('\\','/'),
                             'source_sha256':checksum(source), 'source_language':source_lang,
                             'crop':crop, 'layout':metrics})
            manifest['images'][prefix] = rows
            print(f'Built {prefix}: 5 images', flush=True)
        browser.close()
    save_json(out / 'manifest.json', manifest)
    write_gallery(out, manifest)
    validate(out)


def write_gallery(out, manifest):
    parts = ['<!doctype html><meta charset="utf-8"><title>miu screenshot review</title>',
             '<style>body{background:#18202d;color:white;font:16px Segoe UI;margin:30px}',
             'section{display:flex;gap:12px}img{width:100%;border-radius:5px}a{width:20%;color:#bdd6ff}</style>',
             '<h1>miu · macOS · screenshot review</h1>',
             '<p>2880 × 1800 / RGB PNG. Click an image for full resolution.</p>']
    for prefix, rows in manifest['images'].items():
        parts.append(f'<h2>{prefix}</h2><section>')
        for row in rows:
            name = row['file']
            parts.append(f'<a href="{name}"><img src="{name}">{name}<br>source: {row["source_language"]}</a>')
        parts.append('</section>')
    (out/'review.html').write_text('\n'.join(parts),encoding='utf-8')
    # Compact visual review boards, one per feature, covering every language.
    for idx in range(5):
        prefixes = list(manifest['images'])
        cols = 4
        board = Image.new('RGB',(cols*576, ((len(prefixes)+cols-1)//cols)*386),'#18202d')
        from PIL import ImageDraw, ImageFont
        draw = ImageDraw.Draw(board)
        font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',18)
        for n,prefix in enumerate(prefixes):
            x,y=(n%cols)*576,(n//cols)*386
            with Image.open(out/manifest['images'][prefix][idx]['file']) as im:
                board.paste(im.resize((576,360),Image.Resampling.LANCZOS),(x,y+26))
            draw.text((x+8,y+2),prefix,font=font,fill='white')
        board.save(out/f'contact_{idx+1:02d}.jpg',quality=90)


def validate(out):
    m = read_json(out/'manifest.json')
    if m['platform'] != 'MAC_OS' or m['display_type'] != DISPLAY:
        raise RuntimeError('Unexpected manifest scope')
    count = 0
    for prefix,rows in m['images'].items():
        if len(rows)!=5: raise RuntimeError(f'Expected 5 images: {prefix}')
        for row in rows:
            path = out/row['file']
            with Image.open(path) as im:
                im.load()
                if im.size!=(2880,1800) or im.mode!='RGB' or im.format!='PNG':
                    raise RuntimeError(f'Invalid image: {path}')
            if checksum(path)!=row['sha256'] or checksum(path,'md5')!=row['md5']:
                raise RuntimeError(f'Image changed since build: {path}')
            count += 1
    print(f'Validated {count} RGB PNG images',flush=True)
    return m


def configure_auth(args):
    if args.key_file:
        key = args.key_file
        os.environ['APPSTORE_PRIVATE_KEY'] = key.read_text(encoding='utf-8').strip()
        os.environ['APPSTORE_KEY_ID'] = args.key_id or key.stem.removeprefix('AuthKey_')
    if args.issuer_id:
        os.environ['APPSTORE_ISSUER_ID'] = args.issuer_id
    if args.key_id:
        os.environ['APPSTORE_KEY_ID'] = args.key_id
    legacy.generate_token()  # fail before any network mutation


def api(method, path, payload=None):
    url = path if path.startswith('https://') else BASE+'/'+path
    r = legacy.api_request(method,url,json_data=payload)
    if not r.ok:
        # API errors only: never log authorization or signed upload URLs.
        try: detail = [(e.get('code'),e.get('detail')) for e in r.json().get('errors',[])]
        except ValueError: detail = 'Non-JSON response'
        raise RuntimeError(f'{method} {path.split("?")[0]}: HTTP {r.status_code}: {detail}')
    return r.json() if r.content else {}


def listing(path):
    result = []
    while path:
        response = api('GET',path)
        result.extend(response['data'])
        path = response.get('links',{}).get('next')
    return result


def make_plan(args, manifest):
    apps = listing(f'apps?filter[bundleId]={legacy.BUNDLE_ID}&limit=200')
    if len(apps)!=1: raise RuntimeError('Expected exactly one matching bundle ID')
    app_id = apps[0]['id']
    versions = listing(f'apps/{app_id}/appStoreVersions?limit=50')
    versions = [v for v in versions if v['attributes']['versionString']==args.version
                and v['attributes']['platform']=='MAC_OS']
    if len(versions)!=1: raise RuntimeError(f'Expected exactly one MAC_OS {args.version}')
    version = versions[0]
    state = version['attributes']['appStoreState']
    if state not in EDITABLE: raise RuntimeError(f'Target version is not editable: {state}')
    targets = []
    for loc in listing(f'appStoreVersions/{version["id"]}/appStoreVersionLocalizations?limit=200'):
        locale = loc['attributes']['locale']
        prefix = manifest['locale_map'].get(locale)
        if prefix not in manifest['images']:
            raise RuntimeError(f'No explicit generated assets for locale {locale}')
        sets = listing(f'appStoreVersionLocalizations/{loc["id"]}/appScreenshotSets?limit=50')
        matches = [s for s in sets if s['attributes']['screenshotDisplayType']==DISPLAY]
        if len(matches)>1: raise RuntimeError(f'Duplicate desktop sets for {locale}')
        set_id = matches[0]['id'] if matches else None
        current = listing(f'appScreenshotSets/{set_id}/appScreenshots?limit=50') if set_id else []
        targets.append({'locale':locale,'prefix':prefix,'localization_id':loc['id'],
                        'set_id':set_id,'current':current,'desired':manifest['images'][prefix]})
    if not targets: raise RuntimeError('No localizations found')
    plan = {'app_id':app_id,'bundle_id':legacy.BUNDLE_ID,'platform':'MAC_OS','version':args.version,
            'version_id':version['id'],'state':state,'targets':targets}
    save_json(args.out_dir/'plan.json',plan)
    print(f'Plan: MAC_OS {args.version} ({state}), {len(targets)} locales / {len(targets)*5} images',flush=True)
    return plan


def backup(args, plan):
    dest = args.out_dir/'backups'/time.strftime('%Y%m%d-%H%M%S')
    dest.mkdir(parents=True,exist_ok=False)
    for target in plan['targets']:
        for n,ss in enumerate(target['current']):
            attrs = ss['attributes']
            asset = attrs.get('imageAsset')
            if not asset: raise RuntimeError(f'Cannot back up screenshot {ss["id"]}: no image asset')
            url = asset['templateUrl'].replace('{w}',str(asset['width'])).replace('{h}',str(asset['height'])).replace('{f}','png')
            r = requests.get(url,timeout=60)
            r.raise_for_status()
            file = dest/f'{target["locale"]}_{n+1:02d}_{ss["id"]}.png'
            with Image.open(io.BytesIO(r.content)) as im:
                if im.size!=(asset['width'],asset['height']): raise RuntimeError('Backup dimensions differ')
                im.convert('RGB').save(file,'PNG')
            ss['backup_file'] = file.name
            ss['backup_sha256'] = checksum(file)
        print(f'Backed up {target["locale"]}: {len(target["current"])}',flush=True)
    save_json(dest/'backup.json',plan)
    print(f'Backup complete: {dest}',flush=True)
    return dest


def wait_complete(ids, timeout=300):
    deadline=time.monotonic()+timeout
    pending=set(ids)
    while pending and time.monotonic()<deadline:
        for sid in list(pending):
            attrs=api('GET',f'appScreenshots/{sid}')['data']['attributes']
            state=attrs.get('assetDeliveryState',{}).get('state')
            if state=='COMPLETE': pending.remove(sid)
            elif state=='FAILED': raise RuntimeError(f'Screenshot processing failed: {sid}')
        if pending: time.sleep(3)
    if pending: raise RuntimeError(f'Screenshot processing timed out: {len(pending)}')


def ordered(set_id, ids):
    api('PATCH',f'appScreenshotSets/{set_id}/relationships/appScreenshots',
        {'data':[{'type':'appScreenshots','id':sid} for sid in ids]})
    actual=listing(f'appScreenshotSets/{set_id}/appScreenshots?limit=50')
    if [s['id'] for s in actual]!=ids: raise RuntimeError('Remote order verification failed')


def sync_target(args, target):
    locale=target['locale']
    journal=args.out_dir/'state'/f'{args.version}_{locale}.json'
    set_id=target['set_id']
    if not set_id:
        obj=api('POST','appScreenshotSets',{'data':{'type':'appScreenshotSets',
            'attributes':{'screenshotDisplayType':DISPLAY},'relationships':{'appStoreVersionLocalization':
            {'data':{'type':'appStoreVersionLocalizations','id':target['localization_id']}}}}})
        set_id=obj['data']['id']
    current=listing(f'appScreenshotSets/{set_id}/appScreenshots?limit=50')
    if [ss['id'] for ss in current] != [ss['id'] for ss in target['current']]:
        raise RuntimeError(f'{locale}: Remote set changed after backup; rerun to take a fresh backup')
    wanted=target['desired']
    # Reuse matching remote assets after interrupted runs, based on the actual file checksum.
    matching={}
    for ss in current:
        attr=ss['attributes']
        for row in wanted:
            if attr.get('sourceFileChecksum')==row['md5'] and attr.get('assetDeliveryState',{}).get('state')!='FAILED':
                matching.setdefault(row['md5'],ss['id'])
    keep=set(matching.values())
    stale=[ss['id'] for ss in current if ss['id'] not in keep]
    needed=len(wanted)-len(matching)
    # Prefer upload-before-delete (5 old + 5 new fits Apple's ten-slot limit).
    # If resuming a full set, remove only enough already-backed-up stale slots.
    for sid in stale[:max(0,len(current)+needed-10)]:
        api('DELETE',f'appScreenshots/{sid}')
        stale.remove(sid)
    ids=[]
    for row in wanted:
        sid=matching.get(row['md5'])
        if not sid:
            ok,sid=legacy.upload_single_screenshot(set_id,str(args.out_dir/row['file']))
            if not ok: raise RuntimeError(f'{locale}: {sid}')
        ids.append(sid)
        save_json(journal,{'status':'uploading','set_id':set_id,'uploaded_ids':ids,
                           'desired_sha256':[r['sha256'] for r in wanted]})
    wait_complete(ids)
    for sid in stale: api('DELETE',f'appScreenshots/{sid}')
    ordered(set_id,ids)
    actual=listing(f'appScreenshotSets/{set_id}/appScreenshots?limit=50')
    if [s['attributes'].get('sourceFileChecksum') for s in actual]!=[r['md5'] for r in wanted]:
        raise RuntimeError(f'{locale}: Remote checksums differ')
    save_json(journal,{'status':'complete','set_id':set_id,'uploaded_ids':ids,
                       'desired_sha256':[r['sha256'] for r in wanted]})
    return locale


def sync(args, manifest):
    plan=make_plan(args,manifest)
    dest=backup(args,plan)  # all backups must finish before the first mutation
    results=[]
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures={pool.submit(sync_target,args,t):t['locale'] for t in plan['targets']}
        for future in as_completed(futures):
            locale=futures[future]
            try:
                future.result()
                results.append({'locale':locale,'status':'complete'})
                print(f'Verified {locale}: 5 screenshots',flush=True)
            except Exception as e:
                results.append({'locale':locale,'status':'failed','error':str(e)})
                print(f'FAILED {locale}: {e}',flush=True)
            save_json(args.out_dir/'sync_report.json',{'version':args.version,'platform':'MAC_OS',
                      'backup':str(dest),'results':results})
    if any(r['status']!='complete' for r in results): raise RuntimeError('Some locales failed; rerun sync to resume')


def restore(args):
    plan=read_json(args.backup/'backup.json')
    if plan['version']!=args.version or plan['platform']!='MAC_OS' or plan['bundle_id']!=legacy.BUNDLE_ID:
        raise RuntimeError('Backup scope mismatch')
    state=api('GET',f'appStoreVersions/{plan["version_id"]}')['data']['attributes']
    if state['appStoreState'] not in EDITABLE: raise RuntimeError('Version is not editable')
    # Validate every backup before changing any remote set.
    for target in plan['targets']:
        for ss in target['current']:
            if checksum(args.backup/ss['backup_file'])!=ss['backup_sha256']:
                raise RuntimeError('Backup checksum mismatch')
    for target in plan['targets']:
        sets=listing(f'appStoreVersionLocalizations/{target["localization_id"]}/appScreenshotSets?limit=50')
        found=[s for s in sets if s['attributes']['screenshotDisplayType']==DISPLAY]
        if not found:
            if target['current']: raise RuntimeError('Original set no longer exists')
            continue
        sid=found[0]['id']
        for ss in listing(f'appScreenshotSets/{sid}/appScreenshots?limit=50'):
            api('DELETE',f'appScreenshots/{ss["id"]}')
        ids=[]
        for ss in target['current']:
            ok,result=legacy.upload_single_screenshot(sid,str(args.backup/ss['backup_file']))
            if not ok: raise RuntimeError(result)
            ids.append(result)
        wait_complete(ids)
        ordered(sid,ids)
        print(f'Restored {target["locale"]}',flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=['build','validate','plan','sync','restore'])
    parser.add_argument('--out-dir',type=Path,default=DEFAULT_OUT)
    parser.add_argument('--all-locales',action='store_true')
    parser.add_argument('--prefixes',default='ja,en')
    parser.add_argument('--version',default='1.0.44')
    parser.add_argument('--platform',choices=['MAC_OS'],default='MAC_OS')
    parser.add_argument('--key-file',type=Path)
    parser.add_argument('--key-id')
    parser.add_argument('--issuer-id')
    parser.add_argument('--workers',type=int,choices=range(1,5),default=2)
    parser.add_argument('--backup',type=Path)
    args=parser.parse_args()
    if args.command=='build': return build(args)
    if args.command=='validate': return validate(args.out_dir)
    configure_auth(args)
    if args.command=='restore':
        if not args.backup: parser.error('--backup is required for restore')
        return restore(args)
    manifest=validate(args.out_dir)
    if args.command=='plan': return make_plan(args,manifest)
    return sync(args,manifest)


if __name__=='__main__':
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    try: main()
    except Exception as error:
        print(f'ERROR: {error}',file=sys.stderr)
        sys.exit(1)

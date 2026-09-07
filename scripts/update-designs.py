#!/usr/bin/env python3
"""Refresh the local design review from native 0.3.1 UI captures."""
from pathlib import Path
import base64
import shutil

root = Path(__file__).resolve().parents[1]
design = root / 'Design'
release = design / 'Release-0.3.1'
archive = design / 'Archive-before-0.3.1'
archive.mkdir(exist_ok=True)
shots = release / 'Screenshots'
shots.mkdir(exist_ok=True)
source = root / 'dist' / 'ui-review-0.3.1'
for src, name in [
    (release / 'motefield-presets-17-ui.png','MoteField-0.3.1.png'),
    (source / 'motefield-details.png','MoteField-Details.png'),
    (source / 'motefield-perform-1.png','MoteField-Loop-Capture.png'),
    (source / 'motefield-perform-2.png','MoteField-Material-Magnet.png'),
    (source / 'motefield-perform-3.png','MoteField-Pattern-MIDI.png'),
    (release / 'motefield-looper-27-ui.png','MoteField-Overdub.png'),
]:
    if not src.exists():
        raise FileNotFoundError(src)
    shutil.copy2(src, shots / name)
for old, new in [('motefield-ui-compiled.png','MoteField-0.3.1.png'),
                 ('motefield-details.png','MoteField-Details.png'),
                 ('motefield-granules.png','MoteField-0.3.1.png')]:
    dst = design / old
    if dst.exists() and not (archive / old).exists():
        shutil.copy2(dst, archive / old)
    shutil.copy2(shots / new, dst)
for old, src in [('motefield-held.png','motefield-presets-57-ui.png'),
                 ('motefield-delay.png','motefield-presets-47-ui.png'),
                 ('motefield-preview.mp4','motefield-presets-1080p.mp4'),
                 ('motefield-demo.wav','motefield-presets.wav')]:
    dst = design / old
    if dst.exists() and not (archive / old).exists():
        shutil.copy2(dst, archive / old)
    shutil.copy2(release / src, dst)
for name in ['motefield-small.png', 'motefield-idle.png']:
    dst = design / name
    if dst.exists() and not (archive / name).exists(): shutil.copy2(dst, archive / name)
    shutil.copy2(source / name, dst)

def embed(path, mime='image/png'):
    return 'data:' + mime + ';base64,' + base64.b64encode(path.read_bytes()).decode()

views = [('Instrument','MoteField-0.3.1.png'),('Details','MoteField-Details.png'),
         ('Loop & capture','MoteField-Loop-Capture.png'),('Material & magnet','MoteField-Material-Magnet.png'),
         ('Pattern & MIDI','MoteField-Pattern-MIDI.png'),('Overdub','MoteField-Overdub.png')]
font = embed(root / 'Assets' / 'OpenSauceMedium.ttf','font/ttf')
logo = embed(root / 'Assets' / 'RangoLogo.png')
for target, prefix in [(design / 'index.html','Release-0.3.1/'),
                        (design / 'brand-directions' / 'MoteField-Hardware-Review.html','../Release-0.3.1/')]:
    if target.exists() and target.parent.name == 'brand-directions' and not (archive / target.name).exists():
        shutil.copy2(target, archive / target.name)
    buttons = ''.join(f'<button role="tab" id="tab-{i}" aria-selected="{str(i==0).lower()}" aria-controls="view-{i}" tabindex="{0 if i==0 else -1}" data-index="{i}">{title}</button>' for i,(title,_) in enumerate(views))
    panels = ''.join(f'<figure role="tabpanel" id="view-{i}" aria-labelledby="tab-{i}" {"hidden" if i else ""}><a href="{prefix}Screenshots/{name}" download><img src="{embed(shots/name)}" alt="MoteField 0.3.1 — {title} screen"></a><figcaption>{title} <a href="{prefix}Screenshots/{name}" download>Download PNG ↗</a></figcaption></figure>' for i,(title,name) in enumerate(views))
    movies=''
    for n,clip,title,desc in [('01','presets','One phrase. Five different worlds.','Dry input → First Light → Near Orbit → Glass Seeds → Pocket Cuts → After Hours → Hold.'),('02','looper','Build it. Turn it around.','Record → play with input off → overdub → reverse → half speed → undo → stop.')]:
        size=(release/f'motefield-{clip}-1080p.mp4').stat().st_size/1e6
        movies+=f'<article><div class="eyebrow">{n} / {"PRESET EXPLORATION" if clip=="presets" else "PHRASE LOOPER"}</div><h3>{title}</h3><video controls playsinline preload="metadata" poster="{prefix}motefield-{clip}-{17 if clip=="presets" else 27}-1080.png"><source src="{prefix}motefield-{clip}-1080p.mp4" type="video/mp4"></video><p>{desc}</p><a class="download" href="{prefix}motefield-{clip}-1080p.mp4" download>Download 1080p MP4 <span>60 sec · {size:.1f} MB</span></a></article>'
    html='''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MoteField 0.3 — Rango Labs</title><style>
@font-face{font-family:Sauce;src:url(FONT)}*{box-sizing:border-box}body{margin:0;background:#ede6cf;color:#24281e;font-family:Sauce,Arial,sans-serif}main{max-width:1450px;margin:auto;padding:36px 40px 70px}header{display:flex;align-items:center;justify-content:space-between;border-bottom:1px solid #bcb99f;padding-bottom:24px}header img{width:68px;height:55px;object-fit:contain}header a{color:inherit;font-size:14px;text-decoration:none}.wordmark{display:flex;align-items:center;gap:18px}h1{font-size:clamp(46px,7vw,94px);letter-spacing:-.065em;line-height:1.05;margin:56px 0 20px}h2{font-size:36px;letter-spacing:-.035em;margin:0 0 14px}h3{font-size:27px;letter-spacing:-.025em;line-height:1.2;margin:12px 0 24px}.intro{max-width:780px;font-size:20px;line-height:1.55;color:#565a48}.eyebrow{font-size:12px;letter-spacing:.13em;font-weight:bold}.tag{display:inline-block;background:#d1dca3;border:1px solid #8b956e;padding:7px 12px;border-radius:30px;font-size:12px}.tabs{display:flex;gap:8px;flex-wrap:wrap;margin:32px 0 18px}.tabs button{font:inherit;font-size:13px;color:#383e2d;border:1px solid #babba1;padding:11px 16px;background:transparent;border-radius:6px;cursor:pointer}.tabs button[aria-selected=true]{background:#24281e;color:#eee8d6;border-color:#24281e}button:focus-visible,a:focus-visible{outline:3px solid #67794b;outline-offset:5px}figure{margin:0;background:#23281e;padding:20px;border-radius:16px}figure[hidden]{display:none}figure img{display:block;width:100%;height:auto;max-height:920px;object-fit:contain}figcaption{display:flex;justify-content:space-between;gap:16px;padding:18px 3px 2px;color:#dce2c9;font-size:13px}figcaption a{color:#dce2c9}.notes{display:grid;grid-template-columns:repeat(3,1fr);gap:24px;margin:25px 0 68px}.notes p{line-height:1.6;font-size:14px;color:#5a5d4c}.notes b{font-size:14px}.films{background:#20251d;color:#eee8d6;border-radius:18px;padding:36px;margin-top:36px}.films>p{max-width:760px;line-height:1.6;color:#c3c9af}.videos{display:grid;grid-template-columns:1fr 1fr;gap:32px;margin-top:36px}video{display:block;width:100%;border-radius:8px;background:#11180f}.videos article p{font-size:13px;color:#bdc4ab;line-height:1.65;min-height:45px}.download{color:#dee9b7;display:flex;justify-content:space-between;gap:10px;font-size:13px;padding:16px 0;text-decoration:none;border-top:1px solid #4e5740}.download span{color:#aeb79a}.extras{display:flex;flex-wrap:wrap;gap:22px;margin:28px 0 0}.extras a{color:#dee9b7;font-size:13px}footer{font-size:12px;color:#70755e;margin-top:28px;line-height:1.7}@media(max-width:760px){main{padding:20px 16px 36px}h1{margin-top:36px}.intro{font-size:17px}.notes,.videos{grid-template-columns:1fr;gap:18px}.notes{margin-bottom:35px}.films{padding:24px 16px}figure{padding:8px;border-radius:10px}figcaption{font-size:11px}.tabs{gap:5px}.tabs button{padding:10px;font-size:12px}.download{flex-wrap:wrap}header img{width:47px}header a{font-size:12px}}
</style></head><body><main><header><div class="wordmark"><img src="LOGO" alt="Rango Labs"><span>Rango Labs</span></div><a href="#films">Watch the demos ↓</a></header><h1>Sound in motion.</h1><span class="tag">MoteField / 0.3.1</span><p class="intro">A worn cream instrument for fragments, floating textures, and phrases worth keeping. The current interface, captured straight from MoteField.</p><div class="tabs" role="tablist" aria-label="Interface views">BUTTONS</div>PANELS<div class="notes"><div><b>Feels like an instrument.</b><p>Ribbed knobs, cream enamel, recessed displays, and organic black print. A reactive mark anchors the enclosure.</p></div><div><b>See what you hear.</b><p>Floating material responds to the processed audio. Shape shows the grain envelope; Grid shows its delay taps.</p></div><div><b>More room to perform.</b><p>Record and save phrases, shape the material, map MIDI, and build repeatable patterns in the new Performance pages.</p></div></div><section class="films" id="films"><div class="eyebrow">LANTERNS ABOVE THE TIDE / ORIGINAL DEMO SCORE</div><h2 style="margin-top:15px">A small phrase, opened up.</h2><p>A glass-harp arpeggio at 96 BPM. Two one-minute performances with the actual MoteField sound and interface. Each 1080p file is comfortably below 220 MB.</p><div class="videos">MOVIES</div><div class="extras"><a href="PREFIXLanterns-Above-the-Tide.mid" download>Original MIDI ↗</a><a href="PREFIXmotefield-presets-source.wav" download>Dry arpeggio WAV ↗</a><a href="PREFIXrecorded-phrase.wav" download>Recorded phrase WAV ↗</a><a href="PREFIXComposition.txt">Composition notes ↗</a></div></section><footer>Rango Labs · MoteField 0.3.1 · September 2026<br>These views reflect the current native interface. Earlier browser concepts are kept separately as design history.</footer></main><script>
const tabs=[...document.querySelectorAll('[role=tab]')],panels=[...document.querySelectorAll('[role=tabpanel]')];function activate(n){tabs.forEach((t,i)=>{t.setAttribute('aria-selected',i===n);t.tabIndex=i===n?0:-1;panels[i].hidden=i!==n})}tabs.forEach((t,n)=>{t.addEventListener('click',()=>activate(n));t.addEventListener('keydown',e=>{let next=n;if(e.key==='ArrowRight')next=(n+1)%tabs.length;else if(e.key==='ArrowLeft')next=(n+tabs.length-1)%tabs.length;else if(e.key==='Home')next=0;else if(e.key==='End')next=tabs.length-1;else return;e.preventDefault();activate(next);tabs[next].focus()})});document.querySelectorAll('video').forEach(v=>v.addEventListener('play',()=>document.querySelectorAll('video').forEach(o=>{if(o!==v)o.pause()})));
</script></body></html>'''
    for key,value in [('FONT',font),('LOGO',logo),('BUTTONS',buttons),('PANELS',panels),('MOVIES',movies),('PREFIX',prefix)]: html=html.replace(key,value)
    target.write_text(html)
readme = design / 'brand-directions' / 'README.md'
text = readme.read_text()
marker = '## Current review — 0.3.1'
if marker not in text:
    text = marker + '''

Open [MoteField Hardware Review](MoteField-Hardware-Review.html) for the current native interface and the two 1080p demonstrations. The main design entry point is [Design/index.html](../index.html). Six interface views cover the instrument, Details, the three Performance pages, and overdubbing.

The demo score, MIDI, screenshots, and video files are in `Design/Release-0.3.1`. The videos capture the native processor and editor on the same audio timeline. Older concepts below are retained as design history; they do not describe the current installed version. Earlier top-level images and the previous review entry point were backed up in `Design/Archive-before-0.3.1`.

---

''' + text
readme.write_text(text)
(design / 'README.md').write_text('''# MoteField design and demo assets

Open **index.html** for the current 0.3.1 UI review and demo videos.

- `Release-0.3.1/Screenshots/`: six native UI screenshots.
- `Release-0.3.1/motefield-presets-1080p.mp4`: 60-second preset exploration.
- `Release-0.3.1/motefield-looper-1080p.mp4`: 60-second phrase-looper performance.
- `Release-0.3.1/Lanterns-Above-the-Tide.mid`: original four-bar arpeggio.
- `Release-0.3.1/Composition.txt`: score and capture notes.
- `Archive-before-0.3.1/`: previous top-level assets and review.
- `brand-directions/`: earlier explorations; the Hardware Review entry opens the current review.

Rebuild the native MoteFieldDemo target, then run `python3 scripts/render-demo.py`,
`python3 scripts/demo-score.py`, and `python3 scripts/update-designs.py` from the project root.
''')
print(design / 'index.html')

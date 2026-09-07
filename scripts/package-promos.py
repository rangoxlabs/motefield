#!/usr/bin/env python3
"""Verify full source passes, then package the native promo captures."""
from pathlib import Path
import array, hashlib, json, math, re, subprocess, zipfile
ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 'Design/Promo-0.3.3-v4'
assets = ROOT / 'Design/Promo-0.3.3-v2'
archive = ROOT / 'dist/promos/MoteField-0.3.3-Promo-v4.zip'
clips = [
 ('arp-1', 'Closure / pulse & movement', [12, 0, 5]),
 ('arp-2', 'Closure / clouds & fragments', [16, 20, 6]),
 ('piano-1', 'Hope / space & sustain', [8, 1, 18]),
 ('piano-2', 'Hope / rhythm & detail', [11, 19, 29]),
 ('looper', 'Record once. Reshape it.', [1, 18]),
]

def decode(path):
 a = array.array('f'); a.frombytes(subprocess.check_output(['ffmpeg','-v','error','-i',str(path),'-f','f32le','-']))
 return a

def probe(path):
 return json.loads(subprocess.check_output(['ffprobe','-v','error','-show_format','-show_streams','-of','json',str(path)]))

checks = json.loads((out / 'source-checksums.json').read_text())
for name, checksum in checks.items():
 assert hashlib.sha256((assets/name).read_bytes()).hexdigest() == checksum, 'Original source changed'
report = {'original_source_sha256': checks, 'clips': {}}
sections = []
for clip, title, presets in clips:
 path = out / f'motefield-{clip}-1080p.mp4'; media = probe(path)
 is_loop = clip == 'looper'; instrument = 'arp' if clip.startswith('arp') else 'piano'
 source = decode(out/f'input-{instrument}-48k.wav'); length = len(source)//2
 expected_samples = length*4 + (48000*2 if is_loop else 48000*6+24000)
 duration = float(media['format']['duration']); expected_video_seconds = math.ceil(expected_samples/1600)/30
 assert abs(duration-expected_video_seconds) < .05 and path.stat().st_size < 220_000_000
 assert any(s['codec_type']=='video' and s['width']==1920 and s['height']==1080 for s in media['streams'])
 peak = float(re.findall(r'Peak:\s+(-?[\d.]+) dBFS',(out/f'motefield-{clip}-audio-qc.txt').read_text())[-1]); assert peak < -.5
 log = (out/f'motefield-{clip}-events.txt').read_text()
 chapters = [(int(a),int(b),int(c)) for a,b,c in re.findall(r'SECTION (\d+) START_SAMPLE (\d+) SOURCE_SAMPLES (\d+)',log)]
 assert len(chapters)==4
 counts = [int(x) for x in re.findall(r'VERIFIED_SOURCE_COUNT \d+ (\d+)',log)]
 assert counts == ([length,0,0,0] if is_loop else [length]*4)
 dry = decode(out/f'motefield-{clip}-source.wav'); processed = decode(out/f'motefield-{clip}.wav')
 gain = float(re.search(r'Constant delivery gain: ([\d.]+)',log)[1])
 assert len(dry)==expected_samples*2 and len(processed)==expected_samples*2
 errors = []; differences = []; rms = []
 for section,start,count in chapters:
  if count:
   # Compare the ENTIRE file, including both endpoints. This catches bar slicing,
   # early wraparound, omissions and a truncated last note.
   error = max(abs(dry[start*2+i]-source[i]*.4*gain) for i in range(length*2)); assert error < 2e-5
   errors.append(error)
  else:
   assert max(abs(v) for v in dry[start*2:(start+length)*2]) < 1e-7
  if section:
   n=length*2
   energy=math.sqrt(sum(v*v for v in processed[start*2:(start+length)*2])/n); assert energy>.005
   delta=math.sqrt(sum((processed[start*2+i]-processed[i])**2 for i in range(n))/n); assert delta>.001
   differences.append(delta);rms.append(energy)
 item={'duration_seconds':duration,'bytes':path.stat().st_size,'encoded_true_peak_dbfs':peak,'source_length_samples':length,'verified_source_counts':counts,'full_file_comparison_errors':errors,'processed_difference_rms':differences,'processed_rms':rms,'presets':presets}
 if is_loop:
  recording=decode(out/'recorded-piano-phrase.wav');assert len(recording)==len(source)
  item['recorded_length_samples']=len(recording)//2
  item['recorded_source_error']=max(abs(recording[i]-source[i]*.4) for i in range(4800,len(source)-4800))
  assert item['recorded_source_error']<2e-5
 report['clips'][clip]=item
 subtitle='Full dry loop, then three presets. Every pass finishes; effect tails have room to decay.' if not is_loop else 'Capture the full phrase in PRE-FX, then reshape the recording with the input off.'
 sections.append(f'<section><small>{duration:.1f} SEC · 1080P · {path.stat().st_size/1e6:.1f} MB</small><h2>{title}</h2><p>{subtitle}</p><video controls playsinline preload="metadata" poster="motefield-{clip}-section-1.png" src="{path.name}"></video><a href="{path.name}" download>Download MP4</a></section>')
(out/'audio-verification.json').write_text(json.dumps(report,indent=2)+'\n')
notes='''MOTEFIELD / RANGO LABS / COMPLETE-PHRASE PROMOS

Sources supplied by the user, preserved unchanged:
OS_UMS_133_synth_arp_melody_closure_C#m.wav — 133 BPM, C# minor.
DBM_LV_91_Piano_Loop_Hope_G#min.wav — 91 BPM, G# minor.
The piano is resampled from 44.1 to 48 kHz for processing, without changing tempo or pitch.

Each tour plays the ENTIRE original file dry, then plays it once through each
of three presets. There is no slicing, internal restart, or shortened phrase.
Preset changes occur at exact audio sample boundaries, independently of video
frame rate. Dry has a half-second gap; each processed pass has a two-second
tail. The final fade affects only the tail after the source has finished.
Mix is 0% for dry, then restored to each preset's stored value.

CLOSURE / PULSE & MOVEMENT
Skipping Stones — rhythmic normal/half-speed slices.
First Light — overlapping loops with lower-octave layers.
After Hours — tempo-related stereo delay.
CLOSURE / CLOUDS & FRAGMENTS
Dust Halo — denser normal/half-speed granular layers.
Glass Seeds — articulated grains with octave-up accents.
Tidal — long, diffused delays.
HOPE / SPACE & SUSTAIN
Warm Current — restrained octave layers around the piano.
Soft Focus — normal/octave-up granular cloud.
Moon Pool — longer, pitch-preserving orbiting grains.
HOPE / RHYTHM & DETAIL
Copper Chain — filtered, sequenced slices at original pitch.
Pin Drops — short articulated grains at original pitch.
Ink Wash — dark, diffused delay.

Selection emphasizes harmonic compatibility and distinct rhythmic/textural
roles. All 32 presets were rendered through both complete sources and checked
for finite output, peak headroom, transient steps, width, and effect contrast.
These measurements do not establish a subjective best preset.

RECORD ONCE. RESHAPE IT.
1. Record the full clean piano phrase in PRE-FX, monitoring at Mix 0%.
2. Turn off the source input. Play the captured phrase through Soft Focus.
3. At the next complete loop boundary, switch to Moon Pool.
4. Perform a gradual filter, space and shape swell on the next full pass.
Stop only after the phrase completes, then let the tail fade.
The video uses recorded audio alone after the capture. It does not simulate
playback by continuing to feed the original file to the plugin.

Actual native MoteField 0.3.3 processor/editor rendered offline at 30 fps,
with editorial captions. Source input gain is 0.4 for headroom, followed by
one constant delivery gain per clip across both dry and processed audio.
No per-preset normalization, external limiter, extra instruments or effects.
1920 x 1080 H.264 / stereo AAC 256 kbps. Each video is below 220 MB.
'''
(out/'Production-notes.txt').write_text(notes)
html='''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MoteField / Full-phrase demos</title><style>*{box-sizing:border-box}body{margin:0;background:#0a1112;color:#eef2ed;font:17px system-ui;line-height:1.6}main{max-width:1440px;margin:auto;padding:40px 24px 80px}small,.brand,a{color:#d4f85b}.brand,small{letter-spacing:.1em}h1{font-size:clamp(36px,6vw,72px);line-height:1.05;letter-spacing:-.04em}h2{font-size:32px}p{color:#bac5c2}section{border-top:1px solid #35443e;margin-top:48px;padding-top:24px}video{width:100%;display:block;margin:20px 0;border-radius:16px;background:#000}footer{margin-top:48px;font-size:14px}a{display:inline-block;margin-right:18px}</style><main><div class="brand">RANGO LABS / MOTEFIELD</div><h1>Let the phrase unfold.</h1><p>Complete source loops. Six presets per instrument across two videos. A separate performance turns a recorded piano phrase into an evolving texture.</p><a href="../../dist/promos/MoteField-0.3.3-Promo-v4.zip" download>Download all five videos</a>'''
html+=''.join(sections)+'<footer><a href="Production-notes.txt">Production notes</a><p>Native processor/editor captures rendered offline. Supplied original files remain unchanged.</p></footer></main></html>'
(out/'index.html').write_text(html)
archive.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
 for p in [out/f'motefield-{c[0]}-1080p.mp4' for c in clips]+[out/'Production-notes.txt']:z.write(p,p.name)
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
archive.with_suffix('.zip.sha256').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
(ROOT/'Design/README.md').write_text('Current promos: [Full-phrase preset tours and PRE-FX recording performance](Promo-0.3.3-v4/index.html).\n\nRejected demo exports were deleted. Both original user-supplied WAVs remain unchanged in Promo-0.3.3-v2.\n')
print(json.dumps(report,indent=2));print('Packaged',archive,archive.stat().st_size/1e6,'MB')

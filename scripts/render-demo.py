#!/usr/bin/env python3
"""Capture the native processor/editor directly into bounded-size 1080p demo videos."""
import argparse
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--clip', choices=['presets', 'looper', 'all'], default='all')
parser.add_argument('--short', action='store_true')
parser.add_argument('--audio-only', action='store_true')
parser.add_argument('--output', type=pathlib.Path, default=ROOT / 'Design' / 'Release-0.3.1')
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
exe = ROOT / 'build-macos' / 'Release' / 'MoteFieldDemo'
for clip in (['presets', 'looper'] if args.clip == 'all' else [args.clip]):
    stem = f'motefield-{clip}'
    command = [str(exe), str(out), clip]
    if args.audio_only:
        command.append('--audio-only')
    elif args.short:
        command.append('--short')
    with (out / f'{stem}-render.log').open('w') as log:
        if args.audio_only:
            subprocess.run(command, stdout=subprocess.DEVNULL, stderr=log, check=True)
            continue
        silent = out / f'{stem}-silent.mp4'
        render = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=log)
        encode = subprocess.Popen(['ffmpeg', '-hide_banner', '-loglevel', 'warning', '-y',
            '-f', 'rawvideo', '-pixel_format', 'bgr24', '-video_size', '1920x1080', '-framerate', '30', '-i', 'pipe:0',
            '-an', '-c:v', 'libx264', '-preset', 'fast', '-crf', '18', '-maxrate', '10M', '-bufsize', '20M',
            '-pix_fmt', 'yuv420p', '-colorspace', 'bt709', '-color_primaries', 'bt709', '-color_trc', 'bt709',
            '-movflags', '+faststart', str(silent)], stdin=render.stdout, stderr=log)
        render.stdout.close()
        encoded = encode.wait()
        rendered = render.wait()
        if encoded or rendered:
            raise RuntimeError(f'{clip} failed (render={rendered}, encode={encoded}); see {log.name}')
        destination = out / f'{stem}-1080p.mp4'
        subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'warning', '-y', '-i', str(silent),
            '-i', str(out / f'{stem}.wav'), '-map', '0:v:0', '-map', '1:a:0', '-c:v', 'copy',
            '-af', 'volume=6dB', '-c:a', 'aac', '-b:a', '256k', '-ar', '48000', '-movflags', '+faststart', '-shortest',
            '-metadata', 'artist=Rango Labs', '-metadata', 'title=MoteField - ' + ('Phrase looper' if clip == 'looper' else 'Preset exploration'),
            '-metadata', 'comment=Original composition: Lanterns Above the Tide. Native MoteField 0.3.1 processor and editor capture.',
            str(destination)], stderr=log, check=True)
        assert destination.stat().st_size < 220_000_000, 'Video exceeds 220 MB'
        probe = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-show_streams', '-show_format', '-of', 'json', str(destination)]))
        expected_duration = 2 if args.short else 60
        assert abs(float(probe['format']['duration']) - expected_duration) < .1
        assert any(s['codec_type'] == 'audio' for s in probe['streams'])
        assert any(s['codec_type'] == 'video' and s['width'] == 1920 and s['height'] == 1080 for s in probe['streams'])
        (out / f'{stem}-media.json').write_text(json.dumps(probe, indent=2) + '\n')
        # This is only the reproducible intermediate created immediately above.
        silent.unlink()
        print(f'{destination.name}: {destination.stat().st_size / 1_000_000:.1f} MB, {expected_duration}s, verified', flush=True)

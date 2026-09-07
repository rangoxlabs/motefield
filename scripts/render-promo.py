#!/usr/bin/env python3
"""Render reproducible wet/dry promos from the native 0.3.3 processor/editor."""
import argparse,json,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--clip',choices=['arp-1','arp-2','piano-1','piano-2','looper','all'],default='all');p.add_argument('--audio-only',action='store_true');args=p.parse_args()
out=ROOT/'Design/Promo-0.3.3-v4';out.mkdir(parents=True,exist_ok=True)
for clip in (['arp-1','arp-2','piano-1','piano-2','looper'] if args.clip=='all' else [args.clip]):
    stem='motefield-'+clip
    arp=clip.startswith('arp')
    source=ROOT/'Design/Promo-0.3.3-v2'/('OS_UMS_133_synth_arp_melody_closure_C#m.wav' if arp else 'DBM_LV_91_Piano_Loop_Hope_G#min.wav')
    prepared=out/('input-arp-48k.wav' if arp else 'input-piano-48k.wav')
    if not prepared.exists():
        subprocess.run(['ffmpeg','-v','error','-y','-i',str(source),'-ar','48000','-ac','2','-c:a','pcm_f32le',str(prepared)],check=True)
    bpm=133 if arp else 91
    presets={'arp-1':'12,0,5','arp-2':'16,20,6','piano-1':'8,1,18','piano-2':'11,19,29','looper':'1,18'}[clip]
    cmd=[str(ROOT/'build-macos/Release/MoteFieldPromo'),str(out),clip,str(prepared),str(bpm),presets]
    with (out/(stem+'-render.log')).open('w') as log:
        if args.audio_only:
            subprocess.run(cmd+['--audio-only'],stdout=subprocess.DEVNULL,stderr=log,check=True);continue
        silent=out/(stem+'-silent.mp4')
        render=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=log)
        encode=subprocess.Popen(['ffmpeg','-hide_banner','-loglevel','warning','-y','-f','rawvideo','-pixel_format','bgr24','-video_size','1920x1080','-framerate','30','-i','pipe:0','-an','-c:v','libx264','-preset','fast','-crf','18','-maxrate','8M','-bufsize','16M','-pix_fmt','yuv420p','-movflags','+faststart',str(silent)],stdin=render.stdout,stderr=log)
        render.stdout.close();encoded=encode.wait();rendered=render.wait()
        if encoded or rendered:raise RuntimeError(f'{clip}: renderer={rendered}, encoder={encoded}; see {log.name}')
        final=out/(stem+'-1080p.mp4')
        subprocess.run(['ffmpeg','-hide_banner','-loglevel','warning','-y','-i',str(silent),'-i',str(out/(stem+'.wav')),'-map','0:v:0','-map','1:a:0','-c:v','copy','-c:a','aac','-b:a','256k','-ar','48000','-movflags','+faststart','-shortest','-metadata','artist=Rango Labs','-metadata','title=MoteField / '+clip,'-metadata','comment=User-provided source audio and native MoteField 0.3.3 processor/editor capture. One constant delivery gain across dry and processed sections.',str(final)],stderr=log,check=True)
        probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_streams','-show_format','-of','json',str(final)]))
        source_info=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries','stream=duration_ts,time_base','-of','json',str(prepared)]))['streams'][0]
        source_samples=int(source_info['duration_ts'])
        import math
        seconds=math.ceil((source_samples*4+(48000*2 if clip=='looper' else 48000*6+24000))/1600)/30
        assert abs(float(probe['format']['duration'])-seconds)<.1
        assert final.stat().st_size<220_000_000
        assert any(s['codec_type']=='audio' for s in probe['streams'])
        assert any(s['codec_type']=='video' and s['width']==1920 and s['height']==1080 for s in probe['streams'])
        (out/(stem+'-media.json')).write_text(json.dumps(probe,indent=2)+'\n')
        subprocess.run(['ffmpeg','-hide_banner','-i',str(final),'-af','ebur128=peak=true','-f','null','-'],stdout=subprocess.DEVNULL,stderr=(out/(stem+'-audio-qc.txt')).open('w'),check=True)
        silent.unlink();print(f'{final.name}: {seconds}s, {final.stat().st_size/1e6:.1f} MB, verified',flush=True)

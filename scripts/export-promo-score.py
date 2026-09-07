#!/usr/bin/env python3
"""Export the original promo note sequences as MIDI."""
from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'Design/Promo-0.3.3-v2';out.mkdir(parents=True,exist_ok=True)
def vlq(n):
    b=[n&127]
    while n>>7:n>>=7;b.insert(0,(n&127)|128)
    return bytes(b)
def write(name,bpm,notes,program):
    title=name.encode();tempo=round(60_000_000/bpm)
    events=[(0,b'\xff\x03'+vlq(len(title))+title),(0,b'\xff\x51\x03'+tempo.to_bytes(3,'big')),(0,b'\xff\x58\x04\x04\x02\x18\x08'),(0,bytes([0xc0,program]))]
    for start,duration,note,velocity in notes:events.extend([(start,bytes([0x90,note,velocity])),(start+duration,bytes([0x80,note,0]))])
    events.append((max(7680,max(t for t,_ in events)),b'\xff\x2f\x00'))
    track=b'';last=0
    for tick,event in sorted(events,key=lambda x:x[0]):track+=vlq(tick-last)+event;last=tick
    (out/(name+'.mid')).write_bytes(b'MThd'+struct.pack('>IHHH',6,0,1,480)+b'MTrk'+struct.pack('>I',len(track))+track)
melody=[71,-1,67,66,64,-1,66,69,67,-1,64,62,64,-1,67,71,74,-1,71,69,67,-1,66,67,69,-1,66,64,62,-1,66,69]
write('Lanterns-Over-the-Water-v2',96,[(step*240,min(360,7680-step*240),note,94 if step%8==0 else 77) for step,note in enumerate(melody) if note>=0],10)
chords=[[51,54,58,61,65],[47,51,54,58,61],[44,47,51,54,58],[46,50,53,56,59]]
notes=[]
for bar,chord in enumerate(chords):
    for strike in range(2):
        for n,note in enumerate(chord):notes.append((bar*1920+strike*1200+round(n*.009*800),650 if strike else 1050,note,(74-n*3) if strike==0 else (46-n*2)))
    notes.append((bar*1920,1440,chord[0]-12,80))
write('After-the-Last-Train-v2',100,notes,4)
write('After-the-Last-Train-Counterline-v2',100,[(s*480,410,chords[s//4][(s*2+1)%5]+12,78) for s in range(16)],10)
print('Exported three original MIDI parts.')

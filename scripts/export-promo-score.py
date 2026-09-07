#!/usr/bin/env python3
"""Export the original promo note sequences as MIDI."""
from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'Design/Promo-0.3.3';out.mkdir(parents=True,exist_ok=True)
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
chords=[[62,66,69,73,76],[59,62,66,69,73],[55,59,62,66,69],[57,61,64,69,71]]
pattern=[0,2,1,3,2,4,3,1,2,3,4,2,1,3,2,1]
write('Lanterns-Over-the-Water',120,[(s*120,108,chords[s//16][pattern[s%16]]+(12 if s%16==8 else 0),94 if s%4==0 else 75) for s in range(64)],10)
chords=[[51,54,58,61,65],[47,51,54,58,61],[44,47,51,54,58],[46,50,53,56,59]]
notes=[]
for bar,chord in enumerate(chords):
    for n,note in enumerate(chord):notes.append((bar*1920+round(n*.012*640),1710,note,74-n*3))
    notes.append((bar*1920,1440,chord[0]-12,80))
write('After-the-Last-Train',80,notes,4)
write('After-the-Last-Train-Counterline',80,[(s*480,410,chords[s//4][(s*2+1)%5]+12,78) for s in range(16)],10)
print('Exported three original MIDI parts.')

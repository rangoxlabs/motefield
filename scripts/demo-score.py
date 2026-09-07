#!/usr/bin/env python3
"""Export the original demo arpeggio as standard MIDI (96 BPM, four bars)."""
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[1]
out = root / 'Design' / 'Release-0.3.1'
out.mkdir(parents=True, exist_ok=True)
chords = [[62,66,69,73,76], [59,62,66,69,73], [55,59,62,66,69], [57,62,64,66,71]]
pattern = [0,2,1,3,2,4,3,2,4,2,3,1,2,1,3,2]

def vlq(value):
    data = [value & 127]
    while value >> 7:
        value >>= 7
        data.insert(0, (value & 127) | 128)
    return bytes(data)

name = b'Lanterns Above the Tide - Rango Labs'
events = [(0, b'\xff\x03' + vlq(len(name)) + name), (0, b'\xff\x51\x03\x09\x89\x68'),
          (0, b'\xff\x58\x04\x04\x02\x18\x08'), (0, bytes([0xc0, 10]))]
for step in range(64):
    note = chords[step // 16][pattern[step % 16]] + (12 if step % 16 == 8 else 0)
    events.extend([(step * 120, bytes([0x90, note, 92 if step % 4 == 0 else 72])),
                   (step * 120 + 104, bytes([0x80, note, 0]))])
events.append((7680, b'\xff\x2f\x00'))
track = b''
last = 0
for tick, event in sorted(events, key=lambda e: e[0]):
    track += vlq(tick - last) + event
    last = tick
(out / 'Lanterns-Above-the-Tide.mid').write_bytes(b'MThd' + struct.pack('>IHHH', 6, 0, 1, 480) + b'MTrk' + struct.pack('>I', len(track)) + track)
(out / 'Composition.txt').write_text('''LANTERNS ABOVE THE TIDE
Original demo composition for Rango Labs / MoteField 0.3.1.
96 BPM, 4/4, four bars, sixteenth-note glass-harp arpeggio.
Harmony: Dmaj9 / Bm9 / Gmaj9 / A6sus.
A light, nostalgic fantasy-game mood; no existing game melody was used.
Audio is synthesized from additive sine partials, with no sampled recordings.

The MIDI file contains the four-bar core figure. Instrument timbre is not embedded
in MIDI; use a soft harp, bell, or plucked synth. The exact synthesized demo timbre
is in the source WAV. The looper video adds an original high counterline during DUB.

Capture: actual MoteField processor and native editor, rendered on a shared audio
timeline. Chapter captions and control highlights are editorial overlays. The final
preset-video second has a delivery fade. The MP4s use a constant +6 dB delivery
trim across both clips. No extra reverb, compression, or limiting was added.
The source and processor WAVs retain their original levels.
''')
print(out / 'Lanterns-Above-the-Tide.mid')

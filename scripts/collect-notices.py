#!/usr/bin/env python3
"""Collect upstream notices from the exact JUCE checkout used for the build."""
import argparse
from pathlib import Path
import shutil

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--juce-dir', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
project = Path(__file__).resolve().parent.parent
notices = {
    'JUCE.md': 'LICENSE.md',
    'AudioUnitSDK.txt': 'modules/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt',
    'AAX.txt': 'modules/juce_audio_plugin_client/AAX/SDK/LICENSE.txt',
    'VST3.txt': 'modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt',
    'FLAC.txt': 'modules/juce_audio_formats/codecs/flac/Flac Licence.txt',
    'Ogg-Vorbis.txt': 'modules/juce_audio_formats/codecs/oggvorbis/Ogg Vorbis Licence.txt',
    'JPEG.txt': 'modules/juce_graphics/image_formats/jpglib/README',
    'PNG.txt': 'modules/juce_graphics/image_formats/pnglib/LICENSE',
    'zlib.txt': 'modules/juce_core/zip/zlib/README',
    'HarfBuzz.txt': 'modules/juce_graphics/fonts/harfbuzz/COPYING',
    'SheenBidi.txt': 'modules/juce_graphics/unicode/sheenbidi/LICENSE',
}
sources = {name: args.juce_dir / source for name, source in notices.items()}
sources.update({'MoteField-MIT.txt': project / 'LICENSE', 'Open-Sauce-Sans-OFL.txt': project / 'Assets/OFL.txt'})
for source in sources.values():
    if not source.is_file():
        parser.error(f'Missing upstream notice: {source}')
args.output.mkdir(parents=True, exist_ok=True)
for name, source in sources.items():
    shutil.copyfile(source, args.output / name)
(args.output / 'README.txt').write_text(
    'MoteField by Rango Labs\n\n'
    'Original MoteField code and each dependency retain their respective licenses.\n'
    'This folder includes notices for desktop builds; not every format uses every dependency.\n'
    'JUCE commercial licensing and Avid commercial authorization are separate from these notices.\n'
    'Source: https://github.com/rangoxlabs/motefield\n', encoding='utf-8')
print(f'Collected {len(sources)} notices into {args.output}')

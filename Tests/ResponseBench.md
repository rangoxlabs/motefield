# Musical response diagnostic

`MoteFieldResponseBench` renders all 44 mode/variation combinations at controlled engine defaults. It is an offline diagnostic, not a factory-preset ranking or a perceptual-quality test. It does not change the plugin engine.

Build with `MOTEFIELD_BUILD_TESTS=ON`, then run:

```sh
cmake -S . -B build-macos
cmake --build build-macos --config Release --target MoteFieldResponseBench --parallel 2
build-macos/Release/MoteFieldResponseBench /tmp/motefield-response.csv
```

For a single-configuration generator, the executable is directly inside the build directory. Create the CSV's parent directory first. Each run overwrites the specified CSV.

Each of 220 cases uses a fresh engine, 48 kHz, 480-sample blocks, 120 BPM, valid advancing host position, five seconds of processing, and one second of input silence before a probe. Other `EngineParameters` defaults remain intact except `mix=1`. Results are tied to the source revision used to build the executable; preserve that revision with every baseline.

Sources are a deterministic harmonic plucked note and a three-second raised-cosine swell, each with gains of −12 and −36 dB. These are gain settings, not actual waveform peaks: measured peaks are approximately −13 and −37 dBFS. Right input is left multiplied by 0.91. A digital-silence case completes each combination. No normalization or listening-level compensation is applied.

CSV fields:

| Field | Meaning |
|---|---|
| `input_peak_dbfs`, `wet_peak_dbfs` | Maximum absolute sample across the stereo channels. |
| `wet_rms_dbfs` | Stereo-mean RMS over the complete five seconds, including silence and tail. |
| `wet_to_input_rms_db` | Ratio of complete-render RMS values; blank for digital silence. Not loudness matching. |
| `first_response_ms` | Start of the first three consecutive 10 ms frames at or above −30 dB RMS relative to the measured input peak, measured from the source-envelope start. Blank when absent. |
| `active_frames_pct` | Fraction of post-onset frames above that threshold. Rests may be intentional; higher is not necessarily better. |
| `finite` | All output samples are finite. |

The dB floor is −240 dBFS for numeric reporting. Response timing has 10 ms resolution and includes the source's own attack. It is not hardware latency. The process returns failure for nonfinite output or generated silence-control output above `1e-7`; response absence remains data in the CSV, so the baseline can document an existing defect without concealing other modes' results.

The checked-in [0.3.7 baseline](Baselines/Response-0.3.7.csv) uses the engine from revision `7a03af04c46608d86425a88e780c30431c61e717`, rendered on macOS arm64 with the Release configuration. It finds effectively absent low-level output in all Pluck variations, consistent with the fixed onset threshold in the engine. This harness does not establish hardware equivalence or diagnose every musical weakness. Follow-up work should include level sweeps, real recordings, noise, stereo edge cases, different tempos and host phases, and direct event-timing measurements.

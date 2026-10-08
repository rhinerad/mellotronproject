# TapeBank

A polyphonic Mellotron-style tape-replay sampler: VST3 instrument plus a Standalone app, built with JUCE 8 / C++20.

Each key plays its own recorded "tape" for up to 8 seconds, like the real machine. The output runs through a model of the tape transport: wow, flutter, saturation, hiss and motor hum.

```
MIDI ─► Synthesiser (24 voices, SamplerVoice + ADSR)
          │  one SamplerSound per sample, mapped across C2–C6
          ▼
        Transport  ── modulated delay: wow (≈0.8 Hz sine) + flutter (5–15 Hz random)
          ▼
        Saturation ── 2× oversampled asymmetric tanh (Drive)
          ▼
        Hiss & hum ── optional; hiss grows with the number of keys held
          ▼
        High-pass 80 Hz ─► low-pass (Tone) ─► Volume ─► out
```

## Getting the plug-in

### Option A: download a build (no compiler needed)
Every push to GitHub runs the **Build** workflow (Actions tab). Open the latest run, download
`TapeBank-Windows`, `TapeBank-macOS` or `TapeBank-Linux`, and unzip it. That gives you the zip
inside; unzip that too.

### Option B: build it yourself
Requirements: CMake 3.22+ and a C++20 compiler (Visual Studio 2022, Xcode 14+, or GCC 11+/Clang 14+).
JUCE 8.0.13 is downloaded automatically on the first configure.

```bash
cmake -B build -S .
cmake --build build --config Release
```

The results land in `build/TapeBank_artefacts/Release/`:

| Format     | Path                                   |
|------------|----------------------------------------|
| VST3       | `VST3/TapeBank.vst3`                   |
| Standalone | `Standalone/TapeBank(.exe/.app)`       |

Linux also needs: `libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev libx11-dev
libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libgl1-mesa-dev`.

To have the build copy the VST3 into your system plug-in folder automatically, configure with
`-DTAPEBANK_COPY_AFTER_BUILD=ON`. On Windows this needs an admin terminal.

## Installing in Ableton Live

1. Copy the whole `TapeBank.vst3` folder (it is a bundle; keep it intact) to:
   - **Windows:** `C:\Program Files\Common Files\VST3\`
   - **macOS:** `/Library/Audio/Plug-Ins/VST3/` or `~/Library/Audio/Plug-Ins/VST3/`
2. macOS only, for downloaded builds: the plug-in isn't notarised, so clear the quarantine flag first:
   `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/TapeBank.vst3`
3. In Live: **Settings → Plug-Ins**, turn on **Use VST3 Plug-In System Folders**, then click **Rescan**.
4. TapeBank appears under **Plug-Ins → VST3 → YourName**. Drop it on a MIDI track and play.

## Samples

TapeBank starts with a synthetic **Built-in Tape Strings** bank, so it makes sound straight away.
To play real tapes, click **LOAD SAMPLES** and pick a folder, or drag a folder (or any file in it)
onto the panel. The folder is saved with your Live set and reloaded when the set is opened.

Supported formats: `.wav`, `.aif`/`.aiff`, `.flac`, mono or stereo, any sample rate.

### Naming convention
The root note is read from the file name:

| File name                     | Root note        |
|-------------------------------|------------------|
| `Strings_C3.wav`              | C3 (MIDI 48)     |
| `MkII Flute F#4.wav`          | F#4 (MIDI 66)    |
| `Choir-Bb2.wav`               | Bb2 (MIDI 46)    |
| `cello_060.wav`               | MIDI 60          |

- Note names use **scientific pitch: C4 = MIDI 60 = middle C**. Ableton labels MIDI 60 as "C3", so
  in Live the playable range C2–C6 shows as **C1–C5**.
- If a name contains several candidates, the last one wins.
- Each sample covers the keys nearest its root, so a bank with one sample every few semitones fills
  the keyboard by re-pitching.
- The keyboard spans **C2–C6** (MIDI 36–84). Notes outside that range are silent, as on a real tape frame.
- With no note names at all, a single file is mapped across the whole range (root C4); several files
  are laid out chromatically from C2 in name order.
- Only the first **8 seconds** of each sample play, like Mellotron tapes.

### Default bank
WAVs placed in this repo's `Samples/` folder are copied into every build. The plug-in looks for a
default bank in this order:

1. `TapeBank.vst3/Contents/Resources/Samples/` (or `TapeBank.app/Contents/Resources/Samples/`)
2. `Samples/` next to the Standalone executable (Windows/Linux)
3. `Documents/TapeBank/Samples/`
4. If none of those exist, the built-in synthetic strings

Paths are resolved from the plug-in binary itself, not the host's working directory.

## Controls

| Control       | Range          | What it does |
|---------------|----------------|--------------|
| Drive         | 0–100 %        | Pushes up to +18 dB into an asymmetric tanh "tape" (2× oversampled). |
| Wow           | 0–100 %        | Slow capstan pitch drift, ≈0.8 Hz, up to ≈±20 cents. |
| Flutter       | 0–100 %        | Fast, irregular 5–15 Hz pitch wobble plus random scrape. |
| Tone          | 500 Hz–15 kHz  | 12 dB/oct low-pass. An 80 Hz high-pass is always on. |
| Attack        | 1 ms–1 s       | Pressure-pad engage time. Default 15 ms, so notes start without clicks. |
| Release       | 10 ms–2 s      | Tape disengage time. Default 200 ms. |
| Hiss & Hum    | on/off         | Motor hum, plus tape hiss that grows with the number of keys held. |
| Volume        | −24…+6 dB      | Output level. |

Velocity sets each note's level, the sustain pedal (CC64) works, and the on-screen keyboard can be
clicked to audition sounds. The wow/flutter delay line adds about 3 ms of latency, which is
reported to the host for compensation.

## Project layout

```
Source/Parameters.h        APVTS parameter layout + value formatting
Source/PluginProcessor.*   AudioProcessor: state, background bank loading, synth -> tape chain
Source/SamplerEngine.*     Synthesiser/SamplerVoice, naming convention, folder + built-in banks
Source/TapeProcessor.*     Wow/flutter delay, saturation, hiss/hum, filters
Source/PluginEditor.*      Retro UI (custom LookAndFeel), file chooser, drag & drop, keyboard
Samples/                   Optional default bank, bundled into each build
```

To rename the product, change `PRODUCT_NAME`, `COMPANY_NAME`, `BUNDLE_ID` and
`PLUGIN_MANUFACTURER_CODE` in `CMakeLists.txt`. Keep `PLUGIN_CODE` unique to this plug-in.

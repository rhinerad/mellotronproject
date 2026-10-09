# Default sample bank

Audio files dropped in this folder are copied into each build:

- VST3: `TapeBank.vst3/Contents/Resources/Samples/`
- Standalone (macOS): `TapeBank.app/Contents/Resources/Samples/`
- Standalone (Windows/Linux): `Samples/` next to the executable

If this folder has no audio files, the plug-in falls back to its built-in synthetic strings.

Name each file with its root note, e.g. `Strings_C3.wav`, `Flute F#4.wav` or `cello_060.wav`.
Note names use scientific pitch (C4 = MIDI 60). See the main README for the full rules.
Supported: `.wav`, `.aif`, `.aiff`, `.flac`.

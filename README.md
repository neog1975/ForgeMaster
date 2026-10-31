# Forge Master

**Free, open-source one-click mastering plugin.** Forge Master listens to your mix, analyses it and sets up a complete mastering chain for you: tone EQ, 4-band dynamics, stereo width, saturation and a true-peak limiter that hits your loudness target for Spotify, Apple Music, YouTube, club play and more.

Think of it as an open alternative to online auto-mastering services that runs locally inside your DAW, with every setting open to tweak.

Formats: **VST3**, **AU** (macOS) and **Standalone**. Works in FL Studio, Ableton Live, Logic Pro, Cubase, Reaper and other hosts.

> Status: early release (v0.1.0). Feedback, bug reports and pull requests are very welcome.

## How it works

1. Load Forge Master on your master bus.
2. Press the big round auto-master button and play your song; Forge Master listens to about 20 seconds of non-silent audio (the chorus or drop works best).
3. Forge Master analyses the mix and derives every setting from it:
   - octave-band spectrum (40 Hz – 16 kHz) to correct tonal balance
   - crest factor (how dynamic the mix is) to set compression
   - stereo correlation and low-end side energy for width and mono bass
   - integrated loudness to reach the chosen target
4. Pick a **style** and **intensity**, then fine-tune in the Advanced view if you like.

## Features

- **One-click auto master** with styles *Balanced*, *Warm*, *Open* and *Flat*, plus an intensity control
- **Loudness targets**: Spotify (-14 LUFS), Apple Music (-16), YouTube (-14), Amazon Music (-14), Tidal (-14), Deezer (-15), Broadcast EBU R128 (-23), Medium (-11), Loud (-9), Club/DJ (-8) or a custom value
- **Loudness meter** following ITU-R BS.1770-4 / EBU R128 (momentary, short-term, gated integrated) and true-peak display
- **Tone EQ**: low shelf, low-mid, presence and air bands
- **4-band compressor** with Linkwitz-Riley crossovers and phase compensation, optional Mid/Side mode, per-band width
- **Stereo tools**: mono bass below a set frequency, global stereo width
- **Saturation** with dry/wet mix
- **Lookahead brickwall limiter** with oversampling and adjustable ceiling (dBTP)
- **Gain-matched bypass** to compare honestly at equal loudness
- Simple and Advanced views

## Building from source

Requirements: CMake 3.22+, a C++17 compiler (Xcode on macOS, Visual Studio 2022 on Windows) and Git. JUCE 8 is downloaded automatically during configuration.

```bash
git clone https://github.com/neog1975/ForgeMaster.git
cd ForgeMaster
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The built plug-ins are copied into your system plug-in folders automatically. Pass `-DFORGE_COPY_AFTER_BUILD=OFF` to disable this, or `-DJUCE_DIR=/path/to/JUCE` to use your own JUCE checkout.

On macOS, a universal binary (Apple Silicon + Intel) is built with:

```bash
cmake -B build-universal -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-universal --config Release
tools/make_installer.sh   # creates dist/ForgeMaster-<version>-macOS.pkg
```

## Developer tools

The `tools/` folder contains offline helpers (built unless `-DFORGE_BUILD_TOOLS=OFF`):

- **AutoMasterTest**: runs the auto master on a WAV file and renders the result. `AutoMasterTest --snapshot ui.png` renders the plugin window to an image without a DAW.
- **ForgeRender**: renders audio through a built VST3.
- **compare_landr.py**: measures Forge Master renders side by side with other masters of the same mix (loudness and spectrum).

## Contributing

Ideas, bug reports and pull requests are welcome. Good first areas:

- testing in different DAWs and on Windows
- presets and new styles
- improving the analysis and the auto-master decisions
- UI and usability

Please open an issue first for larger changes.

## License

Forge Master is licensed under the **GNU Affero General Public License v3.0** (see [LICENSE](LICENSE)). It is built with the [JUCE framework](https://juce.com), used under its AGPLv3 option.

VST is a registered trademark of Steinberg Media Technologies GmbH.

# Puzzle Magic

A gothic 9×9 block puzzle with a Lovecraftian edge, made in Unreal Engine 5.8 with C++. Drag pieces onto the board and clear rows, columns and 3×3 boxes in a candle-lit cathedral that darkens and flickers as your luck runs out. It is laid out for iPhone (portrait) and is developed and played on Windows.

Demo recording: [Demo/PuzzleMagic_Demo.mp4](Demo/PuzzleMagic_Demo.mp4)

## The game

- **9×9 board**: 9 rows, 9 columns and 9 boxes, so 27 lines can be cleared. Pieces come three at a time from 25 shapes, with a HOLD slot to keep one for later.
- **Endless** play and **15 quest levels** with star ratings.
- **Combos** earn a relic (Holy Light or Reroll) every third step. Luck grows with combos, is spent when relics are used, and wards off **omens**: lightning strikes and hexes. Gargoyle curse stones drop onto the board as you play.
- **How to play** pages open on first launch, and a built-in bot can play by itself.
- **Look and sound**: Lumen with hardware ray tracing and hit-lighting reflections, MegaLights, virtual shadow maps and TSR. The chant and organ music, sound effects and ambience are synthesized and play through a cathedral convolution reverb.

## Requirements

- Windows 10 or 11 and a DirectX 12 GPU with hardware ray tracing (developed on an AMD Radeon RX 6400, 4 GB).
- Unreal Engine 5.8. The `.bat` launchers expect it in `C:\Program Files\Epic Games\UE_5.8`.
- Visual Studio 2022 or its Build Tools with the C++ game development workload (built with MSVC 14.44).
- [Git LFS](https://git-lfs.com): assets, audio, textures and prebuilt libraries are stored in LFS.

## Get the code

```bat
git lfs install
git clone https://github.com/ValhalaStudio/PuzzleMagic.git "D:\Unreal Projects\PuzzleMagic"
```

Clone into a short folder like that one. Some files in the FSR plugin have 174-character paths, and Windows limits full paths to 260 characters, so a deeply nested clone fails to check out unless you first run `git config --global core.longpaths true`.

## Build and run

Open `PuzzleGame5x5.uproject` and let Unreal build the missing modules, or build from a terminal in the project folder:

```bat
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" PuzzleGame5x5Editor Win64 Development "-Project=%CD%\PuzzleGame5x5.uproject" -WaitMutex
```

| Launcher | What it does |
|---|---|
| `PlayGame.bat` | Starts the game in a phone-shaped 440×950 window at the main menu |
| `RunDemo.bat` | The bot plays Endless while FFmpeg (`C:\ffmpeg\bin\ffmpeg.exe`) records the window to `DemoCapture.mp4` |
| `RunQuestDemo.bat` | The bot plays through the quest levels from level 1; its results are not saved |

**Controls**: drag pieces from the tray, and drop one on HOLD to keep it. The relic buttons sit in the bottom corners. **P** switches between bot and manual play, **R** retries, and **Esc** or right-click cancels Holy Light targeting. On Windows, gamepad input goes through SDL3.

### Command-line options

| Option | Effect |
|---|---|
| `-demo` | The bot plays Endless |
| `-demoquest[=N]` | The bot plays the quest levels from level N (default 1) |
| `-level=N` | Opens quest level N's intro card |
| `-tutorial`, `-tutorialpage=N` | Shows the How to play pages over the menu (they open by themselves on first launch), at page N counting from 0. Ignored with `-demo`, `-demoquest` or `-level` |
| `-omenrate=X` | Fixed omen chance per move, 0 to 1 (the cooldown between omens still applies) |
| `-dread=X` | Keeps the dread effects at least at X, 0 to 1 |
| `-fsr` | AMD FSR upscaling (Quality) instead of TSR at native resolution; Windows only |
| `-recordaudio=N` | Records the game's audio mix to `Saved/Recording/demo_audio.wav` for N seconds, then quits |

## What's where

| Path | Contents |
|---|---|
| `Source/PuzzleGame5x5` | The game module: rules, board, input, camera, cathedral environment, bot, and the UI (UMG built in C++) |
| `Source/MeshOptimizer`, `Source/ThirdParty` | meshoptimizer as an engine module; FastNoise2 (prebuilt static libraries) and SDL3 |
| `Plugins` | RealtimeMeshComponent and AMD FSR |
| `Content` | Map, materials, meshes, textures, audio and fonts |
| `RawAudio`, `RawTextures`, `RawMeshes`, `RawFonts` | Source files that the import scripts read |
| `Tools` | Python and PowerShell scripts that synthesized the audio, generated the textures, baked the pier mesh in Blender and built assets in the editor. `Tools/ArchitectureDoc` generates `Architecture.html` |
| `Architecture.html` | Interactive architecture page: class diagram with expandable members, one move traced through the code, the tools and libraries used, and why the board is 9×9 rather than 8×8 |

GitHub shows `Architecture.html` as source code. Open it in a browser from a clone, or download it first.

## Third-party code and assets

| Component | Version | License |
|---|---|---|
| RealtimeMeshComponent | 5.4 | MIT |
| AMD FSR plugin | 4.1.1 | MIT; the FidelityFX SDK binaries are under AMD's license (`Plugins/FSR/Source/fidelityfx-sdk/Kits/FidelityFX/docs/license.md`) |
| SDL3 | 3.4.16 | zlib |
| meshoptimizer | 1.3 | MIT |
| FastNoise2 | 1.1.1 | MIT |
| Cinzel Decorative and Lilita One fonts | | SIL Open Font License 1.1 |

Their license texts are included with them.

The content tools were Python (NumPy, SciPy, DawDreamer with Faust, pedalboard), Blender 5.2, ComfyUI with Stable Diffusion 1.5 (DreamShaper 8), DeepBump and FFmpeg. `Tools/audio_fx.py` also expects Voxengo's free "St Nicolaes Church" impulse response in `D:\UEDeps\IR`. That file may not be redistributed, so it is not in this repository.

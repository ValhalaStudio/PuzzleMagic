# Puzzle Magic

A gothic 8×8 block puzzle with a Lovecraftian edge, made in Unreal Engine 5.8 with C++. Drag pieces onto the board and build routes across the board in a candle-lit cathedral that darkens and flickers as your luck runs out. It is a landscape-only game for iPhone and is developed and played on Windows.

Demo recording (made with an earlier version of the game): [Demo/PuzzleMagic_Demo.mp4](Demo/PuzzleMagic_Demo.mp4)

## The game

Every tile carries a skeleton hand whose finger points up, right, down or left. Tiles come in black, purple, red and ash grey. Pieces come three at a time (plus a HOLD slot to keep one for later) from 12 shapes: a single tile, 2×1, 3×1 and 4×1 lines, a 2×2 square and a three-tile angle, each in every rotation. Each piece arrives with its hands already set, and they cannot be rotated.

- **Routes.** A tile at the edge of the board that points straight away from the side it touches is a route starter, and one that points straight at a side it touches is a route ender; only a starter can begin a route. A chain of tiles that each point at the next, from a starter to an ender on the far side, is a route, and the whole chain breaks apart. Several routes can clear at once. A route may also leave through a neighbouring side. A loop of tiles pointing round in a circle, or a chain that leaves through the side it started on, breaks apart too, but scores nothing. A lone tile pointing straight out of its own edge does nothing.
- **Score.** Each cleared route scores 10 and every tile in its sequence 2, with no exponents. Routes that share tiles each earn 5 more for every route cleared at the same time. Placing a piece scores nothing. The round ends when no piece can be placed, and your best score is saved.
- **Board size.** Any width and height from 4 to 8 (SELECT COURSE in the menu, or `-grid=WxH`). The default is 8×8.
- **Bonus tiles** (Play options, off by default). Tiles with no hand drop onto the board as your score grows: a carved pumpkin every 200 points (at most 2 on the board), and an outgoing (full magenta potion bottle) and incoming (empty bottle) pair every 600 (at most 1 of each), one spawn per scoring step. A tile lands where it will not score at once, at random, and only on a scoring spot when nothing else is free; it never spawns if that would end the game. A chain of hands that links one to a side of the board clears it, together with the chain: 25 points for a pumpkin and 50 for a bottle, plus 2 per tile of the chain. An outgoing bottle sends a chain out through any neighbour, an incoming one takes a chain arriving from any side, and a chain from an outgoing bottle to an incoming one is worth a flat 100. The pumpkin swells and bursts, the outgoing bottle empties and the incoming one fills.
- **Relics** (Play options, off by default). Switches on combos, shown as "TREAT! x4!" (clears on following moves multiply the route points; every closed circuit, called a "Trick!", costs one combo step), relics (Holy Light purges a 3×3 area and Reroll replaces the whole tray; both score nothing and cost luck) and luck, which lights the candles: as it runs out, the dark wakes.
- **Menus.** P or the MENU button opens a menu (Take over or Resume, Main menu). The main menu has PLAY, SELECT COURSE, PLAY OPTIONS, DEMO (the bot plays) and HOW TO PLAY. The course and options are saved. The move budget from earlier versions is switched off for the MVP.
- **Landscape only.** The interface scales with the screen height, menu cards shrink to fit small windows, the score sits in the top-right corner and the combo badge hangs between the candles.
- **Halloween.** A stream of lime-green liquid runs along every cleared route in its direction (magenta for potion chains, orange for pumpkin chains). Two steaming cauldrons of lime-green brew stand beside the board and bats fly around it.
- **Campaigns: arcade and tutorial.** ARCADE (challenges of escalating difficulty) and TUTORIAL (a step-by-step lesson) on the main menu play campaign files. A step sets a goal score and/or a number of routes to clear, an optional time limit, the board size, which relics (Holy Light, Reroll; either switches combos and luck on) and bonus tiles (pumpkin, full potion, empty potion) are in play, popup text before and after, a hint line, tiles already on the board, and a **curated tray**: buckets of pieces dealt in order instead of random ones. Besides the goal score and routes, a step can set the **starting charges** of Holy Light and Reroll (0 to 3, default 1 each), can be won by **using up every preset bucket**, and can preset **reroll buckets**: each Reroll deals the next one in order instead of random pieces (after the last, random or start over). A step is won when its goals are met and lost when time runs out, no piece fits, or the curated pieces run out; beating one opens the next, and progress is saved per campaign.
  - **Editor.** `Tools/CampaignEditor/campaign_editor.html` is a standalone editor: double-click it to open it in any browser (no install, works offline). Build steps, paint the starting board, design the curated buckets, and Open/Save the `.json` file.
  - **Files.** The game reads `arcade.json` and `tutorial.json` from a `Campaigns` folder next to the game (`PuzzleGame5x5/Campaigns/` in the packaged build, so a new file can be tried without rebuilding), then from the bundled `Content/Campaigns/`. The starter campaigns are in `Content/Campaigns/`.
  - **Testing.** In the game console, `PuzzleArcadeTest win|fail|time` tries a step's popups without playing it.
- **Frame cap.** Phones and tablets are capped at 30 frames a second and the lightweight scene fits its 3D resolution to hold that, which measured about a fifth less power while playing than running uncapped (on a 720x1640 phone: 383 mA against 491 mA). The debug command `PuzzlePerf <max fps> <resolution %> <automatic 1/0>` (and `PuzzleUiCard <screen>`, used by the device test scripts) changes it on a test build.
- **Themes** (Play options THEME button; saved; `-theme=desk|halloween|classic`). A table in `PuzzleStyle.h` holds one entry per theme, and changing it reloads the map. **Desk** (the default) has plain triangle tiles with the bright blue/green/red/purple palette, a graphite board and tray, and a quiet wooden desk with a mug, pencil and notebook; it is real geometry, so even the low-spec mode needs no backdrop picture (`Tools/build_desk_materials.py`). **Halloween** is the skeleton-hand tiles, potions, pumpkin, cauldrons, bats and route streams in the gothic cathedral. **Classic** is the Route puzzle MVP as before the Halloween theme: triangle tiles in the cathedral without cauldrons, bats or streams (backdrop `Tools/LiteArt/T_LiteBackdropClassic.png`, rendered with `-backdropcapture -theme=classic`). Adding a theme is one `EPuzzleTheme` value, one table entry and its scenery.
- **Graphics and screen** (Play options, PC only). GRAPHICS HIGH / LOW: Low switches the lightweight mobile scene on (a pre-rendered backdrop, flat sprites, no ink-outline or madness post-process, and resolution that drops by itself when the frame rate falls under 50); changing it reloads the map to the main menu. FULLSCREEN / WINDOWED switches between a borderless full-screen window and a 16:9 window. Both are saved. Phones and tablets are always Low.
- **Sound.** Play options has two sliders, MUSIC and SOUND EFFECTS, saved with the other options.
- **Phones and tablets.** On Android the game uses a lightweight scene so weak GPUs reach about 30 frames a second: the cathedral is one pre-rendered backdrop picture (made from the real scene with `-demo -grid=8x8 -backdropcapture`), the bats and steam are flat animated sprites, and the 3D render resolution adjusts itself while the interface stays sharp. A PC can try it with `-lite`. Run `python Tools/make_lite_sprites.py` and then `Tools/build_lite_assets.py` (editor closed) to rebuild its art.
- **Look and sound.** Lumen global illumination and reflections, virtual shadow maps and TSR. The chant and organ music, sound effects and ambience are synthesized and play through a cathedral convolution reverb.

## Requirements

- Windows 10 or 11 and a DirectX 12 GPU (developed on an AMD Radeon RX 6400, 4 GB).
- Unreal Engine 5.8. The `.bat` launchers find it through `UE_ROOT`, the Epic Launcher registry entry, a registered source build, or `C:\Program Files\Epic Games\UE_5.8` (see `Tools/FindUE.bat`).
- Visual Studio 2022 or its Build Tools with the C++ game development workload (built with MSVC 14.44).
- [Git LFS](https://git-lfs.com): assets, audio, textures and prebuilt libraries are stored in LFS.

## Get the code

```bat
git lfs install
git clone https://github.com/areklis/PuzzleMagic.git "D:\Unreal Projects\PuzzleMagic"
```

## Build and run

Open `PuzzleGame5x5.uproject` and let Unreal build the missing modules, or build from a terminal in the project folder:

```bat
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" PuzzleGame5x5Editor Win64 Development "-Project=%CD%\PuzzleGame5x5.uproject" -WaitMutex
```

| Launcher | What it does |
|---|---|
| `PlayGame.bat` | Starts the game in a 1280×720 landscape window at the main menu |
| `RunDemo.bat` | The bot plays while FFmpeg (`C:\ffmpeg\bin\ffmpeg.exe`) records the window to `DemoCapture.mp4` |

**Controls**: drag pieces from the tray, and drop one on HOLD to keep it. **P** opens the menu (where you take over from the demo bot), **R** retries, and (when the Relics option is on) **Esc** or right-click cancels Holy Light targeting. On Windows, gamepad input goes through SDL3.

### Command-line options

| Option | Effect |
|---|---|
| `-demo` | The bot plays |
| `-grid=WxH` | Board size, each side 4 to 8, for example `-grid=5x7` (default 8×8) |
| `-tutorial`, `-tutorialpage=N` | Shows the How to play pages over the menu (they open by themselves on first launch), at page N counting from 0. Ignored with `-demo` |
| `-dread=X` | Keeps the dread effects at least at X, 0 to 1 |
| `-recordaudio=N` | Records the game's audio mix to `Saved/Recording/demo_audio.wav` for N seconds, then quits |

## The demo build

A trimmed build for sharing: DEMO 1 (4x4 board, no bonus tiles, ends at 4000 points) and DEMO 2 (7x7, bonus tiles, ends at 6000), the tutorial,
Desk theme only, a Greek translation chosen by the system language. See the "demo build" notes in `CLAUDE.md` for how it is switched on and built
(`Tools/build_packages.ps1 -Demo` for Windows, `-DemoApk` for Android).

## What's where

| Path | Contents |
|---|---|
| `Source/PuzzleGame5x5` | The game module: rules, board, input, camera, cathedral environment, bot, and the UI (UMG built in C++) |
| `Source/MeshOptimizer`, `Source/ThirdParty` | meshoptimizer as an engine module; FastNoise2 (prebuilt static libraries) and SDL3 |
| `Plugins` | RealtimeMeshComponent |
| `Content` | Map, materials, meshes, textures, audio and fonts |
| `RawAudio`, `RawTextures`, `RawMeshes`, `RawFonts` | Source files that the import scripts read |
| `Tools` | Python and PowerShell scripts that synthesized the audio, generated the textures, baked the pier mesh in Blender and built assets in the editor (for example `build_route_tiles.py` and `build_bonus_tiles.py` make the tile materials). `Tools/ArchitectureDoc` generates `Architecture.html` |
| `Architecture.html` | Interactive architecture page: class diagram with expandable members, one move traced through the code, the tools and libraries used, and the board sizes |

GitHub shows `Architecture.html` as source code. Open it in a browser from a clone, or download it first.

## Third-party code and assets

| Component | Version | License |
|---|---|---|
| RealtimeMeshComponent | 5.4 | MIT |
| SDL3 | 3.4.16 | zlib |
| meshoptimizer | 1.3 | MIT |
| FastNoise2 | 1.1.1 | MIT |
| Cinzel Decorative and Lilita One fonts | | SIL Open Font License 1.1 |

Their license texts are included with them. The original code and assets in this repository are released under CC0 1.0 (see LICENSE); the third-party components above keep their own licenses.

The content tools were Python (NumPy, SciPy, DawDreamer with Faust, pedalboard), Blender 5.2, ComfyUI with Stable Diffusion 1.5 (DreamShaper 8), DeepBump and FFmpeg. `Tools/audio_fx.py` also expects Voxengo's free "St Nicolaes Church" impulse response in `D:\UEDeps\IR`. That file may not be redistributed, so it is not in this repository.

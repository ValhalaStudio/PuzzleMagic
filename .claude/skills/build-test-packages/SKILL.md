---
name: build-test-packages
description: Compile PuzzleMagic and build the Windows zip and the Android APK for testers. Use when asked to build, package, rebuild the test builds, or make the zip/APK.
---

# Build the test packages

Run `powershell -File Tools\build_packages.ps1` (add `-Win` or `-Android` for one platform, `-Ue <engine folder>` and `-Out <folder>` if
the defaults are wrong; `-DryRun` shows what it would do). It compiles the editor target, cooks and packages, retries a package once,
zips Windows with `Tools/package_windows.py` and copies the APK to `<Out>\PuzzleMagic_Android_test.apk`. Output goes to the folder
`PuzzleMagic_Builds` next to the repo unless `-Out` is given.

Before you start
- Nothing else may be using the engine: close the editor, any `-game` window and any other build. Two builds at once corrupt
  `Intermediate` (a corrupt `Definitions.PuzzleGame5x5.h` full of null characters is the sign; delete it).
- A build takes about 20 minutes for Windows and 20 for Android; run it in the background and wait for it to finish.
- Android needs the Android SDK, NDK `27.2.12479018` and JDK 21. The script finds JDK 21 itself (or take `-Jdk <folder>`) and does
  not trust an existing `JAVA_HOME`: an old Java 8 there makes the Gradle APK step fail ("This build uses a Java 8 JVM").

Known traps
- A fresh Windows PC refuses to run `.ps1` files ("running scripts is disabled on this system"), and the script then stops in its first
  second. Run it as `powershell -ExecutionPolicy Bypass -File Tools\build_packages.ps1 ...` (one run only), or let the user allow scripts
  for their own account (`Set-ExecutionPolicy RemoteSigned -Scope CurrentUser`); do not change the policy without asking. Windows build
  measured on a 12-core, 16 GB PC: compile 8 min, cook and package 15 min, zip 1 min.
- The Android cook sometimes ends with an engine ensure ("MTAccessDetector") after printing "Done!" and exits 25: it is the engine,
  not the code. The script retries; if both attempts fail, run it again.
- `Compress-Archive` writes backslash paths that other extractors reject; always zip with `Tools/package_windows.py`.
- Windows Development builds keep saves in the game folder, so a tester's zip must not contain a `Saved` folder (the packager removes it).
- Do not edit source while a package is being built.
- Report the exact file paths and sizes when done, and say which commit the build matches (no source newer than the build).

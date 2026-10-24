"""Zips a packaged Windows build for testers.

    python Tools/package_windows.py <WindowsPortable folder> <output.zip> [--demo]

With --demo the zip is the demo build: the same files plus PuzzleGame5x5/DemoBuild.txt (the game reads it at startup, see
PuzzleDemo.h), in a PuzzleMagic_Demo folder, without the campaign editor.

Uses Python's zipfile with forward-slash paths: Compress-Archive writes backslashes, which other extractors reject
("CreateProcess() returned 2"). The Development build keeps saves in the game folder, so a Saved folder left over from a test run is
dropped first. The campaign editor and the starter campaign files go in too. The demo zip also leaves out the debug symbols (.pdb)
and the Manifest_*.txt file lists.
"""
import os, shutil, sys, zipfile

src, out = sys.argv[1], sys.argv[2]
demo = "--demo" in sys.argv[3:]
# --launcher <exe>: replaces the root PuzzleGame5x5.exe (Tools/DemoLauncher builds one that starts the game with -NotInstalled, so a
# Shipping game saves next to itself instead of in AppData).
launcher = sys.argv[sys.argv.index("--launcher") + 1] if "--launcher" in sys.argv else None
top = "PuzzleMagic_Demo" if demo else "PuzzleMagic_Windows"
repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
shutil.rmtree(os.path.join(src, "PuzzleGame5x5", "Saved"), ignore_errors=True)
if os.path.exists(out):
    os.remove(out)
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for root, _, files in os.walk(src):
        for f in files:
            full = os.path.join(root, f)
            rel = os.path.relpath(full, src).replace(os.sep, "/")
            if demo and (f.lower().endswith(".pdb") or f.startswith("Manifest_")):
                continue  # debug symbols and the build's file lists are not for players
            if launcher and rel == "PuzzleGame5x5.exe":
                full = launcher
                rel = os.path.basename(launcher)  # the launcher is named for the game (PuzzleMagic.exe)
            z.write(full, top + "/" + rel)
    if demo:
        z.writestr(top + "/PuzzleGame5x5/DemoBuild.txt", "The game is a demo while this file sits here: 7x7 board, 5000 points, tutorial, Desk theme only.\n")
    else:
        z.write(os.path.join(repo, "Tools", "CampaignEditor", "campaign_editor.html"), top + "/CampaignEditor/campaign_editor.html")
        for name in ("arcade.json", "tutorial.json"):
            z.write(os.path.join(repo, "Content", "Campaigns", name), top + "/CampaignEditor/" + name)
with zipfile.ZipFile(out) as z:
    names = z.namelist()
    bad = [n for n in names if "\\" in n]
    print("%d files, %d with backslashes, integrity %s, %.0f MB" % (len(names), len(bad), "ok" if z.testzip() is None else "BROKEN", os.path.getsize(out) / 1e6))
    sys.exit(1 if bad or z.testzip() else 0)

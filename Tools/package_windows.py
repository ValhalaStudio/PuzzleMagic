"""Zips a packaged Windows build for testers.

    python Tools/package_windows.py <WindowsPortable folder> <output.zip>

Uses Python's zipfile with forward-slash paths: Compress-Archive writes backslashes, which other extractors reject
("CreateProcess() returned 2"). The Development build keeps saves in the game folder, so a Saved folder left over from a test run is
dropped first. The campaign editor and the starter campaign files go in too.
"""
import os, shutil, sys, zipfile

src, out = sys.argv[1], sys.argv[2]
repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
shutil.rmtree(os.path.join(src, "PuzzleGame5x5", "Saved"), ignore_errors=True)
if os.path.exists(out):
    os.remove(out)
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for root, _, files in os.walk(src):
        for f in files:
            full = os.path.join(root, f)
            z.write(full, "PuzzleMagic_Windows/" + os.path.relpath(full, src).replace(os.sep, "/"))
    z.write(os.path.join(repo, "Tools", "CampaignEditor", "campaign_editor.html"), "PuzzleMagic_Windows/CampaignEditor/campaign_editor.html")
    for name in ("arcade.json", "tutorial.json"):
        z.write(os.path.join(repo, "Content", "Campaigns", name), "PuzzleMagic_Windows/CampaignEditor/" + name)
with zipfile.ZipFile(out) as z:
    names = z.namelist()
    bad = [n for n in names if "\\" in n]
    print("%d files, %d with backslashes, integrity %s, %.0f MB" % (len(names), len(bad), "ok" if z.testzip() is None else "BROKEN", os.path.getsize(out) / 1e6))
    sys.exit(1 if bad or z.testzip() else 0)

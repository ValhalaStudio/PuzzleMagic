"""Writes the demo APK's patched copy of the engine's GameActivity.java.template.

    python Tools/make_demo_activity.py <engine folder> <project folder>

The demo APK asks for no ACCESS_NETWORK_STATE (it makes no network connections). The engine's GameActivity asks Android for the active
network in two places (getConnectionType and AndroidThunkJava_GetNetworkConnectionType); without the permission Android throws a
SecurityException there and the game aborts. This takes the engine's own template, wraps those two calls so a missing permission reads as
"no network", and writes the result to <project>/Intermediate/PuzzleDemo/GameActivity.java.template, which PuzzleDemo_UPL.xml hands to the
build (the gameActivityReplacement hook). The engine folder is not touched. It stops with an error when the engine's text is not the one
this expects (another engine version): look at the template again then.
"""
import os, sys

engine, project = sys.argv[1], sys.argv[2]
src = os.path.join(engine, "Engine", "Build", "Android", "Java", "src", "com", "epicgames", "unreal", "GameActivity.java.template")
dst_dir = os.path.join(project, "Intermediate", "PuzzleDemo")
text = open(src, encoding="utf-8", newline="").read()

old = "NetworkInfo activeNetwork = cm.getActiveNetworkInfo();"
new = ("NetworkInfo activeNetwork = null; // PUZZLE DEMO PATCH: no ACCESS_NETWORK_STATE in the demo, so a SecurityException means \"no network\"\n"
       "\t\ttry { activeNetwork = cm.getActiveNetworkInfo(); } catch (SecurityException e) { }")
count = text.count(old)
if count != 2:
    sys.exit("make_demo_activity.py: expected 2 calls of getActiveNetworkInfo() in GameActivity.java.template, found %d: check the engine version" % count)
if "\r\n" in text:
    new = new.replace("\n", "\r\n")
text = text.replace(old, new)

os.makedirs(dst_dir, exist_ok=True)
dst = os.path.join(dst_dir, "GameActivity.java.template")
open(dst, "w", encoding="utf-8", newline="").write(text)
print("wrote %s (%d call sites wrapped)" % (dst, count))

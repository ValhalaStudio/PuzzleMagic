---
name: ui-overlap-check
description: Check that text and buttons stay inside the cards and their frame graphics on every screen shape - PC windows shaped like a phone/tablet and real devices. Use when asked about UI overlap, text over borders, buttons bleeding out of boxes, or after changing a card or any text.
---

# UI overlap check

Cards are UMG widgets built in `Source/PuzzleGame5x5/Private/PuzzleHUDWidget.cpp` (`BuildCardContent`, `BuildTutorialPage`).
The card frame lines are about 830 units apart; text must stay inside them on any screen shape.

1. Device sweep (best): the `device-test` skill. `soak.py` takes a screenshot of every card in landscape and portrait on each device, and
   `analyze_ui.py` flags any card where text breaks the thin inner gold frame line. Then LOOK at the contact sheets
   (`sheet_land_*.png`, `sheet_port_*.png`): the detector can miss things such as buttons clipped by the card edge.
2. Quick PC check: run the game windowed (`UnrealEditor.exe <uproject> -game -windowed`), resize the window to 426x972 or 720x990
   (a phone), 800x1280 and 1280x800 (a tablet) and 1900x1040, open the cards (Select course, Play options, the How to play pages,
   Pause, the arcade and tutorial intros) and run `python Tools\AndroidTests\analyze_ui.py <screenshot or folder>`.
   Capture only the game window, never the whole desktop.

What caused overlap before (and the fixes already in the code)
- Wrapped text draws about 15% wider than its box: long body text wraps in a 640-unit box (`BodyWrapWidth`).
- Card titles in the wide decorative font draw about 25% wider than they measure on a phone: `MakeText` fits titles to 560 units, and
  `bFitWidth` does the same for single lines that come from campaign files.
- New text from a campaign file is the usual culprit; keep titles short or rely on the fit.
- Check the generated rules text (the intro card lists goals, relics and bonus tiles) at its longest, with every option on.

After a fix, rebuild and repeat on a device; do not call it fixed from one window size.

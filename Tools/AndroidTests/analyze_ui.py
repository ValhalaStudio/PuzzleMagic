"""Looks for interface overlap in screenshots of the game's cards: text running over the gold frame lines of the card, and
anything bright spilling past the card's outer rim. Also builds contact sheets so the shots can be checked by eye.

    python analyze_ui.py <folder with ui_*.png> [--debug file.png]
"""
import glob, os, sys
import numpy as np
from PIL import Image, ImageDraw


def masks(im):
    r, g, b = im[:, :, 0], im[:, :, 1], im[:, :, 2]
    gold = (r > 200) & (g > 140) & (g < 240) & ((r - b) > 80)
    white = (r > 215) & (g > 205) & (b > 225)           # the body text colour (0.9, 0.86, 1.0) after shading
    return gold, white


def card_box(gold):
    H, W = gold.shape
    S = min(W, H)
    rows = []
    for y in range(H):
        cur = best = 0
        for v in gold[y]:
            cur = cur + 1 if v else 0
            if cur > best:
                best = cur
        if best >= 0.25 * S:
            rows.append(y)
    if len(rows) < 6:
        return None
    return rows[0], rows[-1]


def clusters(cols):
    out = []
    for c in cols:
        if out and c - out[-1][-1] <= 2:
            out[-1].append(c)
        else:
            out.append([c])
    return out


def analyse(path):
    im = np.array(Image.open(path).convert("RGB")).astype(int)
    gold, white = masks(im)
    r, g, b = im[:, :, 0], im[:, :, 1], im[:, :, 2]
    # the thin inner frame line is a dimmer gold than the outer rim
    line = (r > 150) & (g > 110) & ((r - b) > 40)
    H, W = gold.shape
    box = card_box(gold)
    res = {"file": os.path.basename(path), "size": "%dx%d" % (W, H), "card": box is not None, "text_on_line": 0, "lines": 0, "where": []}
    if not box:
        return res, None
    t, b0 = box
    h = b0 - t
    y0, y1 = t + int(0.13 * h), b0 - int(0.13 * h)
    # the outer rim: bright gold columns
    frac = gold[y0:y1].mean(axis=0)
    cl = clusters([x for x in range(W) if frac[x] > 0.30])
    res["lines"] = len(cl)
    if len(cl) < 2:
        return res, (t, b0, cl)
    left_edge, right_edge = cl[0][-1], cl[-1][0]
    span = right_edge - left_edge
    broken_rows = set()
    for side in (0, 1):
        # look for the inner line within a band next to the rim
        lo, hi = (left_edge + 4, left_edge + int(0.12 * span)) if side == 0 else (right_edge - int(0.12 * span), right_edge - 4)
        counts = [(line[y0:y1, x].sum(), x) for x in range(max(lo, 0), min(hi, W))]
        if not counts:
            continue
        best, x = max(counts)
        if best < 0.5 * (y1 - y0):
            continue                      # no clear inner line on this card
        res["lines"] += 1
        # rows where that line is interrupted by something that is not line colour (text, its dark outline, white fill)
        col = line[y0:y1, x - 1:x + 2].any(axis=1)
        interrupted = np.where(~col)[0] + y0
        broken_rows.update(interrupted.tolist())
    res["text_on_line"] = len(broken_rows)
    if broken_rows:
        res["where"] = [min(broken_rows), max(broken_rows)]
    return res, (t, b0, cl)


def sheets(folder, results):
    files = sorted(glob.glob(os.path.join(folder, "ui_*.png")))
    groups = {}
    for f in files:
        key = os.path.basename(f).split("_")[1]  # land / port
        groups.setdefault(key, []).append(f)
    out = []
    for key, fs in groups.items():
        for chunk in range(0, len(fs), 8):
            part = fs[chunk:chunk + 8]
            ims = [Image.open(f).convert("RGB") for f in part]
            th = 640 if key.startswith("port") else 360
            tw = int(ims[0].width * th / ims[0].height)
            cols = 4
            rows = (len(ims) + cols - 1) // cols
            sheet = Image.new("RGB", (cols * tw, rows * (th + 18)), (20, 20, 20))
            d = ImageDraw.Draw(sheet)
            for i, (f, im) in enumerate(zip(part, ims)):
                x, y = (i % cols) * tw, (i // cols) * (th + 18)
                sheet.paste(im.resize((tw, th)), (x, y + 18))
                name = os.path.basename(f)[3:-4]
                bad = results.get(os.path.basename(f), {}).get("text_on_line", 0)
                d.text((x + 4, y + 3), name + ("  [overlap %d]" % bad if bad else ""), fill=(255, 120, 120) if bad else (230, 230, 230))
            p = os.path.join(folder, "sheet_%s_%d.png" % (key, chunk // 8 + 1))
            sheet.save(p)
            out.append(p)
    return out


def main():
    folder = sys.argv[1]
    files = sorted(glob.glob(os.path.join(folder, "ui_*.png"))) if os.path.isdir(folder) else [folder]
    results = {}
    print("%-36s %-10s %-5s %-6s %s" % ("shot", "size", "card", "lines", "text over frame line (rows)"))
    for f in files:
        r, _ = analyse(f)
        results[r["file"]] = r
        flag = "  <-- OVERLAP" if r["text_on_line"] > 6 else ""
        print("%-36s %-10s %-5s %-6s %d %s%s" % (r["file"], r["size"], "yes" if r["card"] else "no", r["lines"], r["text_on_line"], r["where"] or "", flag))
    if os.path.isdir(folder):
        for p in sheets(folder, results):
            print("sheet:", p)


main()

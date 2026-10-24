"""Power comparison on the phone: the same DEMO play with different frame caps / resolutions.

Run it with the phone UNPLUGGED. The phone will not let adb read the battery current, so this uses the battery's charge counter
(microamp-hours, in steps of about 5 mAh): while discharging, how fast it falls is what the phone uses. A straight line is fitted
to the readings of each run to smooth the steps. (A first attempt on the charger failed: the charger's own current drifts by
+-100 mA with temperature, as much as the differences being measured.)

    python power_test.py [minutes per run]
Settings are applied with the game's PuzzlePerf command (debug build): <max fps> <3D resolution %> <automatic resolution 1/0>.
"""
import datetime, json, os, re, subprocess, sys, time
import numpy as np
from PIL import Image

ADB = os.path.join(os.environ["LOCALAPPDATA"], "Android", "Sdk", "platform-tools", "adb.exe")
PHONE = os.environ.get("PM_DEVICE") or ""   # the adb serial; default: the only connected device
PKG = "com.valhallastudios.puzzlemagic"
ACT = PKG + "/com.epicgames.unreal.GameActivity"
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
BUILDS = os.environ.get("PM_BUILDS", os.path.join(os.path.dirname(REPO), "PuzzleMagic_Builds"))
APK = os.environ.get("PM_APK", os.path.join(BUILDS, "PuzzleMagic_Android_test.apk"))
OUTBASE = os.environ.get("PM_OUT", os.path.join(BUILDS, "AndroidTests"))
os.makedirs(OUTBASE, exist_ok=True)
if not PHONE:
    _o = subprocess.run([ADB, "devices"], capture_output=True).stdout.decode("utf-8", "replace")
    _d = [x.split()[0] for x in _o.splitlines()[1:] if x.strip() and x.split()[-1] == "device"]
    if len(_d) != 1:
        sys.exit("set PM_DEVICE to the adb serial (connected devices: %s)" % _d)
    PHONE = _d[0]
MIN = float(sys.argv[1]) if len(sys.argv) > 1 else 12.0
WARM = 150  # seconds after a change that are not counted
OUT = os.path.join(OUTBASE, "power_" + datetime.datetime.now().strftime("%Y%m%d_%H%M"))
os.makedirs(OUT, exist_ok=True)
CONFIGS = {  # name: (max fps, resolution %, automatic resolution)
    "A uncapped (as shipped)": (0, 70, 1),
    "B cap 30, automatic res": (30, 70, 1),
    "C cap 30, fixed 70%": (30, 70, 0),
}
ORDER = ["A uncapped (as shipped)", "B cap 30, automatic res", "C cap 30, fixed 70%",
         "C cap 30, fixed 70%", "B cap 30, automatic res", "A uncapped (as shipped)"]
LOG = open(os.path.join(OUT, "log.txt"), "a", encoding="utf-8")


def log(m):
    line = "[%s] %s" % (datetime.datetime.now().strftime("%H:%M:%S"), m)
    print(line, flush=True)
    LOG.write(line + "\n"); LOG.flush()


def sh(*a, timeout=120, binary=False):
    try:
        r = subprocess.run([ADB, "-s", PHONE] + list(a), capture_output=True, timeout=timeout)
        return r.stdout if binary else r.stdout.decode("utf-8", "replace").strip()
    except Exception as e:
        return b"" if binary else "ERR " + str(e)


def battery():
    o = sh("shell", "dumpsys", "battery")
    g = lambda k: (re.search(k + r":\s*(-?\d+)", o) or [None, None])[1]
    return {"counter": int(g("Charge counter") or 0), "level": int(g("level") or 0), "temp": int(g("temperature") or 0) / 10.0,
            "ac": "AC powered: true" in o, "usb": "USB powered: true" in o}


def fps():
    raw = sh("shell", "dumpsys", "SurfaceFlinger", "--list")
    for name in dict.fromkeys(re.findall(r"SurfaceView\[com\.valhallastudios\.puzzlemagic/com\.epicgames\.unreal\.GameActivity\][^\s}]*", raw)):
        out = sh("shell", "dumpsys", "SurfaceFlinger", "--latency", '"%s"' % name)
        ts = [int(l.split()[1]) for l in out.splitlines()[1:] if len(l.split()) == 3 and l.split()[1].isdigit() and 0 < int(l.split()[1]) < 9e18]
        if len(ts) >= 30:
            return (len(ts) - 1) / ((ts[-1] - ts[0]) / 1e9)
    return None


def shot(name):
    d = sh("exec-out", "screencap", "-p", binary=True)
    if d and len(d) > 1000:
        p = os.path.join(OUT, name + ".png")
        open(p, "wb").write(d)
        return p


def card(path):
    im = np.array(Image.open(path).convert("RGB")).astype(int)
    H, W, _ = im.shape
    S = min(W, H)
    gold = (im[:, :, 0] > 200) & (im[:, :, 1] > 140) & (im[:, :, 1] < 240) & ((im[:, :, 0] - im[:, :, 2]) > 80)
    rows = [y for y in range(H) if (lambda r: max((len(s) for s in "".join("1" if v else "0" for v in r).split("0")), default=0))(gold[y]) >= 0.25 * S]
    return (W, H, rows[0], rows[-1]) if len(rows) > 5 else None


def awake():
    return "mWakefulness=Awake" in sh("shell", "dumpsys", "power")


def lit(path):
    try:
        return float(np.array(Image.open(path).convert("L")).mean()) > 8.0
    except Exception:
        return False


def ensure_awake():
    """Wakes the screen, dismisses the lock screen and checks that something is really being displayed."""
    for _ in range(4):
        sh("shell", "input", "keyevent", "KEYCODE_WAKEUP"); time.sleep(1.5)
        sh("shell", "wm", "dismiss-keyguard"); time.sleep(1.0)
        sh("shell", "input", "swipe", "360", "1400", "360", "400", "250"); time.sleep(1.5)
        p = shot("awake_check")
        if awake() and p and lit(p):
            return True
    log("PROBLEM: could not wake the screen")
    return False


def board_visible(path):
    """True when there are board tiles (red/purple) in the middle of the screen."""
    a = np.array(Image.open(path).convert("RGB")).astype(int)
    H, W, _ = a.shape
    mid = a[int(H * 0.2):int(H * 0.85), int(W * 0.25):int(W * 0.75)]
    r, g, b = mid[:, :, 0], mid[:, :, 1], mid[:, :, 2]
    coloured = ((r > 120) & (g < 70) & (b < 90)) | ((b > 110) & (r > 60) & (g < 70))
    return float(coloured.mean()) > 0.01


def start_demo():
    sh("shell", "am", "force-stop", PKG); time.sleep(2)
    sh("shell", "setprop", "debug.puzzle.exec", "''")   # a stale command would run again when the game starts
    if not ensure_awake():
        return False
    sh("shell", "am", "start", "-n", ACT); time.sleep(50)
    p = shot("menu")
    c = card(p) if p else None
    if not c:
        log("PROBLEM: menu card not found"); return False
    W, H, t, b = c
    sh("shell", "input", "tap", str(W // 2), str(int(t + (b - t) * 0.785)))
    time.sleep(6)
    return bool(sh("shell", "pidof", PKG).strip())


def perf(cap, res, adapt):
    sh("shell", "setprop", "debug.puzzle.exec", "'PuzzlePerf %d %d %d'" % (cap, res, adapt))
    time.sleep(3)
    sh("shell", "setprop", "debug.puzzle.exec", "''")


def measure(label, seconds, with_fps):
    """Samples the battery every 15 s; returns the fitted rate in mA (positive = battery filling) and extras."""
    t0 = time.time(); xs, ys, temps, fs = [], [], [], []
    plugged = True
    while time.time() - t0 < seconds:
        b = battery()
        xs.append(time.time() - t0); ys.append(b["counter"] / 1000.0); temps.append(b["temp"])
        plugged = plugged and (b["ac"] or b["usb"])
        if b["ac"] or b["usb"]:
            raise RuntimeError("charger connected during a measurement")
        if b["level"] < 20:
            raise RuntimeError("battery under 20%")
        if with_fps and len(xs) % 4 == 0:
            f = fps()
            if f: fs.append(f)
        time.sleep(15)
    xs, ys = np.array(xs), np.array(ys)
    slope = np.polyfit(xs, ys, 1)[0] * 3600.0   # mAh per second -> mA
    return {"rate_mA": slope, "temp": float(np.mean(temps)), "fps": float(np.median(fs)) if fs else None, "plugged": plugged, "n": len(xs)}


def main():
    orig = {k: sh("shell", "settings", "get", "system", k) for k in ("screen_brightness", "screen_brightness_mode", "screen_off_timeout")}
    json.dump(orig, open(os.path.join(OUT, "original_settings.json"), "w"))
    log("original settings %s" % orig)
    results = []
    try:
        sh("shell", "settings", "put", "system", "screen_brightness_mode", "0")
        sh("shell", "settings", "put", "system", "screen_brightness", "412")
        sh("shell", "settings", "put", "system", "screen_off_timeout", "14400000")
        sh("shell", "am", "force-stop", PKG)
        log("install: " + sh("install", "-r", APK, timeout=600)[-40:])
        b = battery(); log("start battery %s" % b)
        if b["ac"] or b["usb"]:
            log("PROBLEM: the phone is on a charger; this method needs it unplugged")
            return
        if "isKeyguardShowing=true" in sh("shell", "dumpsys", "window"):
            log("PROBLEM: the phone is locked; unlock it first (the test never touches the lock screen)")
            return
        if b["level"] < 30:
            log("PROBLEM: battery under 30%, too low for the test")
            return
        # reference: screen on at the test brightness, game closed (home screen). The screen is never put to sleep: the phone has a secure lock screen that adb cannot open.
        if not ensure_awake():
            return
        sh("shell", "input", "keyevent", "KEYCODE_HOME"); time.sleep(20)
        log("screen is on for the home-screen reference: %s" % awake())
        r_home = measure("home screen", 360, False); log("screen on, game closed: %s" % r_home)
        for i, name in enumerate(ORDER, 1):
            cap, res, adapt = CONFIGS[name]
            if i == 1 or ORDER[i - 2] != name:
                if i == 1 or True:
                    pass
            if not start_demo():
                log("PROBLEM: game did not start for run %d" % i); continue
            perf(cap, res, adapt)
            log("run %d: %s" % (i, name)); time.sleep(WARM)
            chk = shot("run%d_start" % i)
            if chk and not board_visible(chk):
                log("PROBLEM: no board in view at the start of run %d" % i)
            r = measure(name, MIN * 60, True)
            r["name"] = name; r["run"] = i
            shot("run%d_end" % i)
            log("run %d result: %s" % (i, r)); results.append(r)
        sh("shell", "am", "force-stop", PKG)
        # discharging: the counter's fall is the phone's own use (battery side)
        use_home = -r_home["rate_mA"]
        report = ["# Power test (unplugged), phone 25028RN03Y (5080 mAh), %s" % os.path.basename(OUT), "",
                  "Screen on at the test brightness, game closed: about %.0f mA." % use_home, "",
                  "| setting | runs | fps (median) | phone uses mA | of which the game mA | watts at 3.85 V | hours on a full battery | temp C |",
                  "|---|---|---|---|---|---|---|---|"]
        for name in CONFIGS:
            rs = [r for r in results if r["name"] == name]
            if not rs:
                continue
            use = -np.mean([r["rate_mA"] for r in rs])
            fp = [r["fps"] for r in rs if r["fps"]]
            report.append("| %s | %d | %s | %.0f | %.0f | %.2f | %.1f | %.1f |" % (name, len(rs), ("%.1f" % np.mean(fp)) if fp else "n/a", use, use - use_home, use * 3.85 / 1000.0, 5080.0 / use, np.mean([r["temp"] for r in rs])))
        report += ["", "Per run: " + json.dumps([{k: (round(v, 1) if isinstance(v, float) else v) for k, v in r.items()} for r in results])]
        open(os.path.join(OUT, "report.md"), "w", encoding="utf-8").write("\n".join(report) + "\n")
        log("report written")
    finally:
        sh("shell", "am", "force-stop", PKG)
        for k, v in orig.items():
            if v and v != "null":
                sh("shell", "settings", "put", "system", k, v)
        log("restored settings, game closed: %s" % (not sh("shell", "pidof", PKG).strip()))


main()

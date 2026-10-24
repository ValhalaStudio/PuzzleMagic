"""Unattended test run of the Android build on every connected device (one thread per device).

Each cycle on a device: restart the game, screenshot the menu, open the options card, flip Landscape/Portrait, screenshot,
close the card, start a game, screenshot the HUD, restart, then let the DEMO bot play for a few minutes while sampling frame rate,
memory and whether the game is still alive. The log is scanned for crashes and ANRs. Everything is written to
AndroidTests/run_<time>/<device>/ with a report.md. The game is force-stopped at the end and the settings this changes (screen
timeout, saved orientation) are put back.

    python soak.py [hours] [serial ...]
"""
import os, re, subprocess, sys, threading, time, datetime, json
import numpy as np
from PIL import Image

ADB = os.path.join(os.environ["LOCALAPPDATA"], "Android", "Sdk", "platform-tools", "adb.exe")
PKG = "com.valhallastudios.puzzlemagic"
ACT = PKG + "/com.epicgames.unreal.GameActivity"
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
# Builds and test output live outside the repo: PM_BUILDS, default the folder PuzzleMagic_Builds next to the repo.
BUILDS = os.environ.get("PM_BUILDS", os.path.join(os.path.dirname(REPO), "PuzzleMagic_Builds"))
APK = os.environ.get("PM_APK", os.path.join(BUILDS, "PuzzleMagic_Android_test.apk"))
OUTBASE = os.environ.get("PM_OUT", os.path.join(BUILDS, "AndroidTests"))
os.makedirs(OUTBASE, exist_ok=True)
HOURS = float(sys.argv[1]) if len(sys.argv) > 1 else 3.0


def connected_devices():
    out = subprocess.run([ADB, "devices"], capture_output=True).stdout.decode("utf-8", "replace")
    return [l.split()[0] for l in out.splitlines()[1:] if l.strip() and l.split()[-1] == "device"]


SERIALS = sys.argv[2:] or connected_devices()   # default: every connected device
DEADLINE = time.time() + HOURS * 3600
RUN = os.path.join(OUTBASE, "run_" + datetime.datetime.now().strftime("%Y%m%d_%H%M"))
os.makedirs(RUN, exist_ok=True)

# Where things are, as a fraction of the golden card's height (measured on a 1280x800 and an 800x1280 screen).
MENU = {"play": 0.348, "arcade": 0.503, "options": 0.718, "demo": 0.785}
OPTS = {"orient": 0.506, "back": 0.910}


def sh(serial, *args, timeout=120, binary=False):
    try:
        r = subprocess.run([ADB, "-s", serial] + list(args), capture_output=True, timeout=timeout)
        return r.stdout if binary else r.stdout.decode("utf-8", "replace").strip()
    except Exception as e:
        return b"" if binary else "ERR " + str(e)


class Dev:
    def __init__(self, serial):
        self.s = serial
        self.dir = None
        self.lines = []
        self.events = []      # problems worth reporting
        self.fps = {"landscape": [], "portrait": []}
        self.mem = []
        self.cycles = 0
        self.shots = 0
        self.orient = "landscape"
        self.restore = {}
        self.nonce = 0
        self.ui_n = 0

    # ---- logging
    def log(self, msg):
        line = "[%s] %s" % (datetime.datetime.now().strftime("%H:%M:%S"), msg)
        self.lines.append(line)
        with open(os.path.join(self.dir, "log.txt"), "a", encoding="utf-8") as f:
            f.write(line + "\n")

    def problem(self, msg):
        self.events.append(msg)
        self.log("PROBLEM: " + msg)

    # ---- helpers
    def adb(self, *a, **k):
        return sh(self.s, *a, **k)

    def online(self):
        out = subprocess.run([ADB, "devices"], capture_output=True).stdout.decode("utf-8", "replace")
        return any(l.startswith(self.s) and l.split()[-1] == "device" for l in out.splitlines())

    def wait_online(self, minutes=10):
        t = time.time() + minutes * 60
        while time.time() < t:
            if self.online():
                return True
            time.sleep(15)
        return False

    def wake(self):
        self.adb("shell", "input", "keyevent", "KEYCODE_WAKEUP")
        self.adb("shell", "wm", "dismiss-keyguard")

    def shot(self, name):
        data = self.adb("exec-out", "screencap", "-p", binary=True)
        path = os.path.join(self.dir, name + ".png")
        if data and len(data) > 1000:
            with open(path, "wb") as f:
                f.write(data)
            self.shots += 1
            return path
        self.problem("screenshot failed: " + name)
        return None

    def size(self, path):
        try:
            return Image.open(path).size
        except Exception:
            return (0, 0)

    def card(self, path):
        """Top and bottom row of the golden card rim, or None."""
        im = np.array(Image.open(path).convert("RGB")).astype(int)
        H, W, _ = im.shape
        S = min(W, H)
        # the rim is a warm gold; phones render it a little lighter than the tablet, so the test is loose on blue
        gold = (im[:, :, 0] > 200) & (im[:, :, 1] > 140) & (im[:, :, 1] < 240) & ((im[:, :, 0] - im[:, :, 2]) > 80)
        rows = []
        for y in range(H):
            best = cur = 0
            for v in gold[y]:
                cur = cur + 1 if v else 0
                best = max(best, cur)
            if best >= 0.25 * S:
                rows.append(y)
        return (W, H, rows[0], rows[-1]) if len(rows) > 5 else None

    def tap(self, x, y):
        self.adb("shell", "input", "tap", str(int(x)), str(int(y)))

    def tap_card(self, path, frac):
        c = self.card(path)
        if not c:
            self.problem("no card found to tap (%.3f) in %s" % (frac, os.path.basename(path or "?")))
            return False
        W, H, t, b = c
        self.tap(W / 2, t + (b - t) * frac)
        return True

    def alive(self):
        return bool(self.adb("shell", "pidof", PKG).strip())

    def launch(self):
        self.adb("shell", "am", "force-stop", PKG)
        time.sleep(2)
        self.adb("logcat", "-c")
        # the game runs whatever is stored in debug.puzzle.exec as soon as it starts, so a stale command must not be left there
        self.adb("shell", "setprop", "debug.puzzle.exec", "''")
        self.wake()
        self.adb("shell", "am", "start", "-n", ACT)
        time.sleep(50)
        if not self.alive():
            self.problem("game not running after launch")
            return False
        return True

    def stop(self):
        self.adb("shell", "am", "force-stop", PKG)

    def fps_sample(self):
        raw = self.adb("shell", "dumpsys", "SurfaceFlinger", "--list")
        names = re.findall(r"SurfaceView\[com\.valhallastudios\.puzzlemagic/com\.epicgames\.unreal\.GameActivity\][^\s}]*", raw)
        names += [l.strip() for l in raw.splitlines() if "GameActivity" in l and "SurfaceView" in l and "{" not in l]
        for name in dict.fromkeys(names):
            out = self.adb("shell", "dumpsys", "SurfaceFlinger", "--latency", '"%s"' % name)
            ts = []
            for line in out.splitlines()[1:]:
                p = line.split()
                if len(p) == 3 and p[1].isdigit() and 0 < int(p[1]) < 9e18:
                    ts.append(int(p[1]))
            if len(ts) >= 30:
                return (len(ts) - 1) / ((ts[-1] - ts[0]) / 1e9)
        return None

    def mem_sample(self):
        out = self.adb("shell", "dumpsys", "meminfo", PKG)
        m = re.search(r"TOTAL PSS:\s+(\d+)", out) or re.search(r"TOTAL\s+(\d+)", out)
        return int(m.group(1)) // 1024 if m else None

    def crash_scan(self, label):
        out = self.adb("logcat", "-d", "-v", "brief", timeout=180)
        hits = [l for l in out.splitlines() if re.search(r"FATAL EXCEPTION|ANR in|Fatal signal|SIGSEGV|SIGABRT|Process: " + re.escape(PKG) + r".*died", l) and (PKG in l or "Fatal signal" in l or "FATAL" in l)]
        if hits:
            self.problem("%s: %d crash/ANR lines, first: %s" % (label, len(hits), hits[0][:200]))
            with open(os.path.join(self.dir, "crash_%s.txt" % label), "w", encoding="utf-8") as f:
                f.write(out[-60000:])

    def battery(self):
        out = self.adb("shell", "dumpsys", "battery")
        m = re.search(r"level:\s*(\d+)", out)
        return int(m.group(1)) if m else 100

    # ---- the interface sweep: every card, in both orientations, for the overlap check
    def uicmd(self, what, a=0, b=0, dwell=3.0):
        self.nonce += 1
        self.adb("shell", "setprop", "debug.puzzle.exec", "'PuzzleUiCard %s %d %d %d'" % (what, a, b, self.nonce))
        time.sleep(dwell)
        self.adb("shell", "setprop", "debug.puzzle.exec", "''")

    def uishot(self, tag):
        self.ui_n += 1
        return self.shot("ui_%s_%02d_%s" % (self.orient[:4], self.ui_n, tag))

    def option_is_on(self, path, frac):
        """Is the options-card button at this height green (on) or red (off)? Counts fill colours in a band at the card's middle."""
        c = self.card(path)
        if not c:
            return None
        W, H, top, bot = c
        y = int(top + (bot - top) * frac)
        im = np.array(Image.open(path).convert("RGB")).astype(int)
        half = int(0.14 * min(W, H))
        band = im[max(0, y - 4):y + 5, W // 2 - half:W // 2 + half]
        r, g, b = band[:, :, 0], band[:, :, 1], band[:, :, 2]
        green = int(((g > r + 35) & (g > 100)).sum())
        red = int(((r > g + 60) & (r > 130)).sum())
        return None if green + red < 20 else green > red

    def set_play_options(self, relics, bonus):
        """Sets the two Play options on the options card; returns what they were before (or None if it could not tell)."""
        self.uicmd("options", dwell=3)
        p = self.shot("opts_probe")
        if not p:
            return None
        was = (self.option_is_on(p, 0.209), self.option_is_on(p, 0.363))
        if None in was:
            self.problem("could not read the Play options buttons")
            return None
        for frac, now, want in ((0.209, was[0], relics), (0.363, was[1], bonus)):
            if now != want:
                self.tap_card(p, frac)
                time.sleep(2)
                p = self.shot("opts_probe") or p
        self.log("play options %s -> %s" % (was, (relics, bonus)))
        return was

    def sweep_pass(self):
        self.ui_n = 0
        self.log("UI sweep, %s" % self.orient)
        self.uicmd("menu", dwell=4);                self.uishot("menu")
        self.uicmd("courses");                      self.uishot("courses")
        self.uicmd("options");                      self.uishot("options")
        self.uicmd("pause");                        self.uishot("pause")
        self.uicmd("gameover");                     self.uishot("gameover")
        # the other How to Play pages only exist with Relics and Bonus Tiles on (they are off by default on a fresh install)
        before = self.set_play_options(True, True)
        for page in range(5):
            self.uicmd("howto", page);              self.uishot("howto%d" % (page + 1))
        if before is not None:
            self.set_play_options(*before)
        for kind, steps, label in ((0, 5, "arcade"), (1, 4, "tutorial")):
            self.uicmd("arcademenu", kind);         self.uishot("%s_menu" % label)
            for s in range(steps):
                self.uicmd("arcadeintro", kind, s); self.uishot("%s_intro%d" % (label, s + 1))
                self.uicmd("arcadebegin", dwell=2); self.uishot("%s_play%d" % (label, s + 1))
                self.uicmd("arcadewin", dwell=9);   self.uishot("%s_outro%d" % (label, s + 1))
            self.uicmd("arcadeintro", kind, 0)
            self.uicmd("arcadebegin", dwell=2)
            self.uicmd("arcadefail", dwell=4);      self.uishot("%s_fail" % label)
            self.uicmd("arcadedone", kind);         self.uishot("%s_done" % label)
        self.uicmd("game", dwell=6);                self.uishot("game")
        self.uicmd("menu", dwell=3)

    def flip_orientation(self):
        self.uicmd("options", dwell=3)
        p = self.shot("flip_options")
        if p and self.tap_card(p, OPTS["orient"]):
            time.sleep(8)
            p2 = self.shot("flip_after")
            W, H = self.size(p2) if p2 else (0, 0)
            now = "portrait" if H > W else "landscape"
            if now == self.orient:
                self.problem("orientation toggle did not change the screen")
            self.orient = now
            return True
        return False

    def sweep(self):
        if not self.launch():
            return
        p = self.shot("sweep_start")
        W, H = self.size(p) if p else (0, 0)
        self.orient = "portrait" if H > W else "landscape"
        self.sweep_pass()
        if self.flip_orientation():
            time.sleep(3)
            self.sweep_pass()
            self.flip_orientation()   # back to where it started
        self.stop()

    # ---- the test
    def setup(self):
        self.dir = os.path.join(RUN, re.sub(r"[^A-Za-z0-9_-]", "_", self.s)[:40])
        os.makedirs(self.dir, exist_ok=True)
        if not self.wait_online(2):
            self.problem("device offline at the start")
            return False
        info = {k: self.adb("shell", "getprop", v) for k, v in
                {"model": "ro.product.model", "android": "ro.build.version.release", "abi": "ro.product.cpu.abi"}.items()}
        info["size"] = self.adb("shell", "wm", "size")
        info["density"] = self.adb("shell", "wm", "density")
        info["battery"] = self.battery()
        self.log("device " + json.dumps(info))
        self.info = info
        orig_file = os.path.join(OUTBASE, "original_settings.json")   # optional {serial: screen_off_timeout}
        known = json.load(open(orig_file)) if os.path.exists(orig_file) else {}
        self.restore["timeout"] = known.get(self.s) or self.adb("shell", "settings", "get", "system", "screen_off_timeout")
        self.adb("shell", "settings", "put", "system", "screen_off_timeout", "14400000")
        self.wake()
        self.log("installing " + APK)
        out = self.adb("install", "-r", APK, timeout=600)
        self.log("install: " + out[-80:])
        if "Success" not in out:
            self.problem("install failed: " + out[-200:])
            return False
        return True

    def cycle(self, n):
        self.log("--- cycle %d" % n)
        if not self.wait_online():
            self.problem("device offline, skipping cycle %d" % n)
            return
        if not self.launch():
            self.crash_scan("c%d_launch" % n)
            return
        p = self.shot("c%02d_1_menu" % n)
        W, H = self.size(p) if p else (0, 0)
        self.orient = "portrait" if H > W else "landscape"
        self.log("menu %dx%d (%s)" % (W, H, self.orient))
        # options card, flip the orientation, back
        if p and self.tap_card(p, MENU["options"]):
            time.sleep(3)
            p2 = self.shot("c%02d_2_options" % n)
            if p2 and self.tap_card(p2, OPTS["orient"]):
                time.sleep(8)
                p3 = self.shot("c%02d_3_options_flipped" % n)
                W2, H2 = self.size(p3) if p3 else (0, 0)
                now = "portrait" if H2 > W2 else "landscape"
                self.log("after flip: %dx%d (%s)" % (W2, H2, now))
                if now == self.orient:
                    self.problem("orientation toggle did not change the screen (%s)" % now)
                self.orient = now
                if p3 and self.tap_card(p3, OPTS["back"]):
                    time.sleep(3)
                    self.shot("c%02d_4_menu_after" % n)
        # a game: the HUD at rest
        p4 = self.shot("c%02d_5_menu_for_play" % n)
        if p4 and self.tap_card(p4, MENU["play"]):
            time.sleep(6)
            self.shot("c%02d_6_game" % n)
            f = self.fps_sample()
            self.log("idle game fps %s" % (None if f is None else round(f, 1)))
        if not self.alive():
            self.problem("game died during the menu steps")
            self.crash_scan("c%d_menus" % n)
            return
        # restart (checks the saved orientation survives), then the DEMO soak
        self.stop()
        time.sleep(2)
        if not self.launch():
            self.crash_scan("c%d_relaunch" % n)
            return
        p5 = self.shot("c%02d_7_relaunch_menu" % n)
        W3, H3 = self.size(p5) if p5 else (0, 0)
        back = "portrait" if H3 > W3 else "landscape"
        self.log("relaunch %dx%d (%s)" % (W3, H3, back))
        if back != self.orient:
            self.problem("orientation not kept over a restart: %s -> %s" % (self.orient, back))
            self.orient = back
        if p5 and self.tap_card(p5, MENU["demo"]):
            time.sleep(5)
            soak_min = 12
            for m in range(soak_min):
                time.sleep(60)
                if time.time() > DEADLINE or self.battery() < 15:
                    break
                self.wake()
                if not self.alive():
                    self.problem("game died during the demo soak at minute %d" % m)
                    break
                f = self.fps_sample()
                mem = self.mem_sample()
                if f is not None:
                    self.fps[self.orient].append(round(f, 1))
                if mem:
                    self.mem.append((int(time.time()), mem))
                if m in (0, 4, 8, 11):
                    self.shot("c%02d_8_demo_min%02d" % (n, m))
                self.log("demo min %d: fps %s mem %s MB" % (m, None if f is None else round(f, 1), mem))
        self.crash_scan("c%d" % n)
        self.cycles += 1

    def finish(self):
        # put the saved orientation back to landscape (as found) and close the game
        try:
            if self.orient == "portrait" and self.wait_online(2) and self.launch():
                p = self.shot("final_menu")
                if p and self.tap_card(p, MENU["options"]):
                    time.sleep(3)
                    p2 = self.shot("final_options")
                    if p2 and self.tap_card(p2, OPTS["orient"]):
                        time.sleep(8)
                        self.shot("final_after_flip")
        finally:
            self.stop()
            time.sleep(2)
            self.log("game running at the end: %s" % self.alive())
            if self.restore.get("timeout"):
                self.adb("shell", "settings", "put", "system", "screen_off_timeout", self.restore["timeout"])
            self.log("restored screen_off_timeout=%s" % self.restore.get("timeout"))

    def run(self):
        if not self.setup():
            self.finish() if self.dir else None
            return
        n = 0
        try:
            if os.environ.get("NO_SWEEP") != "1":
                self.sweep()
            while time.time() < DEADLINE - 15 * 60:
                if self.battery() < 15:
                    self.problem("battery below 15%, stopping early")
                    break
                n += 1
                self.cycle(n)
        except Exception as e:
            self.problem("harness error: %r" % e)
        finally:
            self.finish()

    def report(self):
        def stats(v):
            return "n/a" if not v else "min %.1f / median %.1f / max %.1f (%d samples)" % (min(v), sorted(v)[len(v) // 2], max(v), len(v))
        lines = ["## %s" % self.s, "", "- device: %s" % json.dumps(self.info if hasattr(self, "info") else {}),
                 "- cycles completed: %d, screenshots: %d" % (self.cycles, self.shots),
                 "- fps landscape: " + stats(self.fps["landscape"]), "- fps portrait: " + stats(self.fps["portrait"])]
        if self.mem:
            lines.append("- memory (PSS MB): first %d, last %d, max %d" % (self.mem[0][1], self.mem[-1][1], max(m for _, m in self.mem)))
        lines.append("- problems: %d" % len(self.events))
        lines += ["  - " + e for e in self.events]
        return "\n".join(lines) + "\n"


def main():
    devs = [Dev(s) for s in SERIALS]
    threads = [threading.Thread(target=d.run) for d in devs]
    start = time.time()
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    with open(os.path.join(RUN, "report.md"), "w", encoding="utf-8") as f:
        f.write("# Android soak run %s (%.1f h planned, %.1f h taken)\n\n" % (os.path.basename(RUN), HOURS, (time.time() - start) / 3600))
        for d in devs:
            f.write(d.report() + "\n")
    print("done", RUN)


main()

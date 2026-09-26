#!/usr/bin/env python3
"""Generates Architecture.html in the project root: the interactive architecture guide for Puzzle Magic.

What comes from the code (so the page can't drift from it):
  * every class, struct, enum and namespace in Source/PuzzleGame5x5/Public, with members, signatures,
    UPROPERTY/UFUNCTION specifiers, section headings and doc comments (a small C++ header parser);
  * every relationship arrow is checked against the .cpp line(s) that make it true: if a call or a
    spawn disappears from the code, generation fails and names the stale arrow;
  * tunable constants, launch flags, the number of piece shapes and quest levels, line counts.
What is written here by hand: lane assignment, one-line class summaries, relationship labels, and the
prose/tables about tools and decisions.

Run after changing the code:   python Tools/ArchitectureDoc/gen_architecture.py
Standard library only (Python 3.8+).
"""
import datetime
import glob
import html
import json
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.abspath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(PROJ, "Source", "PuzzleGame5x5")
PUBLIC = os.path.join(SRC, "Public")
PRIVATE = os.path.join(SRC, "Private")
TEMPLATE = os.path.join(HERE, "template.html")
OUT = os.path.join(PROJ, "Architecture.html")


def esc(text):
    return html.escape(str(text), quote=True)


def rel(path):
    return os.path.relpath(path, PROJ).replace(os.sep, "/")


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def fail(message):
    print("gen_architecture: ERROR: " + message, file=sys.stderr)
    sys.exit(1)


# =====================================================================================
# C++ header parser (enough C++ for Unreal-style headers: classes, structs, enums,
# namespaces, UPROPERTY/UFUNCTION macros, inline bodies, initializers, delegates).
# =====================================================================================
ACCESS = ("public", "protected", "private")
FLAG_WORDS = ("static", "virtual", "inline", "constexpr", "explicit", "FORCEINLINE", "mutable")
UE_MACRO = re.compile(r"^(UCLASS|USTRUCT|UENUM|UPROPERTY|UFUNCTION|UINTERFACE|GENERATED_BODY|GENERATED_UCLASS_BODY)\s*\(")


def split_comment(line):
    """(code, comment) for one line; ignores // inside string literals."""
    quote = None
    i = 0
    while i < len(line):
        c = line[i]
        if quote:
            if c == "\\":
                i += 2
                continue
            if c == quote:
                quote = None
        elif c in "\"'":
            quote = c
        elif c == "/" and line[i + 1:i + 2] == "/":
            return line[:i], line[i + 2:].strip()
        i += 1
    return line, None


def find_top(s, target):
    """Index of the first `target` bracket/char at nesting depth 0 (<> () [] {} aware), or -1."""
    depth = angle = 0
    for i, c in enumerate(s):
        if c in "([{":
            if c == target and depth == 0 and angle == 0:
                return i
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "<":
            angle += 1
        elif c == ">" and angle > 0 and s[i - 1:i] != "-":
            angle -= 1
        elif c == target and depth == 0 and angle == 0:
            return i
    return -1


def find_assign(s):
    depth = angle = 0
    for i, c in enumerate(s):
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "<":
            angle += 1
        elif c == ">" and angle > 0 and s[i - 1:i] != "-":
            angle -= 1
        elif c == "=" and depth == 0 and angle == 0:
            if s[i + 1:i + 2] == "=" or s[i - 1:i] in ("=", "!", "<", ">"):
                continue
            return i
    return -1


def match_paren(s, i):
    depth = 0
    for j in range(i, len(s)):
        if s[j] == "(":
            depth += 1
        elif s[j] == ")":
            depth -= 1
            if depth == 0:
                return j
    return len(s) - 1


def is_function_head(s):
    p = find_top(s, "(")
    if p <= 0:
        return False
    a = find_assign(s)
    if a != -1 and a < p:
        return False
    if not re.search(r"[A-Za-z_]\w*\s*$", s[:p]):
        return False
    tail = s[match_paren(s, p) + 1:].strip()
    return (tail == "" or tail.startswith(":") or tail.startswith("->")
            or re.fullmatch(r"((const|override|final|noexcept)\s*)+(=\s*0)?", tail) is not None
            or re.fullmatch(r"=\s*0", tail) is not None)


def scope_kind(s):
    if re.match(r"^namespace\b", s):
        return "namespace"
    if re.match(r"^(class|struct)\s+\w", s) and "(" not in s:
        return s.split()[0]
    if re.match(r"^enum\b", s):
        return "enum"
    return None


class HeaderParser:
    def __init__(self, path):
        self.file = rel(path)
        self.top = []
        self.loose = []
        self.stack = [{"kind": "file", "node": None, "access": "public", "group": None}]
        self.doc = []
        self.macro = ""
        self.group_text = None
        self.stmt = ""
        self.stmt_line = 0
        self.stmt_comment = None
        self.skip = 0
        self.skip_sig = ""
        self.skip_line = 0
        self.init = 0
        self.finished = []
        text = read(path)
        text = re.sub(r"/\*\s*(\w+)\s*\*/", r"\1", text)                   # int32 /*LostCombo*/ -> int32 LostCombo
        text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
        for number, raw in enumerate(text.split("\n"), 1):
            self.feed(number, raw)

    # ---- section headings: "// --- Title: detail ---", possibly over several comment lines ----
    def set_group(self, text):
        text = text.strip().strip("-").strip()
        m = re.match(r"^(.*?)(?::\s+|\.\s+)(.+)$", text)
        title = m.group(1) if m and len(m.group(1)) <= 70 else text
        scope = self.stack[-1]
        scope["group"] = title.strip()
        if scope["node"] is not None:
            scope["node"].setdefault("groups", {})[title.strip()] = text

    def feed(self, ln, raw):
        code, comment = split_comment(raw)
        s = code.strip()
        if s.startswith("#"):
            return
        if not s:
            if self.skip or self.init:
                return
            if comment is None:
                self.doc = []
                if self.group_text is not None:
                    self.set_group(self.group_text)
                    self.group_text = None
                return
            if self.group_text is not None:
                self.group_text += " " + comment
                if comment.rstrip().endswith("---"):
                    self.set_group(self.group_text)
                    self.group_text = None
                return
            if comment.startswith("---"):
                if comment.rstrip().endswith("---") and comment.strip("- "):
                    self.set_group(comment)
                else:
                    self.group_text = comment
                return
            self.doc.append(comment)
            return
        if self.group_text is not None:
            self.set_group(self.group_text)
            self.group_text = None
        self.finished = []
        for i, c in enumerate(code):
            self.char(c, code, i, ln)
        if self.stmt.strip():
            self.stmt += " "
            if comment:
                self.stmt_comment = comment
        if comment and self.finished:
            m = self.finished[-1]
            m["doc"] = (m["doc"] + " " + comment).strip() if m.get("doc") else comment

    def reset_stmt(self):
        self.stmt = ""
        self.stmt_line = 0
        self.stmt_comment = None

    def char(self, c, code, i, ln):
        if self.skip:
            if c == "{":
                self.skip += 1
            elif c == "}":
                self.skip -= 1
                if self.skip == 0 and self.skip_sig:
                    m = self.add_function(self.skip_sig, self.skip_line, True)
                    if m:
                        self.finished.append(m)
            return
        if self.init:
            self.stmt += c
            if c == "{":
                self.init += 1
            elif c == "}":
                self.init -= 1
            return
        if c == "{":
            s = " ".join(self.stmt.split())
            kind = scope_kind(s)
            if kind:
                self.open_scope(kind, s, self.stmt_line or ln)
            elif s and is_function_head(s):
                self.skip, self.skip_sig, self.skip_line = 1, s, self.stmt_line or ln
            elif s:
                self.stmt += c
                self.init = 1
                return
            else:
                self.skip, self.skip_sig = 1, ""
            self.reset_stmt()
            return
        if c == "}":
            scope = self.stack[-1]
            if scope["kind"] == "enum" and self.stmt.strip():
                m = self.add_enumerator(self.stmt, self.stmt_line or ln, self.stmt_comment)
                if m:
                    self.finished.append(m)
            self.reset_stmt()
            if len(self.stack) > 1:
                self.stack.pop()
            return
        if c == ";":
            s = self.stmt.strip()
            if s:
                m = self.finish(s, self.stmt_line or ln)
                if m:
                    self.finished.append(m)
            self.reset_stmt()
            return
        if c == "," and self.stack[-1]["kind"] == "enum":
            m = self.add_enumerator(self.stmt, self.stmt_line or ln, self.stmt_comment)
            if m:
                self.finished.append(m)
            self.reset_stmt()
            return
        if c == ":" and self.stmt.strip() in ACCESS and code[i + 1:i + 2] != ":" and code[i - 1:i] != ":":
            self.stack[-1]["access"] = self.stmt.strip()
            self.stack[-1]["group"] = None
            self.doc = []
            self.reset_stmt()
            return
        if not self.stmt.strip():
            if c.isspace():
                return
            self.stmt_line = ln
        self.stmt += c
        if c == ")":
            t = self.stmt.lstrip()
            m = UE_MACRO.match(t)
            if m and t.count("(") == t.count(")"):
                if not m.group(1).startswith("GENERATED"):
                    self.macro = " ".join(t.split())
                self.stmt = ""
                self.stmt_line = 0

    def open_scope(self, kind, s, ln):
        parent = self.stack[-1]
        node = {"kind": kind, "name": "", "macro": self.macro, "bases": [], "doc": " ".join(self.doc).strip(),
                "file": self.file, "line": ln, "members": [], "nested": [],
                "access": parent["access"] if parent["node"] is not None else "public",
                "group": parent["group"]}
        if kind == "namespace":
            parts = s.split()
            node["name"] = parts[1] if len(parts) > 1 else "(anonymous)"
        elif kind in ("class", "struct"):
            m = re.match(r"^(?:class|struct)\s+(?:\w+_API\s+)?(\w+)(?:\s+final)?\s*(?::\s*(.+))?$", s)
            if not m:
                fail("can't read type header '%s' in %s:%d" % (s, self.file, ln))
            node["name"] = m.group(1)
            if m.group(2):
                node["bases"] = [re.sub(r"^(public|protected|private)\s+", "", b.strip()) for b in m.group(2).split(",")]
        else:
            m = re.match(r"^enum\s+(class\s+)?(\w+)(?:\s*:\s*(\w+))?", s)
            node["name"] = m.group(2)
            node["underlying"] = m.group(3) or ""
        self.doc = []
        self.macro = ""
        if parent["node"] is None:
            self.top.append(node)
        else:
            parent["node"]["nested"].append(node)
        self.stack.append({"kind": kind, "node": node, "access": "private" if kind == "class" else "public", "group": None})

    def base_member(self, kind, name, ln):
        scope = self.stack[-1]
        m = {"kind": kind, "name": name, "access": scope["access"], "group": scope["group"],
             "doc": " ".join(self.doc).strip(), "spec": self.macro, "line": ln}
        self.doc = []
        self.macro = ""
        if scope["node"] is None:
            self.loose.append(m)
        else:
            scope["node"]["members"].append(m)
        return m

    def finish(self, s, ln):
        s = " ".join(s.split())
        if self.stack[-1]["kind"] == "enum":
            return self.add_enumerator(s, ln, None)
        if re.match(r"^(class|struct|enum)\s+(\w+_API\s+)?\w+$", s) or re.match(r"^(using|friend|typedef|template|static_assert)\b", s):
            self.doc = []
            self.macro = ""
            return None
        if s.startswith("GENERATED_"):
            return None
        if s.startswith("DECLARE_"):
            return self.add_delegate(s, ln)
        if is_function_head(s):
            return self.add_function(s, ln, False)
        return self.add_property(s, ln)

    def add_function(self, s, ln, inline):
        p = find_top(s, "(")
        q = match_paren(s, p)
        pre = s[:p].strip()
        params = " ".join(s[p + 1:q].split())
        post = s[q + 1:].strip()
        if post.startswith(":"):
            post = ""                                   # constructor initializer list
        mname = re.search(r"(~?[A-Za-z_]\w*)$", pre)
        name = mname.group(1) if mname else pre
        words = pre[:len(pre) - len(name)].split()
        flags = [w for w in words if w in FLAG_WORDS]
        ret = " ".join(w for w in words if w not in FLAG_WORDS)
        quals = re.findall(r"\b(const|override|final|noexcept)\b", post)
        if re.search(r"=\s*0\s*$", post):
            quals.append("pure")
        owner = self.stack[-1]["node"]
        kind = "method"
        if owner is None or self.stack[-1]["kind"] == "namespace":
            kind = "function"
        elif name == owner["name"]:
            kind = "ctor"
        elif name.startswith("~"):
            kind = "dtor"
        m = self.base_member(kind, name, ln)
        m.update({"ret": ret, "params": params, "quals": quals, "flags": flags, "inline": inline})
        return m

    def add_property(self, s, ln):
        a = find_assign(s)
        decl = s[:a].strip() if a != -1 else s
        default = s[a + 1:].strip() if a != -1 else ""
        m = re.match(r"^(.*?)([A-Za-z_]\w*)\s*((?:\[[^\]]*\])*)\s*(?::\s*\d+)?$", decl)
        if not m or not m.group(1).strip():
            return None
        words = m.group(1).split()
        flags = [w for w in words if w in FLAG_WORDS]
        type_ = " ".join(w for w in words if w not in FLAG_WORDS)
        constant = "constexpr" in flags or ("static" in flags and type_.startswith("const "))
        kind = "constant" if constant else "property"
        if re.fullmatch(r"FOn\w+", type_) and m.group(2).startswith("On"):
            kind = "event"
        mem = self.base_member(kind, m.group(2), ln)
        mem.update({"type": type_, "array": m.group(3), "default": default, "flags": [f for f in flags if f != "constexpr"] if constant else flags})
        return mem

    def add_delegate(self, s, ln):
        m = re.match(r"^(DECLARE_\w+)\s*\(\s*(\w+)\s*(?:,\s*(.*))?\)$", s)
        if not m:
            return None
        mem = self.base_member("delegate", m.group(2), ln)
        mem.update({"params": " ".join((m.group(3) or "").split()), "macro_kind": m.group(1)})
        return mem

    def add_enumerator(self, s, ln, comment):
        s = " ".join(s.split())
        m = re.match(r"^(\w+)\s*(?:=\s*(.+))?$", s)
        if not m:
            return None
        mem = self.base_member("enumerator", m.group(1), ln)
        mem["value"] = m.group(2) or ""
        if comment:
            mem["doc"] = (mem["doc"] + " " + comment).strip()
        return mem


# =====================================================================================
# Hand-written overlay: lanes, summaries, external libraries, relationships.
# =====================================================================================
# (id, name, colour token, blurb, grid column): Libraries stacks under Atmosphere.
LANES = [
    ("ui", "Interface", "bat", "UMG built in C++: cards, meters, popups", 0),
    ("flow", "Game flow", "moon", "Composition, input, camera, save data", 1),
    ("rules", "Rules", "cross", "State and scoring. No visuals.", 2),
    ("board", "Board & effects", "blood", "Board state, tiles, effects, meshes", 3),
    ("atmos", "Atmosphere", "skull", "The cathedral and the dread layer", 4),
    ("lib", "Libraries", "lib", "Third-party code and engine systems", 4),
]

# (id, lane, summary, unused-leftover?) in display order within each lane.
CLASSES = [
    ("UPuzzleHUDWidget", "ui", "The whole game UI, built in C++ with no Widget Blueprint: top bar, combo and luck meters, relic buttons, HOLD label, popups and the modal cards (menu, level intro, results, How to Play). It polls the rules every frame and takes one-shot events from the game mode.", False),
    ("APuzzleHUD", "ui", "Creates UPuzzleHUDWidget and adds it to the viewport as soon as the HUD begins play.", False),
    ("UPuzzleUIButton", "ui", "A UButton that never takes keyboard focus, so game keys keep reaching the player controller after a click.", False),
    ("UPuzzleButtonProxy", "ui", "One per button. UButton::OnClicked carries no payload, so the proxy remembers the action and its parameter (a level number, for example).", False),
    ("EPuzzleCard", "ui", "Which modal card the UI is showing.", False),
    ("UMainMenuUI", "ui", "Widget from the original project zip. Nothing creates it; the menu is a card in UPuzzleHUDWidget.", True),
    ("UPuzzleGameUI", "ui", "Widget from the original project zip. Nothing creates it; UPuzzleHUDWidget replaced it.", True),

    ("APuzzleGameMode", "flow", "Composition root and conductor. StartPlay loads the save, creates the rules, spawns the board, input handler and cathedral, binds the seven rule events and reads the launch flags. It runs the flow (menu, level intro, playing, finished), turns rule events into sound, haptics, shake and UI, plays the music playlist and drives the demo bot.", False),
    ("EPuzzleFlow", "flow", "Where the game is: menu, level intro, playing or finished.", False),
    ("APuzzleInputHandler", "flow", "Turns mouse and touch drags, and SDL3 gamepad navigation, into rule calls: pick a tray piece, show the ghost preview, place, park in HOLD, aim Holy Light. Board input only works while the game mode says the player is playing.", False),
    ("UPuzzleSDLGamepadSubsystem", "flow", "Game-instance subsystem over SDL3's gamepad API. Opens the first controller, polls SDL events every tick and exposes just-pressed buttons and the left stick.", False),
    ("APuzzleCameraPawn", "flow", "Fixed camera that frames board and tray on any screen shape, portrait phones included. It also owns the post-process setup (manual exposure, grade, grain, lens flares, ink outline) and screen shake.", False),
    ("UPuzzleSaveGame", "flow", "Quest progress (highest unlocked level, best stars per level), the best Endless score and whether How to Play was seen.", False),

    ("UPuzzleManager", "rules", "The rules: scoring, the three-placement combo window, the move budget and refunds, relics, gargoyle stones, quest goals, luck and omens. It changes state at once and broadcasts events; everything you see and hear happens elsewhere, later.", False),
    ("FPuzzleClearEvent", "rules", "Everything the presentation needs to celebrate one clear: the clear result, points, bonus moves, combo and where it happened.", False),
    ("UPuzzleBotPlayer", "rules", "Greedy demo bot. Tries every tray piece at every cell, prefers moves that clear lines, then tight packing; uses relics when stuck or crowded.", False),
    ("QuestCatalog", "rules", "The hand-tuned quest levels (goal, target, move budget, gargoyle interval) and the sentence that describes each goal.", False),
    ("PieceLibrary", "rules", "The piece shapes, from the single cell to the five-cell pieces, and random piece drawing.", False),
    ("FQuestLevel", "rules", "One quest level: its goal, target, sigil colour, move budget and gargoyle interval.", False),
    ("FPuzzlePieceShape", "rules", "A piece: occupied cell offsets from its top-left origin, plus its colour.", False),
    ("PuzzleTypes", "rules", "The tile palette (ToLinearColor) and the number of sigil colours.", False),
    ("EPuzzleTileColor", "rules", "The five sigils: red blood drop, green skull, blue moon, yellow cross, purple bat.", False),
    ("EOmen", "rules", "Misfortunes luck can ward off.", False),
    ("ERelic", "rules", "The two relics.", False),
    ("EQuestGoal", "rules", "What a quest level asks for.", False),
    ("EPlayMode", "rules", "Endless or a quest level.", False),
    ("EPuzzleGamepadButton", "rules", "Gamepad actions the input handler reads.", False),

    ("AGridManager", "board", "Owns the 9×9 board state (filled cells, colours, stone levels), the tray of three plus HOLD, the blessed box, and every board visual: tiles, ghost previews, rune circles, clear pops, lightning strikes and hex effects.", False),
    ("FClearResult", "board", "What one clear did: lines, cells and boxes cleared, blessings, stones cracked or broken, counts per colour.", False),
    ("APuzzleTile", "board", "One lacquered tile or gargoyle stone: the shared realtime mesh (enamel body plus gold bezel), a box collider for physics bursts, and its own animation states (arrive, squash, pop, fly, stone hit, petrify).", False),
    ("APuzzleFX", "board", "One-shot effects that destroy themselves when done: sparkles, glow strips, rings, lightning bolts, rune circles and real, shadow-casting light flashes.", False),
    ("MeshBuffers", "board", "Turns ToonMesh buffers into realtime meshes: meshoptimizer ordering, one section per material slot, and a process-wide cache so every tile shares one mesh.", False),
    ("ToonMesh", "board", "Procedural geometry: rounded, bevelled blocks and their outline hulls as raw vertex buffers.", False),

    ("AGothicEnvironment", "atmos", "The cathedral around the board and its moods: wall, stained glass, Blender-made piers, candles, flagstones, lightning, and the Lovecraftian layer. Dread (low luck plus omen spikes) drives breathing and stuttering light, tentacles, eyes, fog, motes, the madness post-process, the MetaSound ambience and the convolution reverb.", False),
    ("EldritchNoise", "atmos", "Fractal noise for organic shapes and light: FastNoise2 SIMD on Windows, FMath::PerlinNoise elsewhere.", False),
]

EXTERNALS = [
    {"id": "lib_SDL3", "name": "SDL3", "stereo": "external module", "path": "Source/ThirdParty/SDL3", "version": "3.4.16",
     "summary": "Game controller input across vendors, from a large community controller database."},
    {"id": "lib_RMC", "name": "RealtimeMeshComponent", "stereo": "project plugin", "path": "Plugins/RealtimeMeshComponent", "version": "plugin 5.4, MIT",
     "summary": "Runtime mesh component whose mesh data can be shared between components."},
    {"id": "lib_meshopt", "name": "meshoptimizer", "stereo": "source module", "path": "Source/MeshOptimizer", "version": "1.3, MIT",
     "summary": "Vertex-cache and vertex-fetch ordering for generated meshes."},
    {"id": "lib_FastNoise2", "name": "FastNoise2", "stereo": "external module", "path": "Source/ThirdParty/FastNoise2", "version": "1.1.1, MIT · Win64",
     "summary": "SIMD fractal noise with runtime SSE2 to AVX-512 dispatch."},
    {"id": "lib_Audio", "name": "MetaSounds · Synthesis", "stereo": "engine plugins", "path": "Engine/Plugins/Runtime", "version": "UE 5.8",
     "summary": "The MS_Abyss graph and the convolution reverb on the SM_Cathedral submix."},
    {"id": "lib_FSR", "name": "AMD FSR", "stereo": "project plugin", "path": "Plugins/FSR", "version": "4.1.1 · Win64",
     "summary": "Optional temporal upscaler, switched on with -fsr."},
]

EDGE_KINDS = [
    ("own", "Creates or spawns"),
    ("call", "Calls or reads"),
    ("event", "Broadcasts events"),
    ("lib", "Uses a library"),
]

# (from, to, kind, label, [(file in Source/PuzzleGame5x5/Private, regex), ...]) -- every regex must match.
GM = "PuzzleGameMode.cpp"
RELATIONS = [
    ("APuzzleGameMode", "UPuzzleManager", "own", "creates the rules", [(GM, r"NewObject<UPuzzleManager>")]),
    ("APuzzleGameMode", "AGridManager", "own", "spawns the board", [(GM, r"SpawnActor<AGridManager>")]),
    ("APuzzleGameMode", "APuzzleInputHandler", "own", "spawns", [(GM, r"SpawnActor<APuzzleInputHandler>")]),
    ("APuzzleGameMode", "AGothicEnvironment", "own", "spawns · TriggerStrike · AddDread", [(GM, r"SpawnActor<AGothicEnvironment>"), (GM, r"Environment->TriggerStrike"), (GM, r"Environment->AddDread")]),
    ("APuzzleGameMode", "UPuzzleBotPlayer", "own", "creates · TakeBestAction", [(GM, r"NewObject<UPuzzleBotPlayer>"), (GM, r"BotPlayer->TakeBestAction")]),
    ("APuzzleGameMode", "APuzzleFX", "own", "celebration burst", [(GM, r"SpawnActor<APuzzleFX>")]),
    ("APuzzleGameMode", "UPuzzleSaveGame", "call", "LoadOrCreate · Save", [(GM, r"UPuzzleSaveGame::LoadOrCreate"), (GM, r"SaveGame->Save\(\)")]),
    ("APuzzleGameMode", "UPuzzleHUDWidget", "call", "ShowCard · ShowClear · ShowOmen", [(GM, r"UI->ShowCard"), (GM, r"UI->ShowClear"), (GM, r"UI->ShowOmen")]),
    ("APuzzleGameMode", "APuzzleCameraPawn", "call", "SetFramingBounds · AddShake", [(GM, r"CameraPawn->SetFramingBounds"), (GM, r"CameraPawn->AddShake")]),
    ("APuzzleGameMode", "QuestCatalog", "call", "level data", [(GM, r"QuestCatalog::Get")]),
    ("APuzzleGameMode", "lib_FSR", "lib", "-fsr turns it on", [(GM, r"r\.FidelityFX\.FSR\.Enabled")]),
    ("UPuzzleManager", "APuzzleGameMode", "event", "7 events: OnCleared, OnOmen, OnFinished…", [(GM, r"PuzzleManager->OnPiecePlaced\.AddUObject"), (GM, r"PuzzleManager->OnCleared\.AddUObject"), (GM, r"PuzzleManager->OnComboBroken\.AddUObject"), (GM, r"PuzzleManager->OnRelicGained\.AddUObject"), (GM, r"PuzzleManager->OnStoneSpawned\.AddUObject"), (GM, r"PuzzleManager->OnOmen\.AddUObject"), (GM, r"PuzzleManager->OnFinished\.AddUObject")]),
    ("UPuzzleManager", "AGridManager", "call", "PlacePieceAt · CheckAndClearLines · StrikeLightning · PlayHex", [("PuzzleManager.cpp", r"GridManager->PlacePieceAt"), ("PuzzleManager.cpp", r"GridManager->CheckAndClearLines"), ("PuzzleManager.cpp", r"GridManager->StrikeLightning"), ("PuzzleManager.cpp", r"GridManager->PlayHex"), ("PuzzleManager.cpp", r"GridManager->SpawnCurseStone")]),
    ("APuzzleInputHandler", "UPuzzleManager", "call", "TryPlacePiece · ParkPiece · UseHolyLight", [("PuzzleInputHandler.cpp", r"PuzzleManager->TryPlacePiece"), ("PuzzleInputHandler.cpp", r"PuzzleManager->ParkPiece"), ("PuzzleInputHandler.cpp", r"PuzzleManager->UseHolyLight")]),
    ("APuzzleInputHandler", "AGridManager", "call", "picking · ghost preview · aim circle", [("PuzzleInputHandler.cpp", r"GridManager->FindTraySlotAt"), ("PuzzleInputHandler.cpp", r"GridManager->ShowGhostPreview"), ("PuzzleInputHandler.cpp", r"GridManager->ShowAreaTarget")]),
    ("APuzzleInputHandler", "UPuzzleSDLGamepadSubsystem", "call", "polls buttons", [("PuzzleInputHandler.cpp", r"GetSubsystem<UPuzzleSDLGamepadSubsystem>"), ("PuzzleInputHandler.cpp", r"GamepadSubsystem->WasButtonJustPressed")]),
    ("APuzzleInputHandler", "APuzzleGameMode", "call", "ToggleAutoPlay · Retry", [("PuzzleInputHandler.cpp", r"GameMode->ToggleAutoPlay"), ("PuzzleInputHandler.cpp", r"GameMode->Retry")]),
    ("UPuzzleBotPlayer", "UPuzzleManager", "call", "TryPlacePiece · UseReroll · UseHolyLight", [("PuzzleBotPlayer.cpp", r"PuzzleManager->TryPlacePiece"), ("PuzzleBotPlayer.cpp", r"PuzzleManager->UseReroll"), ("PuzzleBotPlayer.cpp", r"PuzzleManager->UseHolyLight")]),
    ("UPuzzleBotPlayer", "AGridManager", "call", "SimulateLinesCleared · FindDensestArea", [("PuzzleBotPlayer.cpp", r"GridManager->SimulateLinesCleared"), ("PuzzleBotPlayer.cpp", r"GridManager->FindDensestArea")]),
    ("AGridManager", "APuzzleTile", "own", "spawns cells, tray pieces, ghosts", [("GridManager.cpp", r"SpawnActor<APuzzleTile>")]),
    ("AGridManager", "APuzzleFX", "own", "clears · bolts · hexes", [("GridManager.cpp", r"SpawnActor<APuzzleFX>")]),
    ("AGridManager", "PieceLibrary", "call", "MakeRandomPiece", [("GridManager.cpp", r"PieceLibrary::")]),
    ("AGridManager", "MeshBuffers", "call", "builds the board mesh", [("GridManager.cpp", r"MeshBuffers::BuildRealtimeMesh")]),
    ("AGridManager", "ToonMesh", "call", "blocks and hulls", [("GridManager.cpp", r"ToonMesh::BuildBlock")]),
    ("APuzzleTile", "MeshBuffers", "call", "one shared tile mesh", [("PuzzleTile.cpp", r"MeshBuffers::GetSharedMesh")]),
    ("MeshBuffers", "ToonMesh", "call", "reads the buffers", [("MeshBuffersUtil.cpp", r"ToonMesh::FBuffers")]),
    ("MeshBuffers", "lib_RMC", "lib", "URealtimeMeshSimple", [("MeshBuffersUtil.cpp", r"URealtimeMeshSimple")]),
    ("MeshBuffers", "lib_meshopt", "lib", "optimizeVertexCache · VertexFetchRemap", [("MeshBuffersUtil.cpp", r"meshopt_optimizeVertexCache"), ("MeshBuffersUtil.cpp", r"meshopt_optimizeVertexFetchRemap")]),
    ("AGothicEnvironment", "UPuzzleManager", "call", "reads GetLuck() for dread", [("GothicEnvironment.cpp", r"PuzzleManager->GetLuck\(\)")]),
    ("AGothicEnvironment", "EldritchNoise", "call", "tentacle skin · breathing", [("GothicEnvironment.cpp", r"EldritchNoise::Fractal3D"), ("GothicEnvironment.cpp", r"EldritchNoise::Fractal1D")]),
    ("AGothicEnvironment", "MeshBuffers", "call", "tentacle meshes", [("GothicEnvironment.cpp", r"MeshBuffers::BuildRealtimeMesh")]),
    ("AGothicEnvironment", "APuzzleCameraPawn", "call", "adds the madness post-process", [("GothicEnvironment.cpp", r"FindComponentByClass<UCameraComponent>")]),
    ("AGothicEnvironment", "lib_Audio", "lib", "MS_Abyss · convolution reverb", [("GothicEnvironment.cpp", r"MS_Abyss"), ("GothicEnvironment.cpp", r"USubmixEffectConvolutionReverbPreset")]),
    ("EldritchNoise", "lib_FastNoise2", "lib", "FractalFBm(Simplex)", [("EldritchNoise.cpp", r"FastNoise::New")]),
    ("APuzzleHUD", "UPuzzleHUDWidget", "own", "creates", [("PuzzleHUD.cpp", r"CreateWidget<UPuzzleHUDWidget>")]),
    ("UPuzzleHUDWidget", "APuzzleGameMode", "call", "button actions: StartEndless · SelectLevel…", [("PuzzleHUDWidget.cpp", r"GameMode->StartEndless"), ("PuzzleHUDWidget.cpp", r"GameMode->SelectLevel")]),
    ("UPuzzleHUDWidget", "UPuzzleManager", "call", "polls score · moves · combo · luck", [("PuzzleHUDWidget.cpp", r"Rules->GetLuck\(\)"), ("PuzzleHUDWidget.cpp", r"Rules->ComboStreak")]),
    ("UPuzzleHUDWidget", "APuzzleInputHandler", "call", "targeting and HOLD hover state", [("PuzzleHUDWidget.cpp", r"InputHandler->IsHolyTargeting"), ("PuzzleHUDWidget.cpp", r"InputHandler->IsHoveringReserve")]),
    ("UPuzzleHUDWidget", "AGridManager", "call", "places the HOLD label", [("PuzzleHUDWidget.cpp", r"GridManager->GetTrayAnchorWorldLocation")]),
    ("UPuzzleHUDWidget", "UPuzzleButtonProxy", "own", "one per button", [("PuzzleHUDWidget.cpp", r"NewObject<UPuzzleButtonProxy>")]),
    ("UPuzzleHUDWidget", "UPuzzleUIButton", "own", "every button", [("PuzzleHUDWidget.cpp", r"ConstructWidget<UPuzzleUIButton>")]),
    ("UPuzzleHUDWidget", "QuestCatalog", "call", "level cards", [("PuzzleHUDWidget.cpp", r"QuestCatalog::")]),
    ("UPuzzleButtonProxy", "UPuzzleHUDWidget", "call", "HandleAction", [("PuzzleHUDWidget.cpp", r"Widget->HandleAction")]),
    ("UPuzzleSDLGamepadSubsystem", "lib_SDL3", "lib", "SDL gamepad API", [("PuzzleSDLGamepadSubsystem.cpp", r"SDL_GetGamepadButton")]),
]

FLAG_TEXT = {
    "demo": "Endless mode with the demo bot playing.",
    "demoquest": "The bot plays the quest levels from level N (default 1). Its results are never saved.",
    "level": "Open quest level N's intro card.",
    "tutorial": "Show the How to Play pages even if they were seen before.",
    "tutorialpage": "Open How to Play at page N (0-based).",
    "omenrate": "Fixed omen chance per move, 0 to 1. For testing.",
    "dread": "Hold dread at X or above, 0 to 1. For testing and recordings.",
    "fsr": "AMD FSR upscaling (Quality) instead of TSR. Windows only.",
    "recordaudio": "Record the game's audio mix for N seconds to Saved/Recording/demo_audio.wav, then quit.",
}

FLAG_ARG = {"demoquest": "[=N]", "level": "=N", "tutorialpage": "=N", "omenrate": "=X", "dread": "=X", "recordaudio": "=N"}

SCRIPTS = [
    ("arcane_head.py", "Unreal editor Python", "Shared helpers for the material scripts: create or rebuild a material, custom HLSL nodes, parameters, and the NOISE and SYMBOLS HLSL libraries."),
    ("arcane_body.py", "Unreal editor Python", "M_TileArcane, M_HolyAura, M_PPInkOutline, M_UIPanel, M_UIIcon."),
    ("build_gothic_symbols.py", "Unreal editor Python", "M_TileGothic (the five sigils), M_UIIcon, M_GroundMist."),
    ("build_mist2.py", "Unreal editor Python", "M_GroundMist2."),
    ("build_storm_fx.py", "Unreal editor Python", "M_FXBolt, the lightning bolt."),
    ("build_eldritch_fx.py", "Unreal editor Python", "M_Tentacle, M_EldritchEye, M_PPMadness."),
    ("build_motes.py", "Unreal editor Python", "M_Mote, the instanced dust, ember and wisp particle."),
    ("gothic_audio.py", "rocm-env Python", "SFXG_Place, _Clear, _Blessed, _Holy, _GameOver, _Gargoyle, _Relic, _ComboLost, _Thunder and the organ loop MUS_Gothic."),
    ("chant_audio.py", "rocm-env Python", "MUS_Chant (Faust singers, opening with the Dies irae) and SFXG_Strike, _Hex, _Ward."),
    ("eldritch_audio.py", "rocm-env Python", "SFXG_Flicker, AMB_Abyss and its stems AMB_Drone, _Shepard, _Whisper, _Air."),
    ("audio_fx.py", "rocm-env Python (module)", "Faust bell and singer through DawDreamer, church convolution, pedalboard mastering. Used by the three audio scripts."),
    ("cathedral_ir.py", "rocm-env Python", "IR_Cathedral.wav, a reference copy of the impulse response the game synthesizes at start-up."),
    ("import_gothic_audio.py", "Unreal editor Python", "Imports RawAudio/*.wav into /Game/Audio; MUS_ and AMB_ sounds loop."),
    ("gen_textures.py", "rocm-env Python + ComfyUI", "T_PillarStone, T_WallStone, T_FloorSlab and their _N normal maps."),
    ("blender_pier.py", "Blender (blender -b -P)", "SM_GothicPier.fbx with T_Pier_N and T_Pier_AO bakes."),
    ("build_allopts_assets.py", "Unreal editor Python", "Imports the textures and the pier; builds M_PierStone, M_FloorSlab, SM_Cathedral with every SFX sending to it, the AMB stems and the MS_Abyss MetaSound."),
    ("limit_textures.py", "Unreal editor Python", "Caps texture sizes for 4 GB graphics cards and phones."),
    ("record_video.ps1", "Windows PowerShell 5.1", "Demo video frames via PrintWindow into FFmpeg; pairs with -recordaudio."),
    ("ArchitectureDoc/gen_architecture.py", "Any Python 3", "This page."),
]


# =====================================================================================
# Helpers: small SVG builder for the static figures.
# =====================================================================================
def svg_open(w, h, label):
    return '<svg class="dia" viewBox="0 0 %d %d" role="img" aria-label="%s" xmlns="http://www.w3.org/2000/svg">' % (w, h, esc(label))


def svg_node(x, y, w, h, title, sub="", lane="", mono=True):
    cls = "node" + (" l-" + lane if lane else "")
    tcls = "title mono" if mono else "title"
    cx = x + w / 2
    if sub:
        t = '<text class="%s" x="%.1f" y="%.1f" text-anchor="middle">%s</text>' % (tcls, cx, y + h / 2 - 3, esc(title))
        t += '<text class="sub" x="%.1f" y="%.1f" text-anchor="middle">%s</text>' % (cx, y + h / 2 + 14, esc(sub))
    else:
        t = '<text class="%s" x="%.1f" y="%.1f" text-anchor="middle">%s</text>' % (tcls, cx, y + h / 2 + 4.5, esc(title))
    return '<g class="%s"><rect x="%d" y="%d" width="%d" height="%d" rx="7"/>%s</g>' % (cls, x, y, w, h, t)


def svg_edge(points, kind="call", head=True):
    d = "M" + " L".join("%.1f,%.1f" % p for p in points)
    out = '<path class="edge %s" d="%s"/>' % (kind, d)
    if head:
        (x1, y1), (x2, y2) = points[-2], points[-1]
        a = math.atan2(y2 - y1, x2 - x1)
        s = 7.5
        p1 = (x2 - s * math.cos(a) + s * 0.5 * math.sin(a), y2 - s * math.sin(a) - s * 0.5 * math.cos(a))
        p2 = (x2 - s * math.cos(a) - s * 0.5 * math.sin(a), y2 - s * math.sin(a) + s * 0.5 * math.cos(a))
        out += '<polygon class="head" points="%.1f,%.1f %.1f,%.1f %.1f,%.1f"/>' % (x2, y2, p1[0], p1[1], p2[0], p2[1])
    return out


def svg_text(x, y, text, anchor="middle", cls="elabel"):
    return '<text class="%s" x="%.1f" y="%.1f" text-anchor="%s">%s</text>' % (cls, x, y, anchor, esc(text))


def table(headers, rows, num_cols=()):
    out = ['<div class="tscroll"><table><thead><tr>']
    out += ["<th>%s</th>" % esc(h) for h in headers]
    out.append("</tr></thead><tbody>")
    for r in rows:
        out.append("<tr>")
        for i, cell in enumerate(r):
            cls = ' class="num"' if i in num_cols else (' class="why"' if i == len(r) - 1 and len(r) > 2 else "")
            out.append("<td%s>%s</td>" % (cls, cell))
        out.append("</tr>")
    out.append("</tbody></table></div>")
    return "".join(out)


# =====================================================================================
# Build
# =====================================================================================
def main():
    headers = sorted(glob.glob(os.path.join(PUBLIC, "*.h")))
    nodes = {}
    loose_by_file = {}
    for h in headers:
        parser = HeaderParser(h)
        for n in parser.top:
            if n["name"] in nodes:
                fail("type %s declared twice (%s and %s)" % (n["name"], nodes[n["name"]]["file"], n["file"]))
            nodes[n["name"]] = n
        loose_by_file[rel(h)] = parser.loose
    # File-scope constants (PuzzleColorCount) belong to their header's namespace.
    for f, loose in loose_by_file.items():
        if not loose:
            continue
        spaces = [n for n in nodes.values() if n["file"] == f and n["kind"] == "namespace"]
        if spaces:
            spaces[0]["members"] = loose + spaces[0]["members"]

    listed = {c[0] for c in CLASSES}
    missing = sorted(set(nodes) - listed)
    if missing:
        fail("types in the headers but not placed on the diagram (add them to CLASSES): " + ", ".join(missing))
    stale = sorted(listed - set(nodes))
    if stale:
        fail("CLASSES lists types that no longer exist: " + ", ".join(stale))

    # ---- line counts ----
    cpp_lines = {}
    total_loc = 0
    for path in glob.glob(os.path.join(PUBLIC, "*.h")) + glob.glob(os.path.join(PRIVATE, "*.cpp")):
        n = read(path).count("\n") + 1
        total_loc += n
        cpp_lines[os.path.basename(path)] = n

    # ---- classes for the page ----
    classes = []
    for cid, lane, summary, legacy in CLASSES:
        n = nodes[cid]
        macro = (n.get("macro") or "").split("(")[0]
        if macro in ("UCLASS", "USTRUCT", "UENUM"):
            stereo = "\u00ab%s\u00bb" % macro
        else:
            stereo = {"namespace": "namespace", "struct": "struct", "class": "class", "enum": "enum"}[n["kind"]]
        base = os.path.splitext(os.path.basename(n["file"]))[0]
        cpp = base + ".cpp"
        search = [m["name"].lower() for m in n["members"]]
        for sub in n["nested"]:
            search.append(sub["name"].lower())
            search += [m["name"].lower() for m in sub["members"]]
        classes.append({
            "id": cid, "name": cid, "lane": lane, "kind": n["kind"], "stereo": stereo, "bases": n["bases"],
            "summary": summary, "legacy": legacy, "doc": n["doc"], "file": n["file"], "line": n["line"],
            "cppLines": cpp_lines.get(cpp, 0) if n["kind"] != "enum" else 0,
            "members": n["members"], "nested": n["nested"],
            "memberCount": len(n["members"]) + len(n["nested"]),
            "searchNames": search,
        })
    for ext in EXTERNALS:
        if not os.path.exists(os.path.join(PROJ, ext["path"])) and not ext["path"].startswith("Engine/"):
            fail("external %s path missing: %s" % (ext["name"], ext["path"]))
        classes.append({"id": ext["id"], "name": ext["name"], "lane": "lib", "kind": "external", "stereo": ext["stereo"],
                        "bases": [], "summary": ext["summary"], "legacy": False, "doc": "", "file": "", "line": 0,
                        "path": ext["path"], "version": ext["version"], "cppLines": 0, "members": [], "nested": [],
                        "memberCount": 0, "searchNames": []})
    ids = {c["id"] for c in classes}

    # ---- relationships, each verified against the code ----
    sources = {os.path.basename(p): read(p) for p in glob.glob(os.path.join(PRIVATE, "*.cpp"))}
    relations = []
    for frm, to, kind, label, evidence in RELATIONS:
        if frm not in ids or to not in ids:
            fail("relation %s -> %s names an unknown type" % (frm, to))
        for f, pattern in evidence:
            if f not in sources or not re.search(pattern, sources[f]):
                fail("relation %s -> %s (%s) is no longer true: /%s/ not found in %s" % (frm, to, label, pattern, f))
        relations.append({"from": frm, "to": to, "kind": kind, "label": label,
                          "evidence": ["%s: %s" % (f, p) for f, p in evidence]})

    # ---- constants read from the code ----
    def const(owner, name):
        for m in nodes[owner]["members"]:
            if m["name"] == name and m["kind"] in ("constant", "property"):
                v = m["default"].rstrip("f")
                try:
                    return float(v) if "." in v else int(v)
                except ValueError:
                    return m["default"]
        fail("constant %s::%s not found" % (owner, name))

    def fmt(v):
        return ("%g" % v) if isinstance(v, float) else str(v)

    PM, GRID = "UPuzzleManager", "AGridManager"
    grid_size, box_size = const(GRID, "GridSize"), const(GRID, "BoxSize")
    arrive, omen_delay = const(GRID, "ArriveDuration"), const(PM, "OmenDelay")
    tile_spacing = const(GRID, "TileSpacing")
    shapes = len(re.findall(r"^\s*\{\s*\{", read(os.path.join(PUBLIC, "PieceLibrary.h")), flags=re.M))
    levels = len(re.findall(r"\{\s*EQuestGoal::", read(os.path.join(PUBLIC, "QuestCatalog.h"))))
    if grid_size % box_size:
        fail("GridSize %s is not a multiple of BoxSize %s" % (grid_size, box_size))
    lines_9 = grid_size * 2 + (grid_size // box_size) ** 2

    # ---- launch flags read from the code ----
    flags_found = {}
    for f, text in sources.items():
        for m in re.finditer(r'FParse::(?:Value|Param)\(\s*(?:CommandLine|FCommandLine::Get\(\))\s*,\s*TEXT\("([^"=]+)=?"\)', text):
            flags_found.setdefault(m.group(1), set()).add(f)
    if set(flags_found) != set(FLAG_TEXT):
        fail("launch flags in code %s differ from FLAG_TEXT %s" % (sorted(flags_found), sorted(FLAG_TEXT)))

    # =================================================================================
    # Page sections
    # =================================================================================
    n_types = sum(1 for c in classes if c["kind"] != "external") + sum(len(c["nested"]) for c in classes)
    facts = [
        (str(n_types), "types"),
        (str(len(headers)), "headers"),
        ("{:,}".format(total_loc), "lines of C++"),
        (str(shapes), "piece shapes"),
        (str(levels), "quest levels"),
        (str(len(relations)), "checked arrows"),
    ]
    facts_html = "".join("<div><dt>%s</dt><dd>%s</dd></div>" % (esc(label), esc(value)) for value, label in facts)

    # Masthead mark: the 9x9 board with its alternating boxes and one row a tile short of clearing.
    cell = 17
    mark = ['<svg class="board-mark" viewBox="0 0 %d %d" role="img" aria-label="A 9 by 9 board of nine 3 by 3 boxes, one row a tile short of clearing" xmlns="http://www.w3.org/2000/svg">' % (grid_size * cell + 8, grid_size * cell + 8)]
    colours = ["t-red", "t-green", "t-blue", "t-yellow", "t-purple"]
    tiles = {(0, 4): 0, (1, 4): 0, (2, 4): 2, (3, 4): 3, (4, 4): 3, (6, 4): 1, (7, 4): 4, (8, 4): 2,
             (6, 1): 4, (7, 1): 4, (7, 2): 4, (1, 7): 1, (2, 7): 1, (2, 8): 1, (4, 6): 2}
    for y in range(grid_size):
        for x in range(grid_size):
            light = ((x // box_size) + (y // box_size)) % 2 == 0
            cls = colours[tiles[(x, y)]] if (x, y) in tiles else ("cell-a" if light else "cell-b")
            mark.append('<rect class="%s" x="%d" y="%d" width="%d" height="%d" rx="2.5"/>' % (cls, 4 + x * cell + 1, 4 + y * cell + 1, cell - 2, cell - 2))
    mark.append('<rect class="frame" x="1.5" y="1.5" width="%d" height="%d" rx="5"/>' % (grid_size * cell + 5, grid_size * cell + 5))
    mark.append("</svg>")
    board_mark = "".join(mark)

    L = lambda o, n: fmt(const(o, n))
    overview = """
<h2>What this is</h2>
<p class="kicker">A block-placement puzzle in the Woodoku family, built with Unreal Engine 5.8 and C++. It targets iPhone (portrait, touch first) and runs on Windows, where it also gets a ray-traced cathedral.</p>
<p>You drag pieces from a tray of three onto a {g}×{g} board. Any full row, column or {b}×{b} box clears. The rules live in one plain object, <code>UPuzzleManager</code>, which changes state the moment a piece lands and broadcasts events. What you see and hear follows later, timed to the animations: the game mode turns rule events into sound, haptics, camera shake and UI, and the cathedral reads your luck to decide how dark the room gets.</p>
<div class="glance">
  <div><h3>Board</h3><p>{g}×{g} cells and {lines} lines that can clear: {g} rows, {g} columns, {nb} boxes. A tray of {tray} pieces plus HOLD, drawn from {shapes} shapes.</p></div>
  <div><h3>Combo</h3><p>A clear starts a combo that survives {cw} placements without a clear. Multi-line clears climb faster; a line of one sigil is Blessed; the box inside the rune circle scores ×3.</p></div>
  <div><h3>Moves</h3><p>Each piece costs a move; clears refund moves. Endless starts with {moves}. Quest levels carry their own budget and earn 1 to 3 stars from the moves left.</p></div>
  <div><h3>Relics</h3><p>One every {rs} combo steps, at most {rmax} of each. Holy Light purges a {b}×{b} area; Reroll replaces the tray.</p></div>
  <div><h3>Curses</h3><p>Gargoyle stones land every few moves (every {stones} in Endless): a clear cracks one, a second shatters it. Omens strike too: lightning turns a tile to stone, a hex takes {hex} moves.</p></div>
  <div><h3>Luck</h3><p>Starts at {luck0}. +{lstep} per combo step, −{lholy} for Holy Light, −{lre} for Reroll, −{lward} per ward. It is the chance, up to {lmax}%, that an omen is warded off.</p></div>
  <div><h3>Modes</h3><p>Endless (best score saved) and {levels} quest levels, plus a demo mode where a greedy bot plays.</p></div>
  <div><h3>Presentation</h3><p>Ray-traced cathedral on PC, a dread layer that wakes as luck runs out, an interface built in C++, a synthesized Gregorian score and a MetaSound ambience.</p></div>
</div>
""".format(g=grid_size, b=box_size, lines=lines_9, nb=(grid_size // box_size) ** 2, tray=const(GRID, "TraySize"), shapes=shapes,
           cw=L(PM, "ComboWindowMoves"), moves=L(PM, "EndlessStartingMoves"), rs=L(PM, "RelicComboStep"), rmax=L(PM, "MaxRelicCharges"),
           stones=L(PM, "EndlessStoneInterval"), hex=L(PM, "HexMoveCost"), luck0=L(PM, "StartingLuck"), lstep=L(PM, "LuckPerComboStep"),
           lholy=L(PM, "HolyLightLuckCost"), lre=L(PM, "RerollLuckCost"), lward=L(PM, "WardLuckCost"), lmax=L(PM, "MaxWardPercent"), levels=levels)

    # ---- architecture figure ----
    a = [svg_open(1200, 620, "Input reaches the rules; the rules change the board and broadcast events; the game mode turns events into UI, sound, camera shake and the cathedral's mood, which reads luck back from the rules.")]
    a.append(svg_node(40, 24, 104, 46, "Touch · mouse", "drag · drop · tap", "flow", mono=False))
    a.append(svg_node(156, 24, 104, 46, "Gamepad", "SDL3", "flow", mono=False))
    a.append(svg_node(40, 118, 220, 56, "APuzzleInputHandler", "pick · preview · place", "flow"))
    a.append(svg_node(330, 118, 240, 56, "UPuzzleBotPlayer", "demo mode", "rules"))
    a.append(svg_node(650, 118, 220, 56, "UPuzzleSaveGame", "stars · best score · tutorial", "flow"))
    a.append(svg_node(330, 250, 240, 78, "UPuzzleManager", "score · combo · luck · omens", "rules"))
    a.append(svg_node(650, 250, 220, 78, "APuzzleGameMode", "flow · feedback · music", "flow"))
    a.append(svg_node(960, 250, 200, 78, "AGothicEnvironment", "dread: light · tentacles · eyes", "atmos"))
    a.append(svg_node(330, 420, 240, 62, "AGridManager", "%d×%d state · tray · board visuals" % (grid_size, grid_size), "board"))
    a.append(svg_node(620, 420, 170, 56, "UPuzzleHUDWidget", "cards · meters · popups", "ui"))
    a.append(svg_node(810, 420, 150, 56, "Sound · haptics", "SFX · music playlist", "", mono=False))
    a.append(svg_node(980, 420, 180, 56, "APuzzleCameraPawn", "framing · shake", "flow"))
    a.append(svg_node(330, 548, 240, 56, "APuzzleTile · APuzzleFX", "animated · self-destroying", "board"))
    a.append(svg_edge([(92, 70), (92, 118)]))
    a.append(svg_edge([(208, 70), (208, 118)]))
    a.append(svg_edge([(150, 174), (150, 289), (330, 289)]))
    a.append(svg_text(158, 228, "TryPlacePiece · ParkPiece · UseHolyLight", "start"))
    a.append(svg_edge([(450, 174), (450, 250)]))
    a.append(svg_text(458, 216, "same calls", "start"))
    a.append(svg_edge([(570, 289), (650, 289)], "event"))
    a.append(svg_text(610, 307, "7 events"))
    a.append(svg_edge([(760, 250), (760, 174)]))
    a.append(svg_text(768, 216, "load · save", "start"))
    a.append(svg_edge([(870, 289), (960, 289)]))
    a.append(svg_text(915, 280, "TriggerStrike"))
    a.append(svg_text(915, 307, "AddDread"))
    a.append(svg_edge([(1060, 250), (1060, 96), (600, 96), (600, 270), (570, 270)], "read"))
    a.append(svg_text(830, 88, "reads GetLuck(): dread rises as luck falls"))
    a.append(svg_edge([(450, 328), (450, 420)]))
    a.append(svg_text(458, 364, "PlacePieceAt · CheckAndClearLines", "start"))
    a.append(svg_text(458, 381, "StrikeLightning · PlayHex", "start"))
    a.append(svg_edge([(450, 482), (450, 548)]))
    a.append(svg_text(458, 520, "spawns", "start"))
    a.append(svg_edge([(760, 328), (760, 372), (705, 372), (705, 420)]))
    a.append(svg_edge([(760, 372), (885, 372), (885, 420)]))
    a.append(svg_edge([(760, 372), (1070, 372), (1070, 420)]))
    a.append(svg_text(711, 402, "ShowClear · cards", "start"))
    a.append(svg_text(891, 402, "PlaySfx", "start"))
    a.append(svg_text(1076, 402, "AddShake", "start"))
    a.append("</svg>")
    arch_svg = "".join(a)

    folders = [
        ("Source/PuzzleGame5x5", "All game code: Public headers, Private sources."),
        ("Source/MeshOptimizer", "meshoptimizer 1.3, compiled with the project."),
        ("Source/ThirdParty", "SDL3 and FastNoise2 as prebuilt external modules (Win64 libraries)."),
        ("Plugins", "RealtimeMeshComponent (source) and AMD FSR (prebuilt, Win64)."),
        ("Content", "Audio, Materials, Meshes, Textures, UI/Fonts and the one map."),
        ("Config", "DefaultEngine.ini (rendering, iOS, streaming), DefaultGame.ini (packaging), DefaultInput.ini."),
        ("Tools", "The scripts that build every asset, and this page's generator."),
        ("RawAudio", "Synthesized .wav files the import script reads."),
        ("RawTextures", "Generated stone textures and their normal maps."),
        ("RawMeshes", "The Blender pier (.fbx) and its bakes."),
        ("Demo", "PuzzleMagic_Demo.mp4."),
    ]
    for path, _ in folders:
        if not os.path.exists(os.path.join(PROJ, path)):
            fail("folder listed on the page is missing: " + path)
    folder_rows = [("<code>%s/</code>" % esc(p), esc(t)) for p, t in folders]

    architecture = """
<h2>Architecture</h2>
<p class="kicker">Input reaches the rules, the rules change the board and announce what happened, and the game mode decides how it looks and sounds.</p>
<figure>
  <div class="fig-scroll">{svg}</div>
  <figcaption>Solid arrows are calls, the dashed arrow is the rules' seven events, the dotted arrow is a read. Rule state changes in the frame of the drop; the game mode schedules the feedback to match the animations (see <a href="#move">A move</a>).</figcaption>
</figure>
<h3>Decisions that shape the code</h3>
<ol class="decisions">
  <li><b>Rules never touch visuals.</b> <code>UPuzzleManager</code> is a UObject with no rendering. It updates the board state immediately and broadcasts events; the board plays animations with delays, and the game mode times sound, haptics and UI to them.</li>
  <li><b>The UI is code.</b> <code>UPuzzleHUDWidget</code> builds every widget in C++ from two materials (enamel panel, embossed icon) and two fonts. Nothing lives in a Widget Blueprint, so the UI diffs and reviews like the rest.</li>
  <li><b>Every asset comes from a script.</b> Materials are HLSL custom nodes written by Python in the editor, sounds are synthesized, meshes are procedural or scripted in Blender, and textures come from ComfyUI.</li>
  <li><b>Rebuildable assets load at run time.</b> An asset held by a C++ constructor (ConstructorHelpers) is rooted inside the editor commandlet, and rebuilding it there crashes. Newer assets use LoadObject so their scripts can rebuild them in place.</li>
  <li><b>All tiles share one mesh.</b> MeshBuffers builds the tile once as a realtime mesh and every tile's component points at it: one set of GPU buffers for the whole board.</li>
  <li><b>PC gets ray tracing; iPhone gets the mobile renderer.</b> Lumen and MegaLights don't run on iOS in 5.8, so the phone look has to be authored for the mobile renderer.</li>
  <li><b>Visual changes are checked in screenshots.</b> Logs stayed clean while the camera faced the sky and while meshes rendered inside out, so every visual change is verified with a capture of the running game.</li>
</ol>
<h3>Modules and folders</h3>
{folders}
""".format(svg=arch_svg, folders=table(["Folder", "What is in it"], folder_rows))

    # ---- lanes (cards) ----
    by_lane = {}
    for c in classes:
        by_lane.setdefault(c["lane"], []).append(c)
    lane_color = {l[0]: l[2] for l in LANES}
    lanes_html = []
    column = -1
    for lid, name, color, blurb, col in LANES:
        if col != column:
            if column != -1:
                lanes_html.append("</div>")
            lanes_html.append('<div class="col">')
            column = col
        lanes_html.append('<div class="lane" style="--lane:var(--%s)"><div class="lane-head"><h3>%s</h3><p>%s</p></div>' % (color, esc(name), esc(blurb)))
        for c in by_lane.get(lid, []):
            enum_chips = ""
            if c["kind"] == "enum":
                enum_chips = '<p class="enums">' + "".join('<code title="%s">%s</code>' % (esc(m.get("doc", "")), esc(m["name"])) for m in c["members"]) + "</p>"
            where = ("<code>%s:%d</code>" % (esc(c["file"].split("/")[-1]), c["line"])) if c["file"] else ("<code>%s</code>" % esc(c.get("path", "")))
            meta = where
            if c["cppLines"]:
                meta += "<span>{:,} lines in .cpp</span>".format(c["cppLines"])
            if c.get("version"):
                meta += "<span>%s</span>" % esc(c["version"])
            bases = (' <span class="base">: %s</span>' % esc(", ".join(c["bases"]))) if c["bases"] else ""
            tag = ' <span class="tag">unused</span>' if c["legacy"] else ""
            exp = ""
            if c["memberCount"]:
                exp = '<button type="button" class="exp" aria-expanded="false" aria-controls="m-%s" data-exp="%s">Members <span class="n">%d</span></button>' % (esc(c["id"]), esc(c["id"]), c["memberCount"])
            lanes_html.append(
                '<article class="card%s" id="%s" data-id="%s" style="--lane:var(--%s)">' % (" legacy" if c["legacy"] else "", esc(c["id"]), esc(c["id"]), lane_color[c["lane"]])
                + '<p class="stereo">%s%s%s</p>' % (esc(c["stereo"]), bases, tag)
                + '<h3 class="cname"><button type="button" class="sel-btn" data-sel="%s">%s</button></h3>' % (esc(c["id"]), esc(c["name"]))
                + '<p class="summary">%s</p>%s<p class="meta">%s</p>%s' % (esc(c["summary"]), enum_chips, meta, exp)
                + '<div class="members" id="m-%s" hidden></div></article>' % esc(c["id"]))
        lanes_html.append("</div>")
    lanes_html.append("</div>")

    edge_toggles = "".join(
        '<label><input type="checkbox" id="ek-%s" data-kind="%s" checked> <span class="swatch k-%s"></span> %s</label>' % (k, k, k, esc(t))
        for k, t in EDGE_KINDS)

    # ---- a move, end to end (sequence diagram) ----
    lanes_seq = [("input", "APuzzleInputHandler", "flow"), ("rules", "UPuzzleManager", "rules"), ("grid", "AGridManager", "board"),
                 ("fx", "Tiles · FX", "board"), ("mode", "APuzzleGameMode", "flow"), ("pres", "HUD · sound · camera", "ui"),
                 ("env", "AGothicEnvironment", "atmos")]
    xs = {k: 160 + i * 158 for i, (k, _, _) in enumerate(lanes_seq)}
    stone_at = arrive + 0.7 + 0.45
    rows = [
        ("0 s", "input", "rules", "TryPlacePiece(slot, x, y)", "call"),
        ("", "rules", "grid", "PlacePieceAt · CheckAndClearLines", "call"),
        ("", "grid", "fx", "spawns tiles; they fly in from the tray", "call"),
        ("", "rules", "rules", "score · combo window · refunds · luck · relics", "self"),
        ("", "rules", "grid", "SpawnCurseStone (every Nth move) · RollOmen", "call"),
        ("", "rules", "mode", "OnPiecePlaced · OnCleared · OnOmen · OnFinished", "event"),
        ("", "mode", "mode", "Later(delay, …) schedules the feedback", "self"),
        ("%g s" % arrive, "mode", "pres", "tiles land: place sound · haptic · shake", "call"),
        ("%g s" % (arrive + 0.1), "grid", "fx", "cleared tiles pop and burst with physics", "call"),
        ("", "mode", "pres", "bell (higher with the combo) · ShowClear popups", "call"),
        ("%g s" % omen_delay, "grid", "fx", "lightning bolt and light flash, or hex runes", "call"),
        ("", "mode", "env", "TriggerStrike · AddDread", "call"),
        ("", "mode", "pres", "strike or hex sound · LIGHTNING / HEXED / WARDED", "call"),
        ("%g s" % stone_at, "mode", "pres", "gargoyle lands: stone sound · popup", "call"),
        ("2 s", "mode", "pres", "result card, if the round ended", "call"),
    ]
    top, dy = 96, 34
    height = top + len(rows) * dy + 10
    s = [svg_open(1200, height, "Sequence of one move: the rules update at once and broadcast; tiles land at %g seconds, clears pop at %g, omens strike at %g, gargoyles land at %g." % (arrive, arrive + 0.1, omen_delay, stone_at))]
    for key, title, lane in lanes_seq:
        x = xs[key]
        s.append('<line class="life" x1="%d" y1="56" x2="%d" y2="%d"/>' % (x, x, height - 6))
        s.append(svg_node(x - 74, 14, 148, 40, title, "", lane, mono=title[:1] in "AU"))
    for i, (tick, frm, to, label, kind) in enumerate(rows):
        y = top + i * dy
        if tick:
            if i:
                s.append('<line class="band" x1="12" y1="%d" x2="1188" y2="%d"/>' % (y - dy / 2, y - dy / 2))
            s.append(svg_text(78, y + 4, tick, "end", "tick"))
        if kind == "self":
            x = xs[frm]
            s.append(svg_edge([(x, y - 7), (x + 26, y - 7), (x + 26, y + 7), (x + 2, y + 7)]))
            s.append(svg_text(x + 34, y + 4, label, "start"))
        else:
            x1, x2 = xs[frm], xs[to]
            s.append(svg_edge([(x1, y), (x2 - (3 if x2 > x1 else -3), y)], "event" if kind == "event" else "call"))
            s.append(svg_text((x1 + x2) / 2, y - 6, label))
    s.append("</svg>")
    seq_svg = "".join(s)

    move = """
<h2>A move, end to end</h2>
<p class="kicker">What happens between dropping a piece and the last popup fading. Times come from <code>AGridManager::ArriveDuration</code> ({arrive:g} s) and <code>UPuzzleManager::OmenDelay</code> ({omen:g} s) in the code.</p>
<figure>
  <div class="fig-scroll">{svg}</div>
  <figcaption>The first seven messages happen in the frame of the drop: the rules already know the outcome, including any omen. Everything below a time mark is a delayed callback (<code>APuzzleGameMode::Later</code>) or a delayed animation, so the sound lands with the tile and the bolt lands with the flash.</figcaption>
</figure>
<h3>Start-up, in order</h3>
<ol>
  <li><code>APuzzleGameMode::StartPlay</code> loads or creates the save (<code>UPuzzleSaveGame::LoadOrCreate</code>) and creates the rules.</li>
  <li>It spawns <code>AGridManager</code>, <code>APuzzleInputHandler</code> and <code>AGothicEnvironment</code>, binds the rules to the board and gives the input handler both.</li>
  <li>It binds the seven rule events, creates the demo bot and frames the camera on the board and tray.</li>
  <li>It starts the music playlist: the chant twice, then the organ piece, crossfaded.</li>
  <li>It reads the launch flags (see <a href="#run">Run &amp; tune</a>) and opens the menu, with How to Play on the very first launch, or goes straight to a demo or a level.</li>
</ol>
<p>Meanwhile <code>AGothicEnvironment::BeginPlay</code> builds the wall, stained glass, piers, flagstones, mist and candles, attaches the convolution reverb to the <code>SM_Cathedral</code> submix and builds the dread layer: tentacles, eyes, fog volumes, motes, the madness post-process and the MetaSound ambience.</p>
""".format(arrive=arrive, omen=omen_delay, svg=seq_svg)

    # ---- tools and libraries ----
    engine_rows = [
        ("Unreal Engine", "5.8", "Engine: rendering, UMG, audio, iPhone and Windows builds.", "One C++ codebase for the iPhone target and desktop ray tracing, with MetaSounds and editor Python built in."),
        ("MSVC (Visual Studio Build Tools 2022)", "14.44", "C++20 compiler for the Windows builds.", "Unreal 5.8's Windows toolchain. Installed on D: to spare the C: drive."),
        ("UMG, driven from C++", "engine", "Every screen, meter, card and popup.", "No Widget Blueprints: the UI is code, reviewable and diffable."),
        ("Lumen hardware ray tracing, MegaLights, Virtual Shadow Maps, TSR", "engine", "The PC look: ray-traced light and reflections, a shadow from every candle.", "Runs at 70 to 85 FPS at 440×950 on the RX 6400. Not available on iOS, which uses the mobile renderer."),
        ("MetaSounds", "engine", "MS_Abyss: four ambience stems mixed live by dread, through a ladder filter.", "Parameter-driven mixing at run time instead of one fixed loop."),
        ("Synthesis plugin (convolution reverb)", "engine", "The SM_Cathedral submix reverb every sound effect sends to.", "Real convolution; the impulse response is synthesized at start-up, so there is no file to import or license."),
        ("Local Fog Volumes, instanced static meshes", "engine", "Floor fog; dust, embers and wisps in one draw call.", "Cheap on PC and on phones."),
    ]
    lib_rows = [
        ("SDL3", "3.4.16", "Gamepad input.", "A large community controller database and consistent button layouts across vendors."),
        ("RealtimeMeshComponent", "plugin 5.4", "Tiles, board and tentacles.", "Mesh data can be shared, so all tiles use one set of GPU buffers. Faster than ProceduralMeshComponent, which the code no longer uses."),
        ("meshoptimizer", "1.3", "Vertex-cache and vertex-fetch ordering of generated meshes.", "MIT, and builds from source for every platform."),
        ("FastNoise2", "1.1.1", "Tentacle skin, irregular breathing of the light.", "SIMD fractal noise. Windows only for now: an iOS build must be made on a Mac, and until then iOS uses FMath::PerlinNoise."),
        ("AMD FSR", "4.1.1", "Optional upscaler (-fsr).", "Measured no faster than TSR at 440×950, so TSR stays the default; kept for large PC windows."),
        ("Lilita One, Cinzel Decorative", "OFL", "Body and title type in the UI.", "Loaded from .ttf files at run time, because importing fonts needs Slate, which commandlets lack."),
    ]
    pipe_rows = [
        ("Editor Python (UnrealEditor-Cmd -run=pythonscript)", "3.11.8", "Builds materials from HLSL custom nodes; imports audio, textures and meshes; builds the MetaSound graph.", "Every asset can be rebuilt from a script."),
        ("numpy, scipy", "2.5.3 · 1.18.1", "Synthesis of every sound effect and music loop.", "Full control and nothing to license."),
        ("DawDreamer with the Faust libraries", "0.9.0", "Church bells (pm.churchBell) and the chant's singers (pm.SFFormantModelFofSmooth).", "Physical and formant models sound more real than hand-rolled additive synthesis."),
        ("pedalboard", "0.9.25", "Mastering: high-pass, compressor, limiter.", "A three-stage chain in a few lines of Python."),
        ("Voxengo impulse response “St Nicolaes Church”", "free pack", "Offline convolution of the music and effects.", "Royalty-free for commercial use, but the file itself may not be redistributed, so only processed audio ships."),
        ("Blender with Cycles (HIP)", "5.2", "SM_GothicPier: the clustered pier and its normal and AO bakes.", "Real modelling and baking, scripted end to end; bakes on the RX 6400."),
        ("ComfyUI, Stable Diffusion 1.5 (DreamShaper 8), ComfyUI-seamless-tiling", "0.35.0", "Tileable stone textures.", "Already installed on this PC's ROCm setup; circular padding makes the textures tile."),
        ("DeepBump", "git", "Normal maps from colour, and ×2 upscaling.", "Runs on the CPU through ONNX."),
        ("CMake", "4.4.3", "Builds the FastNoise2 static libraries.", "FastNoise2's own build system."),
        ("FFmpeg", "9.0.1", "The demo video.", "Encodes the captured frames."),
    ]
    not_rows = [
        ("Niagara", "Its Python API in 5.8 can spawn systems but not add emitters or edit modules, so the particles are instanced meshes driven from C++."),
        ("Wwise, FMOD", "Would replace Unreal's audio engine and the MetaSound work, and needs an account."),
        ("NVIDIA DLSS", "Runs only on NVIDIA GPUs; neither the RX 6400 nor iPhones can use it."),
        ("Stable Audio Open", "The model weights need a Hugging Face login and licence acceptance first."),
        ("MusicGen, AudioLDM2", "Their model licences forbid commercial use."),
    ]

    def tool_rows(rows):
        return [(esc(a_), esc(b_), esc(c_), esc(d_)) for a_, b_, c_, d_ in rows]

    tools = """
<h2>Tools and libraries</h2>
<p class="kicker">What the project is built with, what each piece does here, and why it was chosen.</p>
<h3>Engine and run time</h3>
{t1}
<h3>Libraries and plugins in the build</h3>
{t2}
<h3>Content pipeline (never shipped)</h3>
{t3}
<h3>Considered and not used</h3>
{t4}
""".format(t1=table(["Tool", "Version", "Used for", "Why"], tool_rows(engine_rows), num_cols=(1,)),
           t2=table(["Library", "Version", "Used for", "Why"], tool_rows(lib_rows), num_cols=(1,)),
           t3=table(["Tool", "Version", "Used for", "Why"], tool_rows(pipe_rows), num_cols=(1,)),
           t4=table(["Option", "Why not"], [(esc(x), esc(y)) for x, y in not_rows]))

    # ---- content pipeline ----
    materials = sorted(os.path.splitext(f)[0] for f in os.listdir(os.path.join(PROJ, "Content", "Materials")) if f.endswith(".uasset"))
    scripted = set()
    for py in glob.glob(os.path.join(PROJ, "Tools", "*.py")):
        # Any quoted material name counts: scripts create them with new_material("M_X") or, like
        # build_gothic_symbols.py, by renaming another script's code ('"M_TileArcane"' -> '"M_TileGothic"').
        scripted |= set(re.findall(r"[\"'](M_[A-Za-z0-9_]+)[\"']", read(py)))
    unscripted = [m for m in materials if m not in scripted]
    for f, _, _ in SCRIPTS:
        if not os.path.exists(os.path.join(PROJ, "Tools", f)):
            fail("script listed on the page is missing: Tools/" + f)
    listed_scripts = {f for f, _, _ in SCRIPTS}
    for f in sorted(glob.glob(os.path.join(PROJ, "Tools", "*.py")) + glob.glob(os.path.join(PROJ, "Tools", "*.ps1"))):
        if os.path.basename(f) not in listed_scripts:
            fail("Tools/%s is not described in SCRIPTS" % os.path.basename(f))

    p = [svg_open(1200, 400, "Scripts generate raw audio, meshes and textures; editor commandlets import them and build materials into Content; C++ loads core assets with ConstructorHelpers and rebuildable ones with LoadObject.")]
    for x, t in [(130, "GENERATE"), (385, "RAW FILES"), (645, "IMPORT AND BUILD (COMMANDLET)"), (895, "CONTENT"), (1105, "LOADED BY C++")]:
        p.append(svg_text(x, 22, t, "middle", "colhead"))
    p.append(svg_node(20, 40, 220, 58, "numpy · Faust · pedalboard", "gothic_ · chant_ · eldritch_audio.py", "", mono=False))
    p.append(svg_node(20, 130, 220, 58, "Blender 5.2 · Cycles", "blender_pier.py", "", mono=False))
    p.append(svg_node(20, 220, 220, 58, "ComfyUI · SD 1.5 · DeepBump", "gen_textures.py", "", mono=False))
    p.append(svg_node(290, 40, 190, 58, "RawAudio/", "*.wav"))
    p.append(svg_node(290, 130, 190, 58, "RawMeshes/", "*.fbx · baked *.png"))
    p.append(svg_node(290, 220, 190, 58, "RawTextures/", "colour + normal *.png"))
    p.append(svg_node(530, 40, 230, 58, "import_gothic_audio.py", "wav → SoundWave"))
    p.append(svg_node(530, 130, 230, 148, "build_allopts_assets.py", "pier · textures · submix · MetaSound"))
    p.append(svg_node(530, 310, 230, 58, "build_*.py · arcane_*.py", "HLSL custom-node materials"))
    p.append(svg_node(810, 40, 170, 58, "/Game/Audio", "SFXG · MUS · AMB · MS_Abyss"))
    p.append(svg_node(810, 130, 170, 58, "/Game/Meshes", "SM_GothicPier"))
    p.append(svg_node(810, 220, 170, 58, "/Game/Textures", "stone · bakes"))
    p.append(svg_node(810, 310, 170, 58, "/Game/Materials", "%d materials" % len(materials)))
    p.append(svg_node(1040, 60, 140, 70, "ConstructorHelpers", "core assets"))
    p.append(svg_node(1040, 250, 140, 70, "LoadObject", "rebuildable assets"))
    for y in (69, 159, 249):
        p.append(svg_edge([(240, y), (290, y)]))
    p.append(svg_edge([(480, 69), (530, 69)]))
    p.append(svg_edge([(480, 159), (530, 159)]))
    p.append(svg_edge([(480, 249), (530, 249)]))
    p.append(svg_edge([(760, 69), (810, 69)]))
    p.append(svg_edge([(760, 159), (810, 159)]))
    p.append(svg_edge([(760, 249), (810, 249)]))
    p.append(svg_edge([(760, 339), (810, 339)]))
    for y in (69, 159, 249, 339):
        p.append(svg_edge([(980, y), (1010, y)], "bus", head=False))
    p.append(svg_edge([(1010, 69), (1010, 339)], "bus", head=False))
    p.append(svg_edge([(1010, 95), (1040, 95)]))
    p.append(svg_edge([(1010, 285), (1040, 285)]))
    p.append("</svg>")
    pipe_svg = "".join(p)
    script_rows = [("<code>%s</code>" % esc(f), esc(where), esc(what)) for f, where, what in SCRIPTS]
    pipeline = """
<h2>Content pipeline</h2>
<p class="kicker">Nothing in <code>Content/</code> is drawn or recorded by hand. Scripts generate it, editor commandlets import it, and C++ picks it up.</p>
<figure>
  <div class="fig-scroll">{svg}</div>
  <figcaption>Commandlet runs look like <code>UnrealEditor-Cmd.exe PuzzleGame5x5.uproject -run=pythonscript -script=Tools\\build_eldritch_fx.py</code>. Close the running game first: it locks the map and the module DLL.</figcaption>
</figure>
{scripts}
<p>{unscripted}</p>
""".format(svg=pipe_svg, scripts=table(["Script", "Runs in", "Produces"], script_rows),
           unscripted=("Materials in <code>Content/Materials</code> with no build script in <code>Tools/</code>: " + ", ".join("<code>%s</code>" % esc(m) for m in unscripted) + ". They came from earlier scripts that were not kept.") if unscripted else "Every material in Content/Materials has its build script in Tools/.")

    # ---- why 9x9 ----
    def board_svg(ox, oy, n, size, box, title, note1, note2):
        cellw = size / n
        out = [svg_text(ox + size / 2, oy - 14, title, "middle", "gridtitle")]
        for y in range(n):
            for x in range(n):
                bx, by = x // box, y // box
                whole = (bx + 1) * box <= n and (by + 1) * box <= n
                cls = ("cell-a" if (bx + by) % 2 == 0 else "cell-b") if whole else "cell-bad"
                out.append('<rect class="%s" x="%.1f" y="%.1f" width="%.1f" height="%.1f"/>' % (cls, ox + x * cellw, oy + y * cellw, cellw, cellw))
        for i in range(n + 1):
            out.append('<line class="gridline" x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f"/>' % (ox + i * cellw, oy, ox + i * cellw, oy + size))
            out.append('<line class="gridline" x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f"/>' % (ox, oy + i * cellw, ox + size, oy + i * cellw))
        for i in range(0, n + 1, box):
            out.append('<line class="boxline" x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f"/>' % (ox + i * cellw, oy, ox + i * cellw, oy + size))
            out.append('<line class="boxline" x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f"/>' % (ox, oy + i * cellw, ox + size, oy + i * cellw))
        out.append('<rect class="boxline" x="%.1f" y="%.1f" width="%.1f" height="%.1f"/>' % (ox, oy, size, size))
        out.append(svg_text(ox + size / 2, oy + size + 26, note1, "middle", "note"))
        out.append(svg_text(ox + size / 2, oy + size + 44, note2, "middle", "note"))
        return "".join(out)

    g = [svg_open(640, 330, "An 8 by 8 board leaves 3 by 2, 2 by 3 and 2 by 2 fragments where the third row and column of boxes should be; a 9 by 9 board splits into nine whole 3 by 3 boxes.")]
    g.append(board_svg(40, 48, 8, 216, box_size, "8 × 8", "16 lines: 8 rows, 8 columns", "shaded: box fragments (3×2, 2×3, 2×2)"))
    g.append(board_svg(380, 48, grid_size, 216, box_size, "%d × %d" % (grid_size, grid_size), "%d lines: %d rows, %d columns, %d boxes" % (lines_9, grid_size, grid_size, (grid_size // box_size) ** 2), "every box is a whole 3×3"))
    g.append('<rect class="ring" x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="3"/>' % (380 + 72, 48 + 72, 72, 72))
    g.append("</svg>")
    grid_svg = "".join(g)
    grid = """
<h2>Why 9×9, not 8×8</h2>
<p class="kicker">The first request was an 8×8 board. The rules decided it: they clear rows, columns and 3×3 boxes, and boxes only tile a board whose side is a multiple of three.</p>
<figure>
  <div class="fig-small">{svg}</div>
  <figcaption>Left: on 8×8 the third row and column of boxes are fragments. Right: 9×9 gives nine whole boxes, alternating in shade as the game draws them; the gold outline marks a blessed box.</figcaption>
</figure>
<p>On 8×8 the last row and column of boxes would be 3×2, 2×3 and 2×2 fragments. They are smaller targets, easier to fill than a real box, so the best play would drift to the board's right and bottom edges. Dropping boxes altogether would leave 16 lines and turn the game into a plain row-and-column puzzle.</p>
<p>On {g}×{g} the board splits into nine whole boxes, like a Sudoku grid, and {lines} lines can clear instead of 16. Several features depend on whole boxes:</p>
<ul>
  <li><b>The blessed box.</b> One of the nine is marked with a rune circle, and clearing it scores ×3.</li>
  <li><b>Holy Light</b> purges a 3×3 area around a chosen cell, with its centre clamped to cells 1 to {g2} so it always stays on the board.</li>
  <li><b>Box shading.</b> Slots alternate indigo and burgundy per box (<code>AGridManager::BuildBoardVisuals</code>), so each box reads at a glance.</li>
  <li><b>Goals and the bot.</b> Quest levels ask you to clear boxes, and the demo bot aims Holy Light at the densest 3×3 area (<code>FindDensestArea</code>).</li>
</ul>
<p>It also fits the phone. {g} cells at {sp} units each make an {w}-unit board inside the 1080-unit portrait canvas, with room for the tray below and the score above.</p>
""".format(svg=grid_svg, g=grid_size, g2=grid_size - 2, lines=lines_9, sp=fmt(tile_spacing), w=fmt(tile_spacing * grid_size))

    # ---- run and tune ----
    launcher_rows = []
    for bat in sorted(glob.glob(os.path.join(PROJ, "*.bat"))):
        text = read(bat)
        rems = []
        for line in text.splitlines():
            s_ = line.strip()
            if s_.lower().startswith("start "):
                break
            if s_.lower().startswith("rem "):
                rems.append(s_[4:].strip())
        cmd = re.search(r'\.uproject"\s*(.*)$', text, flags=re.M)
        launcher_rows.append(("<code>%s</code>" % esc(os.path.basename(bat)), esc(" ".join(rems)), "<code>%s</code>" % esc(cmd.group(1).strip() if cmd else "")))
    flag_rows = []
    for key in sorted(FLAG_TEXT):
        files = ", ".join(sorted(flags_found[key]))
        flag_rows.append(("<code>-%s%s</code>" % (esc(key), esc(FLAG_ARG.get(key, ""))), esc(FLAG_TEXT[key]), "<code>%s</code>" % esc(files)))
    tune_rows = []
    for c in classes:
        for m in c["members"]:
            editable = m.get("spec", "").startswith("UPROPERTY") and re.search(r"Edit(Anywhere|DefaultsOnly)", m.get("spec", ""))
            if m["kind"] == "constant" or (m["kind"] == "property" and editable and m.get("default")):
                if m["name"] == "SlotName":
                    continue
                tune_rows.append(("<code>%s</code>" % esc(c["name"]), "<code>%s</code>" % esc(m["name"]), esc(m.get("default", "")), esc(m.get("doc", ""))))
    run = """
<h2>Run and tune</h2>
<h3>Launchers in the project folder</h3>
{launchers}
<h3>Launch flags</h3>
<p>Add these to the game's command line (after <code>-game</code>). Read in the files shown.</p>
{flags}
<h3>Build, rebuild, regenerate</h3>
<pre class="cmd">"C:\\Program Files\\Epic Games\\UE_5.8\\Engine\\Build\\BatchFiles\\Build.bat" PuzzleGame5x5Editor Win64 Development -Project="D:\\Unreal Projects\\PuzzleGame5x5\\PuzzleGame5x5.uproject" -WaitMutex

"C:\\Program Files\\Epic Games\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "D:\\Unreal Projects\\PuzzleGame5x5\\PuzzleGame5x5.uproject" -run=pythonscript -script="D:\\Unreal Projects\\PuzzleGame5x5\\Tools\\build_eldritch_fx.py"

python Tools\\ArchitectureDoc\\gen_architecture.py</pre>
<h3>Tunables</h3>
<p>Constants and editable defaults as they stand in the headers.</p>
{tune}
""".format(launchers=table(["File", "What it does", "Flags"], launcher_rows),
           flags=table(["Flag", "Effect", "Read in"], flag_rows),
           tune=table(["Class", "Name", "Value", "Meaning"], tune_rows, num_cols=(2,)))

    footer = "Generated %s by <code>Tools/ArchitectureDoc/gen_architecture.py</code> from %d headers and %d source files. All %d arrows in the class diagram were matched to the .cpp code that makes them true." % (
        datetime.date.today().isoformat(), len(headers), len(sources), len(relations))

    data = {
        "lanes": [{"id": l[0], "name": l[1], "color": l[2], "blurb": l[3], "column": l[4]} for l in LANES],
        "edgeKinds": [{"id": k, "label": t} for k, t in EDGE_KINDS],
        "classes": classes,
        "relations": relations,
    }
    data_json = json.dumps(data, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")

    page = read(TEMPLATE)
    for key, value in [("__FACTS__", facts_html), ("__BOARD_MARK__", board_mark), ("__OVERVIEW__", overview),
                       ("__ARCHITECTURE__", architecture), ("__EDGE_TOGGLES__", edge_toggles), ("__LANES__", "".join(lanes_html)),
                       ("__MOVE__", move), ("__TOOLS__", tools), ("__PIPELINE__", pipeline), ("__GRID__", grid), ("__RUN__", run),
                       ("__FOOTER__", footer), ("__DATA__", data_json)]:
        if key not in page:
            fail("template placeholder %s missing" % key)
        page = page.replace(key, value)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(page)
    print("wrote %s (%d KB): %d types, %d members, %d arrows, %d launch flags" % (
        rel(OUT), len(page.encode("utf-8")) // 1024, n_types,
        sum(len(c["members"]) + sum(len(n["members"]) for n in c["nested"]) for c in classes), len(relations), len(flags_found)))


if __name__ == "__main__":
    main()

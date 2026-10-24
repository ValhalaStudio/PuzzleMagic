"""Stamps a Windows exe with the game's name, company, copyright and icon (its Details tab and its icon in Explorer).

    python Tools/brand_exe.py <exe> --ico <icon.ico> --original <file name> [--name "Puzzle Magic"] [--company "Valhalla Studios"]
                              [--copyright "(c) 2026 Valhalla Studios"] [--version 1.0.0.0]

The engine compiles its Windows resource file once for every game (it says "UnrealGame", "Epic Games, Inc." and "Fill out your copyright
notice..."), so the packaged game exe is patched here after packaging, and the demo's own launcher exe gets the same. It writes a version
resource and the icon into the exe's resource section with the Windows resource-update calls (the same a resource editor makes), after
removing the old ones. The exe is not signed, so nothing else changes; run the game once afterwards to be sure it still starts.
"""
import argparse, ctypes, os, struct, subprocess, sys, tempfile
from ctypes import wintypes

RT_ICON, RT_GROUP_ICON, RT_VERSION = 3, 14, 16

ap = argparse.ArgumentParser()
ap.add_argument("exe")
ap.add_argument("--ico", required=True)
ap.add_argument("--original", required=True)
ap.add_argument("--name", default="Puzzle Magic")
ap.add_argument("--company", default="Valhalla Studios")
ap.add_argument("--copyright", default="\u00a9 2026 Valhalla Studios")
ap.add_argument("--version", default="1.0.0.0")
args = ap.parse_args()


def find_rc():
    base = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"), "Windows Kits", "10", "bin")
    found = []
    for root, _, files in os.walk(base):
        if "rc.exe" in files and root.lower().endswith("x64"):
            found.append(os.path.join(root, "rc.exe"))
    if not found:
        sys.exit("brand_exe.py: rc.exe (Windows SDK) not found")
    return sorted(found)[-1]


def rc_escape(s):
    # rc string literals: backslash and double quote are escaped; characters above ASCII as octal escapes of the code page 1252 byte
    out = []
    for ch in s:
        if ch == "\\": out.append("\\\\")
        elif ch == '"': out.append('""')
        elif ord(ch) < 128: out.append(ch)
        else: out.append("\\%03o" % ch.encode("cp1252")[0])
    return "".join(out)


ver = [int(x) for x in args.version.split(".")] + [0, 0, 0, 0]
ver = ver[:4]
rc_text = """1 ICON "%(ico)s"
1 VERSIONINFO
FILEVERSION %(v0)d,%(v1)d,%(v2)d,%(v3)d
PRODUCTVERSION %(v0)d,%(v1)d,%(v2)d,%(v3)d
FILEFLAGSMASK 0x3fL
FILEFLAGS 0x0L
FILEOS 0x40004L
FILETYPE 0x1L
FILESUBTYPE 0x0L
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904b0"
    BEGIN
      VALUE "CompanyName", "%(company)s"
      VALUE "FileDescription", "%(name)s"
      VALUE "FileVersion", "%(version)s"
      VALUE "InternalName", "%(name)s"
      VALUE "LegalCopyright", "%(copyright)s"
      VALUE "OriginalFilename", "%(original)s"
      VALUE "ProductName", "%(name)s"
      VALUE "ProductVersion", "%(version)s"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
""" % dict(ico=args.ico.replace("\\", "\\\\"), v0=ver[0], v1=ver[1], v2=ver[2], v3=ver[3], company=rc_escape(args.company),
           name=rc_escape(args.name), version=args.version, copyright=rc_escape(args.copyright), original=rc_escape(args.original))

tmp = tempfile.mkdtemp(prefix="brand_exe_")
rc_path, res_path = os.path.join(tmp, "brand.rc"), os.path.join(tmp, "brand.res")
open(rc_path, "w", encoding="cp1252", errors="replace").write(rc_text)
r = subprocess.run([find_rc(), "/nologo", "/fo", res_path, rc_path], capture_output=True, text=True)
if r.returncode != 0 or not os.path.exists(res_path):
    sys.exit("brand_exe.py: rc.exe failed:\n" + r.stdout + r.stderr)


def read_res(path):
    data = open(path, "rb").read()
    pos, out = 0, []
    while pos < len(data):
        size, hsize = struct.unpack_from("<II", data, pos)
        p = pos + 8
        def name_at(p):
            if struct.unpack_from("<H", data, p)[0] == 0xFFFF:
                return struct.unpack_from("<H", data, p + 2)[0], p + 4
            end = p
            while struct.unpack_from("<H", data, end)[0] != 0: end += 2
            return data[p:end].decode("utf-16-le"), (end + 2)
        rtype, p = name_at(p)
        rname, p = name_at(p)
        p = (p + 3) & ~3
        _dv, _mf, lang, _v, _c = struct.unpack_from("<IHHII", data, p)
        body = data[pos + hsize:pos + hsize + size]
        if size > 0:
            out.append((rtype, rname, lang, body))
        pos = (pos + hsize + size + 3) & ~3
    return out


entries = read_res(res_path)
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.LoadLibraryExW.restype = wintypes.HMODULE
kernel.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD]
kernel.FreeLibrary.argtypes = [wintypes.HMODULE]
kernel.BeginUpdateResourceW.restype = wintypes.HANDLE
kernel.BeginUpdateResourceW.argtypes = [wintypes.LPCWSTR, wintypes.BOOL]
kernel.UpdateResourceW.restype = wintypes.BOOL
kernel.UpdateResourceW.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, wintypes.WORD, ctypes.c_void_p, wintypes.DWORD]
kernel.EndUpdateResourceW.restype = wintypes.BOOL
kernel.EndUpdateResourceW.argtypes = [wintypes.HANDLE, wintypes.BOOL]

ENUMNAME = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HMODULE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)
ENUMLANG = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HMODULE, ctypes.c_void_p, ctypes.c_void_p, wintypes.WORD, ctypes.c_void_p)
kernel.EnumResourceNamesW.argtypes = [wintypes.HMODULE, ctypes.c_void_p, ENUMNAME, ctypes.c_void_p]
kernel.EnumResourceLanguagesW.argtypes = [wintypes.HMODULE, ctypes.c_void_p, ctypes.c_void_p, ENUMLANG, ctypes.c_void_p]


def existing(path, rtype):
    """(name pointer value, language) of every resource of this type in the exe."""
    found = []
    mod = kernel.LoadLibraryExW(path, None, 0x2)  # LOAD_LIBRARY_AS_DATAFILE
    if not mod:
        sys.exit("brand_exe.py: cannot open %s" % path)
    names = []
    def on_name(_m, _t, name, _p):
        names.append(name if name is not None else 0)
        return True
    cb_name = ENUMNAME(on_name)
    kernel.EnumResourceNamesW(mod, ctypes.c_void_p(rtype), cb_name, None)
    for nm in names:
        langs = []
        def on_lang(_m, _t, _n, lang, _p):
            langs.append(lang); return True
        cb_lang = ENUMLANG(on_lang)
        kernel.EnumResourceLanguagesW(mod, ctypes.c_void_p(rtype), ctypes.c_void_p(nm), cb_lang, None)
        for l in langs:
            found.append((nm, l))
    kernel.FreeLibrary(mod)
    return found


exe = os.path.abspath(args.exe)
old = {t: existing(exe, t) for t in (RT_ICON, RT_GROUP_ICON, RT_VERSION)}
h = kernel.BeginUpdateResourceW(exe, False)
if not h:
    sys.exit("brand_exe.py: cannot update %s (is it running?)" % exe)
for t, items in old.items():
    for nm, lang in items:
        kernel.UpdateResourceW(h, ctypes.c_void_p(t), ctypes.c_void_p(nm), lang, None, 0)  # delete
keep = []
for rtype, rname, lang, body in entries:
    buf = ctypes.create_string_buffer(body, len(body))
    keep.append(buf)
    if not kernel.UpdateResourceW(h, ctypes.c_void_p(rtype), ctypes.c_void_p(rname), lang, buf, len(body)):
        kernel.EndUpdateResourceW(h, True)
        sys.exit("brand_exe.py: UpdateResource failed (%d)" % ctypes.get_last_error())
if not kernel.EndUpdateResourceW(h, False):
    sys.exit("brand_exe.py: EndUpdateResource failed (%d)" % ctypes.get_last_error())
print("branded %s: %s | %s | %s | %d icon images, %d version block(s) (replaced %d old resources)" % (
    os.path.basename(exe), args.name, args.company, args.copyright, sum(1 for e in entries if e[0] == RT_ICON), sum(1 for e in entries if e[0] == RT_VERSION),
    sum(len(v) for v in old.values())))

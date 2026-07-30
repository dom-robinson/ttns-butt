#!/usr/bin/env python3
"""Bundle non-system dylibs into a macOS .app so it runs without Homebrew.

Usage:
  macos-bundle-dylibs.py /path/to/Something.app Contents/MacOS/binary-name

Copies libraries under Contents/Frameworks using the install-name *basename*
(preserving symlink sonames like libfoo.1.dylib) and rewrites references to @rpath/.
"""
from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


def run_out(cmd: list[str]) -> str:
    return subprocess.check_output(cmd, text=True, stderr=subprocess.DEVNULL)


def otool_deps(path: Path) -> list[str]:
    lines = run_out(["otool", "-L", str(path)]).splitlines()[1:]
    out = []
    for line in lines:
        line = line.strip()
        if not line:
            continue
        out.append(line.split(" (", 1)[0].strip())
    return out


def otool_rpaths(path: Path) -> list[str]:
    try:
        info = run_out(["otool", "-l", str(path)])
    except subprocess.CalledProcessError:
        return []
    rpaths = []
    lines = info.splitlines()
    for i, line in enumerate(lines):
        if "cmd LC_RPATH" in line:
            for j in range(i + 1, min(i + 6, len(lines))):
                m = re.search(r"path\s+(\S+)", lines[j])
                if m:
                    rpaths.append(m.group(1))
                    break
    return rpaths


def is_system(dep: str) -> bool:
    return dep.startswith("/usr/lib/") or dep.startswith("/System/")


def expand_rpath(dep: str, referrer: Path) -> Path | None:
    if not dep.startswith("@rpath/"):
        return None
    name = dep[len("@rpath/") :]
    candidates: list[Path] = []
    for rp in otool_rpaths(referrer):
        if rp.startswith("@loader_path"):
            rest = rp[len("@loader_path") :].lstrip("/")
            base = referrer.parent if not rest else (referrer.parent / rest).resolve()
            candidates.append(Path(base) / name)
        elif not rp.startswith("@"):
            candidates.append(Path(rp) / name)
    candidates.append(referrer.parent / name)
    candidates.append(referrer.parent.parent / "lib" / name)
    for c in candidates:
        if c.is_file():
            return c  # keep symlink path if possible
    for root in ("/opt/homebrew", "/usr/local"):
        lib = Path(root) / "lib" / name
        if lib.is_file():
            return lib
        opt = Path(root) / "opt"
        if opt.is_dir():
            for d in opt.iterdir():
                cand = d / "lib" / name
                if cand.is_file():
                    return cand
    return None


def resolve_dep(dep: str, referrer: Path) -> tuple[str, Path] | None:
    """Return (soname basename, readable path) for a bundlable dependency."""
    if is_system(dep):
        return None
    if dep.startswith("@executable_path/"):
        return None

    if dep.startswith("@rpath/"):
        src = expand_rpath(dep, referrer)
        if src is None:
            return None
        return (Path(dep).name, src)

    if dep.startswith("@loader_path/"):
        src = referrer.parent / dep[len("@loader_path/") :]
        if src.is_file():
            return (src.name, src)
        return None

    if dep.startswith("/"):
        src = Path(dep)
        if not src.is_file():
            return None
        # Prefer the path as named in the install name (symlink soname)
        return (src.name, src)

    return None


def change(binary: Path, old: str, new: str) -> None:
    subprocess.check_call(
        ["install_name_tool", "-change", old, new, str(binary)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def set_id(dylib: Path, new_id: str) -> None:
    subprocess.check_call(
        ["install_name_tool", "-id", new_id, str(dylib)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def add_rpath(binary: Path, rpath: str) -> None:
    try:
        info = run_out(["otool", "-l", str(binary)])
    except subprocess.CalledProcessError:
        return
    if f"path {rpath} (" in info:
        return
    try:
        subprocess.check_call(
            ["install_name_tool", "-add_rpath", rpath, str(binary)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        pass


def copy_lib(src: Path, dest: Path) -> None:
    # Copy through the symlink so we get the real bytes, but keep dest soname
    real = src.resolve()
    shutil.copy2(real, dest)
    os.chmod(dest, 0o755)
    subprocess.call(["xattr", "-cr", str(dest)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def bundle_app(app: Path, rel_bin: str) -> None:
    binary = app / rel_bin
    if not binary.is_file():
        raise SystemExit(f"Binary not found: {binary}")

    frameworks = app / "Contents" / "Frameworks"
    frameworks.mkdir(parents=True, exist_ok=True)

    needed: dict[str, Path] = {}  # soname -> source path (may be symlink)
    queue: list[Path] = [binary]
    scanned: set[str] = set()

    while queue:
        cur = queue.pop(0)
        key = str(cur.resolve())
        if key in scanned:
            continue
        scanned.add(key)
        for dep in otool_deps(cur):
            resolved = resolve_dep(dep, cur)
            if resolved is None:
                continue
            soname, src = resolved
            if soname not in needed:
                needed[soname] = src
                queue.append(src)

    for soname, src in sorted(needed.items()):
        dest = frameworks / soname
        copy_lib(src, dest)
        set_id(dest, f"@rpath/{soname}")

    # Rewrite references; pull in any newly discovered @rpath deps
    for _ in range(8):
        added = False
        targets = [binary] + sorted(frameworks.glob("*.dylib"))
        for tgt in targets:
            for dep in otool_deps(tgt):
                if is_system(dep):
                    continue
                resolved = resolve_dep(dep, tgt)
                if resolved is None:
                    if dep.startswith("@rpath/"):
                        name = Path(dep).name
                        if (frameworks / name).is_file() and dep != f"@rpath/{name}":
                            change(tgt, dep, f"@rpath/{name}")
                        elif not (frameworks / name).is_file():
                            src = expand_rpath(dep, tgt)
                            if src and src.is_file():
                                copy_lib(src, frameworks / name)
                                set_id(frameworks / name, f"@rpath/{name}")
                                needed[name] = src
                                added = True
                                change(tgt, dep, f"@rpath/{name}")
                    continue
                soname, src = resolved
                if not (frameworks / soname).is_file():
                    copy_lib(src, frameworks / soname)
                    set_id(frameworks / soname, f"@rpath/{soname}")
                    needed[soname] = src
                    added = True
                new = f"@rpath/{soname}"
                if dep != new:
                    change(tgt, dep, new)
            add_rpath(tgt, "@executable_path/../Frameworks")
            if tgt.suffix == ".dylib" or ".dylib" in tgt.name:
                add_rpath(tgt, "@loader_path")
        if not added:
            # still rewrite pass may have changes; one clean verify loop
            break

    bad = []
    for tgt in [binary] + list(frameworks.glob("*.dylib")):
        for dep in otool_deps(tgt):
            if is_system(dep):
                continue
            if dep.startswith("@rpath/"):
                name = Path(dep).name
                if not (frameworks / name).is_file():
                    bad.append(f"{tgt.name} -> {dep} (missing)")
                continue
            if dep.startswith("/") and not is_system(dep):
                bad.append(f"{tgt.name} -> {dep}")

    if bad:
        print("ERROR: unresolved deps:", file=sys.stderr)
        for b in bad:
            print(f"  {b}", file=sys.stderr)
        raise SystemExit(1)

    print(f"Bundled {len(list(frameworks.glob('*.dylib')))} dylibs into {frameworks}")


def main() -> None:
    if len(sys.argv) != 3:
        print(__doc__.strip(), file=sys.stderr)
        raise SystemExit(2)
    bundle_app(Path(sys.argv[1]).resolve(), sys.argv[2])


if __name__ == "__main__":
    main()

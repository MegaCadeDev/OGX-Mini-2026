#!/usr/bin/env python3
"""Rules learnt from bugs that only showed on hardware, checked in the source (CI job "rules").

1. Every Core1 entry of a board (Firmware/RP2040/src/OGXMini/Board/*.cpp, functions named
   core1_task*) registers itself for the flash lockout (multicore_lockout_victim_init() or
   flash_safe_execute_core_init()) before anything else runs. Without it, a settings write ran
   while Core1 executed from flash, which could hang it (UserSettings/NVSTool.h).
"""
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
LOCKOUT = re.compile(r"\b(multicore_lockout_victim_init|flash_safe_execute_core_init)\s*\(")


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def function_bodies(text, name_pattern):
    for match in re.finditer(r"^\s*(?:static\s+)?void\s+(" + name_pattern + r")\s*\(\s*\)\s*\{", text, re.M):
        depth, i = 1, match.end()
        while depth and i < len(text):
            depth += {"{": 1, "}": -1}.get(text[i], 0)
            i += 1
        yield match.group(1), text[match.end():i]


def main():
    errors = []
    for path in sorted((REPO / "Firmware/RP2040/src/OGXMini/Board").glob("*.cpp")):
        text = strip_comments(path.read_text())
        for name, body in function_bodies(text, r"core1_task\w*"):
            first = LOCKOUT.search(body)
            if not first:
                errors.append(f"{path.relative_to(REPO)}: {name}() never registers for the flash lockout")
                continue
            before = body[:first.start()]
            calls = [c for c in re.findall(r"\b(\w+)\s*\(", before) if c not in ("OGXM_LOG", "printf")]
            if calls:
                errors.append(f"{path.relative_to(REPO)}: {name}() calls {', '.join(calls)} before the flash lockout setup")
    for e in errors:
        print(f"check_firmware_rules: {e}", file=sys.stderr)
    if errors:
        return 1
    print("Firmware rules OK: every Core1 entry registers for the flash lockout first.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

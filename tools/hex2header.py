"""Bettet eine Intel-HEX-Datei als C-String in src/firmware_embedded.h ein.

Läuft automatisch vor jedem Build (platformio.ini: extra_scripts = pre:...).
Die Quelldatei wird über `custom_target_hex` in platformio.ini gewählt.
Standalone: python3 tools/hex2header.py [firmware/target.hex]
"""

import os
import sys

DEFAULT_HEX = "firmware/target.hex"
HEADER = "src/firmware_embedded.h"


def build_header(hex_path):
    name = os.path.basename(hex_path)
    if os.path.isfile(hex_path):
        with open(hex_path, encoding="ascii") as f:
            lines = [l.strip() for l in f if l.strip()]
    else:
        print(f"hex2header: {hex_path} fehlt – eingebettete Firmware bleibt leer")
        lines = []

    body = "\n".join(f'    "{l}\\n"' for l in lines) or '    ""'
    return (
        "// Automatisch erzeugt von tools/hex2header.py – nicht von Hand bearbeiten.\n"
        "#pragma once\n\n"
        f'static const char EMBEDDED_HEX_NAME[] = "{name}";\n'
        "static const char EMBEDDED_HEX[] =\n"
        f"{body};\n"
    )


def write_if_changed(path, content):
    if os.path.isfile(path):
        with open(path, encoding="utf-8") as f:
            if f.read() == content:
                return
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"hex2header: {path} aktualisiert")


def run(project_dir, hex_rel):
    write_if_changed(
        os.path.join(project_dir, HEADER),
        build_header(os.path.join(project_dir, hex_rel)),
    )


try:
    Import("env")  # noqa: F821 – von PlatformIO/SCons bereitgestellt
    run(env.subst("$PROJECT_DIR"), env.GetProjectOption("custom_target_hex", DEFAULT_HEX))  # noqa: F821
except NameError:
    if __name__ == "__main__":
        here = os.path.dirname(os.path.abspath(sys.argv[0]))
        run(os.path.dirname(here), sys.argv[1] if len(sys.argv) > 1 else DEFAULT_HEX)

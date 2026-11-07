#!/usr/bin/env python3
"""Fa fallire il build quando un modulo include un modulo da cui non dipende.

Uso: check_modules.py <src> <modules.txt>

modules.txt lo scrive CMake da lucerna_module(): una riga "Modulo: Dip1 Dip2" per modulo.
Regole:
  - un file di src/<Modulo>/ puo' includere "<Altro>/..." solo se Altro e' tra le sue DEPS;
  - ImGui e ImGuizmo si includono solo dall'Editor;
  - in cima a src ci stanno solo Main.cpp e ptpch.h.
"""
import pathlib
import re
import sys

INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]')
IMGUI = re.compile(r'^(imgui|imgui_\w+|ImGuizmo)\.h$')
TOP_LEVEL = {"Main.cpp", "ptpch.h"}


def main() -> int:
    src = pathlib.Path(sys.argv[1])
    deps = {}
    for line in pathlib.Path(sys.argv[2]).read_text().splitlines():
        if ":" in line:
            name, rest = line.split(":", 1)
            deps[name.strip()] = set(rest.replace(";", " ").split())  # CMake separa le liste con ;

    errors = []
    for f in src.iterdir():
        if f.is_file() and f.suffix in (".h", ".cpp") and f.name not in TOP_LEVEL:
            errors.append(f"{f}: in cima a src vanno solo {', '.join(sorted(TOP_LEVEL))}")

    for module, allowed in deps.items():
        for f in sorted((src / module).rglob("*")):
            if f.suffix not in (".h", ".cpp"):
                continue
            for n, line in enumerate(f.read_text(errors="replace").splitlines(), 1):
                m = INCLUDE.match(line)
                if not m:
                    continue
                path = m.group(1)
                head = path.split("/")[0]
                if head in deps and head != module and head not in allowed:
                    errors.append(f"{f}:{n}: {module} non dipende da {head} (include \"{path}\")")
                if IMGUI.match(path.split("/")[-1]) and module != "Editor":
                    errors.append(f"{f}:{n}: ImGui si usa solo nell'Editor (include \"{path}\")")

    for e in errors:
        print(e, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())

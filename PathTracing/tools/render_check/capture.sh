#!/bin/bash
# Cattura deterministica: PNG, HDR lineare e tempi GPU per ogni scena.
# Uso: capture.sh <cartella-output> [scene...]
# Ambiente: FRAMES (default 600), piu' qualunque PT_* da passare all'app.
set -u
out=$(mkdir -p "$1" && cd "$1" && pwd); shift
scenes=${*:-2 5 6 8}
res=$(cd "$(dirname "$0")/../../res" && pwd)
ini=$(mktemp); cp "$res/imgui.ini" "$ini"   # l'app riscrive il layout a ogni avvio

for s in $scenes; do
    (cd "$res" && PT_NOVSYNC=1 PT_VIEWPORT=${PT_VIEWPORT:-1280x720} PT_SCENE=$s PT_SHOT="$out/s$s.png" \
        PT_HDR="$out/s$s.hdr" PT_SHOT_FRAME=${FRAMES:-600} PT_LOG_FPS=1 timeout 240 ./Lucerna > "$out/s$s.log" 2>&1)
    rc=$?
    ms=$(grep PTFPS "$out/s$s.log" | awk '{print $4}' | sort -n | awk '{a[NR]=$1} END{print a[int((NR+1)/2)]}')
    # sotto 5 FPS il compositor sta rallentando la finestra coperta: tempi non validi.
    # La prima riga e' esclusa perche' copre il caricamento della scena.
    slow=$(grep PTFPS "$out/s$s.log" | awk 'NR>1 && $2<5' | wc -l)
    echo "scena $s rc=$rc pt_ms=$ms throttled=$slow errori=$(grep -c ERROR "$out/s$s.log")"
done

cp "$ini" "$res/imgui.ini"; rm -f "$ini"

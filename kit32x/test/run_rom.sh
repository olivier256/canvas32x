#!/bin/sh
# =========================================================================
# Lance une ROM 32X dans BlastEm instrumenté, en headless.
# Usage : run_rom.sh ROM TRAMES [SORTIE] [CAPTURE_TOUTES_LES_N]
#   ROM        : la ROM .32x
#   TRAMES     : trames réelles à émuler (60 = 1 s)
#   SORTIE     : répertoire des résultats (défaut : out_<nom de la ROM>)
#   N          : une capture PPM toutes les N trames réelles (défaut : 60,
#                0 = aucune)
# Il faut les BIOS 32X dans le répertoire BIOS (défaut : ~/bios) :
#   32X_G_BIOS.bin, 32X_M_BIOS.bin, 32X_S_BIOS.bin
# Résultats dans SORTIE : frame_XXXXX.ppm, fs.log (VBlanks, échanges FS),
# sh2_state.txt (PC des SH2, COMM, FBCR à la fin), puis un résumé.
#
# Attention : en headless, BlastEm compte chaque trame Mega Drive DEUX
# fois. Ce script double donc -b et la période de capture ; les numéros
# des fichiers frame_XXXXX sont ceux de BlastEm (doublés).
# =========================================================================
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
BLASTEM=${BLASTEM:-$HOME/emu/blastem-v1.0.0/blastem}
BIOS=${BIOS:-$HOME/bios}
ROM=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
FRAMES=$2
OUT=${3:-out_$(basename "$1" .32x)}
EVERY=${4:-60}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

cd "$BIOS"    # BlastEm cherche les BIOS dans le répertoire courant
SDL_AUDIODRIVER=dummy BLASTEM_FS_LOG=1 BLASTEM_SH2_STATE=1 \
  BLASTEM_DUMP_DIR="$OUT" BLASTEM_DUMP_EVERY=$((EVERY * 2)) \
  timeout 600 "$BLASTEM" -b $((FRAMES * 2)) -m 32x "$ROM" \
  > "$OUT/stdout.txt" 2> "$OUT/stderr.txt" || true
grep -E "^(FS|VBLK)" "$OUT/stderr.txt" > "$OUT/fs.log" || true
grep -A2 "^master PC" "$OUT/stderr.txt" > "$OUT/sh2_state.txt" || true
python3 -I "$HERE/fs_stats.py" "$OUT/fs.log"
cat "$OUT/sh2_state.txt"

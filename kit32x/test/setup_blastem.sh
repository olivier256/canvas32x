#!/bin/sh
# =========================================================================
# Installe BlastEm 1.0.0 instrumenté, pour tester des ROM 32X en headless.
#   - dépendances Ubuntu (SDL2, GLEW, zlib) ;
#   - sources du tag v1.0.0 depuis retrodev.com ;
#   - patch d'instrumentation (captures PPM, journal FS/VBlank, état SH2) ;
#   - compilation sans Nuklear, zstd ni lzma (sans OpenGL ne compile pas).
# Usage : setup_blastem.sh [répertoire]   (défaut : ~/emu)
# Résultat : <répertoire>/blastem-v1.0.0/blastem
# =========================================================================
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DEST=${1:-$HOME/emu}
mkdir -p "$DEST"
cd "$DEST"

apt-get install -y libsdl2-dev libglew-dev libgl-dev zlib1g-dev pkg-config >/dev/null

if [ ! -d blastem-v1.0.0 ]; then
  curl -sS -m 300 -o blastem-v1.0.0.tar.gz \
    https://www.retrodev.com/repos/blastem/archive/v1.0.0.tar.gz
  tar xzf blastem-v1.0.0.tar.gz
  (cd blastem-v1.0.0 && patch -p1 < "$HERE/blastem-1.0.0-instrumentation.patch")
fi
cd blastem-v1.0.0
make -j"$(nproc)" NONUKLEAR=1 NOZSTD=1 NOLZMA=1 blastem >build.log 2>&1 \
  || { tail -20 build.log; exit 1; }
echo "$DEST/blastem-v1.0.0/blastem"

#!/bin/sh
# =========================================================================
# kit32x : construit, une seule fois, les outils de compilation 32X dans
# ~/opt/cross (ou PREFIX) :
#   - binutils + gcc sh-elf : C seulement, sans bibliothèque C, pour SH2
#     gros-boutiste (-m2 -mb par défaut), avec sa libgcc (division) ;
#   - binutils m68k-elf (as, ld, objcopy pour md68k.s).
#
# macOS (Apple Silicon ou Intel) : il faut les outils en ligne de commande
# Xcode (xcode-select --install) et, par Homebrew : gmp mpfr libmpc.
#     brew install gmp mpfr libmpc
# Linux (Debian, Ubuntu) : apt install build-essential libgmp-dev
#     libmpfr-dev libmpc-dev zlib1g-dev curl xz-utils
#
# Usage : kit32x/build-cross.sh        (20 à 40 minutes selon la machine)
# Après un échec : effacer le dossier build-* indiqué dans ~/opt/cross-build
# et relancer ; les sources déjà téléchargées sont gardées.
# Ensuite, le Makefile du projet trouve les outils tout seul (il ajoute
# ~/opt/cross/bin au PATH).
# =========================================================================
set -e

PREFIX=${PREFIX:-$HOME/opt/cross}
BINUTILS=binutils-2.45.1
GCC=gcc-15.3.0
MIRROR=${MIRROR:-https://ftp.gnu.org/gnu}
WORK=${WORK:-$HOME/opt/cross-build}

if [ "$(uname)" = Darwin ]; then
  JOBS=$(sysctl -n hw.ncpu)
  command -v brew >/dev/null || { echo "Homebrew manque" >&2; exit 1; }
  for lib in gmp mpfr libmpc; do
    brew --prefix --installed $lib >/dev/null 2>&1 ||
      { echo "Il manque $lib : brew install gmp mpfr libmpc" >&2; exit 1; }
  done
  GCC_LIBS="--with-gmp=$(brew --prefix gmp) --with-mpfr=$(brew --prefix mpfr)
            --with-mpc=$(brew --prefix libmpc)"
else
  JOBS=$(nproc)
  GCC_LIBS=""
fi

mkdir -p "$WORK" "$PREFIX"
cd "$WORK"

# ---- Sources ----------------------------------------------------------------
fetch() {   # fetch répertoire archive
  if [ ! -d "$2" ]; then
    [ -f "$2.tar.xz" ] || curl -fsSL --retry 3 -o "$2.tar.xz" "$MIRROR/$1/$2.tar.xz"
    tar xJf "$2.tar.xz"
  fi
}
fetch binutils "$BINUTILS"
fetch "gcc/$GCC" "$GCC"

export PATH="$PREFIX/bin:$PATH"

# ---- binutils ---------------------------------------------------------------
# MAKEINFO=true : pas besoin de texinfo pour la documentation.
# --with-system-zlib : la copie de zlib fournie avec binutils et gcc est
# trop ancienne pour le SDK des macOS récents (fdopen redéfini) ; celle du
# système convient partout.
binutils() {   # binutils cible
  if [ ! -x "$PREFIX/bin/$1-as" ]; then
    rm -rf "build-binutils-$1"
    mkdir "build-binutils-$1"
    (cd "build-binutils-$1" &&
      "../$BINUTILS/configure" --target="$1" --prefix="$PREFIX" \
        --disable-nls --disable-werror --disable-gdb --disable-sim \
        --disable-gprofng --disable-gprof --with-system-zlib \
        >configure.log 2>&1 &&
      make -j"$JOBS" MAKEINFO=true >make.log 2>&1 &&
      make install MAKEINFO=true >install.log 2>&1) ||
      { echo "Échec de binutils $1 : voir $WORK/build-binutils-$1/*.log" >&2
        exit 1; }
  fi
  echo "binutils $1 : ok"
}
binutils sh-elf
binutils m68k-elf

# ---- gcc sh-elf -------------------------------------------------------------
# Un seul processeur (SH2, gros-boutiste) : pas de multilib. Seuls le
# compilateur C et libgcc sont construits. libgcc sans -j : certaines de
# ses règles propres au SH (unwind-dw2-Os-4-200.o) oublient de dépendre
# de libgcc_tm.h, et échouent si make les lance trop tôt.
if [ ! -x "$PREFIX/bin/sh-elf-gcc" ]; then
  rm -rf build-gcc
  mkdir build-gcc
  # shellcheck disable=SC2086
  (cd build-gcc &&
    "../$GCC/configure" --target=sh-elf --prefix="$PREFIX" \
      --with-cpu=m2 --with-endian=big --disable-multilib \
      --enable-languages=c --without-headers --with-newlib \
      --disable-nls --disable-shared --disable-threads --disable-libssp \
      --disable-libquadmath --disable-libgomp --disable-libatomic \
      --disable-decimal-float --disable-lto --with-system-zlib $GCC_LIBS \
      >configure.log 2>&1 &&
    make -j"$JOBS" all-gcc MAKEINFO=true >make.log 2>&1 &&
    make all-target-libgcc MAKEINFO=true >>make.log 2>&1 &&
    make install-gcc install-target-libgcc MAKEINFO=true >install.log 2>&1) ||
    { echo "Échec de gcc : voir $WORK/build-gcc/*.log" >&2; exit 1; }
fi
echo "gcc sh-elf : ok"

# ---- Vérification -----------------------------------------------------------
printf 'int f(int a, int b) { return a / b; }\n' > test.c
sh-elf-gcc -m2 -mb -O2 -c test.c -o test.o
sh-elf-gcc -m2 -mb -nostdlib -e _f -o test.elf test.o -lgcc
echo
"$PREFIX/bin/sh-elf-gcc" --version | head -1
"$PREFIX/bin/m68k-elf-as" --version | head -1
echo "Outils installés dans $PREFIX/bin. Les sources ($WORK) peuvent être effacées."

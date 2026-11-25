# Historique

## 0.1 : canvas32x, le kit32x et sa mini-démo

Modèle de projet Sega 32X, à copier pour chaque nouveau projet : premier
environnement de développement 32X simple et réutilisable, tiré du portage
d'AMB96 (qui, lui, reste tel quel).

- `kit32x/` : démarrage des SH2 (`sh2_start.S`), carte mémoire
  (`sh2.ld`), programme 68000 (`md68k.s` : démarrage 32X, manette dans
  COMM0, option `AUTOPAD`), accès au matériel (`hw32x.h`, `hw32x.c` :
  registres, frame buffer, palette, échange au VBlank, `memcpy`/`memset`),
  conversion des images BMP 8 bits (`img2c.c`, outil hôte compilé par
  make), construction des compilateurs (`build-cross.sh`, sh-elf-gcc
  15.3.0 et binutils 2.45.1 dans `~/opt/cross`), banc BlastEm headless
  (`test/`).
- `Makefile` : `make` → `<projet>.32x`, compatible avec le make de macOS ;
  les `.S` de `src/` sont optionnels.
- README : nouveau projet, C et assembleur, travailler avec Claude.
- Mini-démo (`src/`) : un plan qui défile, un sprite, la manette ; les deux
  SH2 se partagent les lignes. Le plan est dessiné en mots longs (lectures
  alignées, décalage selon `scrollX & 3`), en C seul.
- Résultat dans BlastEm 1.0.0 : 60 images/s, dessin fini vers la ligne 40
  à 75 de la trame suivante ; captures identiques au rendu attendu pour
  les quatre phases du défilement.

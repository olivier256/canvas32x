# canvas32x : modèle de projet Sega 32X

Le point de départ de tout nouveau projet 32X : le kit (`kit32x/`), un
Makefile, et une mini-démo en C pour les deux SH2 (un plan de tiles qui
défile, un vaisseau, la manette), à remplacer par le jeu.

Ce dépôt ne se modifie pas pour un jeu : on le copie (voir « Nouveau
projet »). Une amélioration du kit faite dans un projet est reportée ici.

| Commande       | Effet                              |
|----------------|------------------------------------|
| Croix          | déplace le vaisseau                |
| C (maintenu)   | plus vite                          |
| A (maintenu)   | arrête le défilement               |
| B (maintenu)   | défilement à l'envers              |

## Construire

Une fois pour toutes, sur le Mac (20 à 40 minutes) :

```sh
xcode-select --install          # si ce n'est pas déjà fait
brew install gmp mpfr libmpc
kit32x/build-cross.sh           # → ~/opt/cross/bin
```

Puis, dans le projet, avec `icd_mars.bin` (l'ICD 32X de Sega, 1040
octets, non versionné) à la racine :

```sh
make                 # → canvas32x.32x, à ouvrir dans BlastEm
make AUTOPAD=0x48    # manette simulée (A + droite) → canvas32x-autopad.32x
make clean
```

Options : `TITLE="MON JEU"` (nom dans l'en-tête), `ROM_SIZE=524288`,
`EXTRA_CFLAGS=...`. Le Makefile marche avec le `make` de macOS (3.81).

## Organisation

```
Makefile           make → <nom du dossier>.32x
src/               le jeu, en C pour les SH2 (+ .S optionnels, compilés
                   automatiquement s'il y en a)
  main.c           master_main() et slave_main() : la boucle de jeu
  plane.c/.h       une ligne de plan de tiles 16x16, défilement quelconque
  sprite.c/.h      sprites, index 0 transparent (zone overwrite)
assets/            images BMP 8 bits + assets.txt, convertis par make
kit32x/            identique d'un projet à l'autre
  sh2_start.S      démarrage des SH2 (vecteurs, poignée de main, BSS)
  sh2.ld           carte mémoire SH2 (tout en SDRAM, 224 Ko)
  md68k.s          côté 68000 : démarrage 32X, manette → COMM0
  hw32x.h/.c       registres, frame buffer, palette, échange, COMM
  assets_types.h   types produits par img2c (Tileset, Tilemap, Sprite)
  img2c.c          conversion des images (outil hôte, compilé par make)
  build-cross.sh   construit sh-elf-gcc et m68k-elf binutils
  test/            banc BlastEm 1.0.0 instrumenté, en headless
icd_mars.bin       non versionné (code Sega)
```

## Nouveau projet

```sh
cp -R ~/dev/canvas32x ~/dev/monjeu && cd ~/dev/monjeu
rm -rf .git build *.32x && git init
make                                 # → monjeu.32x (le nom vient du dossier)
```

`icd_mars.bin` est copié avec le reste (il n'est pas versionné, mais il
est dans le dossier). Ensuite, remplacer `src/` et `assets/` par le jeu :
il faut `master_main()` et `slave_main()`, qui ne reviennent jamais, et
`assets/assets.txt`. `kit32x/` et le `Makefile` restent tels quels.

## C et assembleur

Une fonction déclarée dans un `.h` peut être écrite en C (`.c`) ou en
assembleur (`.S`, avec un S majuscule : il passe par le préprocesseur) :
le Makefile compile tous les `.c` et `.S` de `src/`, l'éditeur de liens
relie les appels. Règles :

- une seule définition par fonction : pour garder la version C comme
  référence, l'entourer de `#ifndef MA_FONCTION_ASM` (ou la sortir de
  `src/`) ;
- en assembleur, le nom porte le `_` de gcc : `SYMBOL(nom)`, voir
  `kit32x/sh2_start.S` ; `#include "assets_types.h"` donne la position
  des champs des structures ;
- convention d'appel de gcc : arguments dans R4 à R7 (la suite sur la
  pile), résultat dans R0, R8 à R14 rendus intacts, PR sauvegardé si la
  routine en appelle une autre ;
- instructions SH2 seulement (`-Wa,--isa=sh2` les vérifie).

## Ce que fournit le kit

- **Démarrage** : la BootROM copie l'image SH2 (ROM 10000h) en SDRAM
  (0600 0000h) ; Master et Slave partent de `_start` puis appellent
  `master_main()` / `slave_main()`. Pas d'interruptions.
- **COMM0** : la manette (octet SACBRLDU, 1 = appuyé, `HW_PAD_*`), écrite
  par le 68000 à chaque image ; `hw_pad()` la lit. COMM1 à COMM7 sont
  libres pour le jeu.
- **Vidéo** : `hw_video_init(palette)` (tables de lignes 320 x 224,
  palette, mode Packed Pixel), `hw_flip()` (échange au VBlank, au plus une
  fois par image), `hw_fb16(x, y)` et `hw_fb16_overwrite(x, y)`.
- **Mémoire partagée entre SH2** : `HW_UNCACHED(&x)` et `HW_BARRIER()`.
- **Images** (`assets/assets.txt`) : `plane nom fichier.bmp` (découpé en
  tiles 16 x 16 dédupliquées, dimensions en tiles puissances de 2) et
  `sprite nom fichier.bmp`. make produit `build/assets.h` : variables
  `asset_<nom>`, `asset_tiles`, `asset_palette` (celle de la première
  image).

## Règles à connaître

- Frame buffer et palette : accès 16 ou 32 bits seulement, jamais
  `memcpy`/`memset`. `-fno-tree-loop-distribute-patterns` empêche gcc d'en
  mettre de lui-même.
- Palette : en mode Blank ou pendant les blanks.
- Les octets nuls sont ignorés dans la zone overwrite (2402 0000h).
- Un échange de frame buffer n'a lieu qu'au VBlank : ne jamais le supposer
  immédiat, même à l'initialisation.
- Une donnée écrite par un SH2 et lue par l'autre passe par
  `HW_UNCACHED()` ; le signal qui l'annonce (COMM) est écrit après.
- Image SH2 + BSS : 224 Ko au plus (l'éditeur de liens s'arrête sinon).

## Tester sans console (banc BlastEm headless)

```sh
kit32x/test/setup_blastem.sh ~/emu      # Linux : BlastEm 1.0.0 instrumenté
make test                               # 300 trames, résultats dans build/test
```

`run_rom.sh` donne les images/s et la ligne où le Master demande
l'échange (fin du dessin). BlastEm compte chaque trame deux fois en
headless (le script compense) et n'émule pas la FIFO du frame buffer : ses
mesures de vitesse sont un minimum, seule la console donne la vraie
vitesse. Dans les captures, la dernière ligne (223) montre déjà l'image
suivante : c'est l'instant de la capture, pas un défaut du dessin.

## Travailler avec Claude

La compétence « portage-32x » (compte Claude) décrit la méthode, cette
organisation et les pièges connus. Dans un nouveau chat, joindre :

- le projet en zip, sans `build/` ni ROM : `git archive -o monjeu.zip HEAD` ;
- les trois BIOS 32X (`32X_G_BIOS.bin`, `32X_M_BIOS.bin`,
  `32X_S_BIOS.bin`) et `icd_mars.bin` (1040 octets) : Claude repart d'un
  espace vide à chaque chat et ne peut pas les garder ;
- une phrase d'objectif (« porter le moteur de SuperVDP dans ce
  projet »).

Claude construit alors le banc BlastEm 1.0.0 instrumenté
(`kit32x/test/`), compile avec les paquets Ubuntu `gcc-sh-elf`,
`binutils-sh-elf` et `binutils-m68k-linux-gnu`, et vérifie la ROM en
headless.

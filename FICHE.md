# Fiche de démarrage : projet Sega 32X (canvas32x)

À coller en début de nouveau chat, avec :

- le projet en zip, sans `build/` ni ROM : `git archive -o projet.zip HEAD`
  (le modèle : https://github.com/olivier256/canvas32x) ;
- les trois BIOS 32X (`32X_G_BIOS.bin`, `32X_M_BIOS.bin`, `32X_S_BIOS.bin`)
  et `icd_mars.bin` (**1040 octets**, code 68000 commençant par
  `28 7C FF FF FF C0`) ;
- une phrase d'objectif.

La compétence « portage-32x » de mon compte décrit la même méthode.

## Où on en est

- **canvas32x** (dépôt GitHub) : modèle de projet 32X, versions 0.1 et 0.2.
  Il ne se modifie pas pour un jeu : on le copie. Une amélioration du kit
  faite dans un projet y est reportée.
- **Compilateur installé sur mon Mac** (Apple Silicon), une fois pour
  toutes : `~/opt/cross/bin`, gcc 15.3.0 `sh-elf` (C, libgcc SH2) et
  binutils 2.45.1 `sh-elf` + `m68k-elf`, construits par
  `kit32x/build-cross.sh`. Le Makefile les trouve tout seul (variable
  `CROSS`).
- **Mini-démo** (plan qui défile, vaisseau, manette) en C seul : 60
  images/s dans BlastEm 1.0.0, dessin fini vers la ligne 40 à 75 de la
  trame suivante, captures identiques au rendu attendu.
- Je teste les ROM dans **BlastEm 1.0.0** ; Claude les teste dans BlastEm
  1.0.0 instrumenté, en headless (`kit32x/test/`).

## Organisation d'un projet

```
projet/
  Makefile         make → projet.32x (nom du dossier) ; make AUTOPAD=0x48 ;
                   make test ; options TITLE=, ROM_SIZE=, EXTRA_CFLAGS=, CROSS=
  src/             le jeu en C pour les SH2 : master_main(), slave_main()
                   (ne reviennent jamais) ; + .S optionnels
  assets/          BMP 8 bits indexés + assets.txt (plane|sprite nom fichier)
  kit32x/          identique d'un projet à l'autre
    sh2_start.S, sh2.ld   démarrage et carte mémoire SH2 (224 Ko en SDRAM)
    md68k.s               68000 : démarrage 32X, manette → COMM0
    hw32x.h, hw32x.c      registres, frame buffer, palette, hw_flip(),
                          HW_UNCACHED(), memcpy/memset
    assets_types.h        Tileset, Tilemap, Sprite (tiles 16 x 16)
    img2c.c               conversion des images (outil hôte, compilé par make)
    build-cross.sh        construit les compilateurs dans ~/opt/cross
    test/                 banc BlastEm headless (côté Claude)
  icd_mars.bin     non versionné (code Sega), comme les BIOS
```

Nouveau projet :
```sh
git clone https://github.com/olivier256/canvas32x.git newgame && cd newgame
git remote remove origin        # ou rm -rf .git && git init
cp ~/dev/canvas32x/icd_mars.bin . && make
```

## C et assembleur

Une fonction déclarée dans un `.h` est définie soit dans un `.c`, soit
dans un `.S` de `src/` (S majuscule : préprocesseur) ; le Makefile compile
les deux. Une seule définition : garder la version C comme référence sous
`#ifndef NOM_ASM`. En assembleur : `SYMBOL(nom)` pour le `_` de gcc,
`#include "assets_types.h"` pour la position des champs ; arguments R4 à
R7, résultat R0, R8 à R14 rendus intacts, PR sauvegardé si appel ; SH2
seulement (`-Wa,--isa=sh2`). L'assembleur ne doit pas contraindre le C.

## Ce qu'on a appris (pièges)

**Compilation**
- Vrai SH2 : `sh-elf-gcc -m2 -mb` (Ubuntu : `apt install gcc-sh-elf
  binutils-sh-elf binutils-m68k-linux-gnu`). `sh4-linux-gnu-gcc` refuse
  `-m2` et produit du SH3/SH4 (`shad`, `shld`).
- `.S` avec `-Wa,--isa=sh2`. Préfixe `_` des symboles ; les symboles du
  script ld s'écrivent sans `SYMBOL()`.
- `-ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns` : sinon
  gcc met des `memcpy` (octet par octet) dans les boucles sur la palette ou
  le frame buffer. Lier avec `-lgcc` (division).
- Mots longs lus dans des tableaux d'octets : type
  `__attribute__((may_alias))`.

**Construction sur macOS** (corrigé en 0.2)
- binutils 2.45.1 et gcc 15.3.0 : leur zlib embarquée ne compile plus avec
  le SDK récent (`fdopen` redéfini) → `--with-system-zlib`.
- libgcc pour SH échoue avec `make -j` sur beaucoup de cœurs
  (`unwind-dw2-Os-4-200.o` sans dépendance sur `libgcc_tm.h`) → libgcc
  sans `-j`.
- Le make de macOS (3.81) ignore un `PATH` modifié dans le Makefile pour
  les commandes simples → outils appelés par leur chemin complet.
- Makefile compatible make 3.81 : pas de cibles groupées `&:`, pas de
  `truncate` (dd + head -c).

**Démarrage** (réglé dans le kit, à ne pas casser)
- En-tête MARS en 3C0h : source 10000h, destination 0, taille multiple de
  4, PC 0600 0240h, VBR 0600 0000h / 0600 0120h. ICD en 3F0h–7FFh, puis
  `bcc.w`. Après l'ICD, la cartouche est en 880000h pour le 68000 : code
  relogeable, table de sauts `jmp 880000h+…` en 200h.
- Poignée de main : 'M_OK' (COMM0-1) et 'S_OK' (COMM2-3) ; le 68000
  attend 'M_OK' et met FM à 1 ; le Master attend FM puis 'S_OK', efface la
  BSS puis COMM0-1 ; le Slave attend que COMM0-1 change, puis efface
  COMM2-3.
- FRT et purge du cache (CCR FFFF FE92h) au démarrage de chaque SH2.

**Vidéo 32X**
- Palette (2000 4200h) en accès 16 bits, en mode Blank ou pendant les
  blanks.
- Échange : attendre la fin du VBlank, écrire FS, attendre que FS change.
  Jamais immédiat, même à l'initialisation (`hw_flip()`).
- Octets nuls ignorés en zone overwrite (2402 0000h) : sprites à index 0
  transparent ; ne pas écrire les mots entièrement transparents.
- Écritures 32 bits dans le frame buffer : moitié moins d'instructions que
  16 bits, même charge sur le bus du VDP.
- Donnée partagée entre les SH2 : `HW_UNCACHED(&x)`, puis `HW_BARRIER()`,
  puis le signal COMM.

**Vitesse en C**
- Plan écrit mot par mot (reprise du C de référence d'AMB96, qui en ROM
  utilisait son `.S`) : 39 images/s. Le goulot est le nombre
  d'instructions, pas les lectures ni les écritures.
- Plan en mots longs (lectures alignées, `prev << 8p | cur >> (32 - 8p)`
  selon `scrollX & 3`, tile entière déroulée, une boucle par phase) : 60
  images/s en C seul.
- Regarder `sh-elf-objdump -d` avant de passer à l'assembleur ; isoler un
  coût avec des variantes `EXTRA_CFLAGS=-D...`.

**BlastEm**
- Sans OpenGL (`NOGL=1`), ne compile pas : il faut GLEW et GL.
- En headless, chaque trame compte deux fois (`-b 600` = 300 trames) :
  `run_rom.sh` compense.
- Pas de FIFO du frame buffer émulée : vitesse minimale ; seule la console
  donne la vraie.
- Dans une capture, la ligne 223 montre déjà l'image suivante : l'ignorer
  dans les comparaisons.
- Captures identiques ≠ blocage (zone uniforme) : vérifier COMM dans
  `sh2_state.txt`.
- Téléchargement de BlastEm parfois tronqué par le proxy : `gzip -t`,
  recommencer. Les BIOS vont dans `~/bios`, BlastEm dans `~/emu`.

## Pistes

- Porter SuperVDP, puis Glide32, dans un projet copié de canvas32x.
- PNG dans `img2c` (zlib du système).
- `kit32x/nouveau-projet.sh` : créer un projet en une commande.
- Mesurer la vraie vitesse sur console (FIFO du frame buffer).

## Préférences

- Me vouvoyer.
- Livrer des patches ou des fichiers, avec un message de commit proposé ;
  dans les dépôts qui en ont un, ajouter une section à la fin de
  `VERSION.md`.
- L'assembleur doit rester optionnel : il ne doit pas contraindre le C.
- Ne jamais versionner les BIOS ni l'ICD (code Sega).
- Sauvegarde hors GitHub : `git bundle create … --all` dans iCloud (jamais
  de dépôt git directement dans iCloud Drive).

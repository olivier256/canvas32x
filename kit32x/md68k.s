| =========================================================================
| kit32x : programme 68000 de la cartouche (GNU as, m68k).
|
| Rôle du 68000 : démarrer la 32X, donner le VDP 32X aux SH2, puis lire
| la manette à chaque image et l'écrire dans COMM0 (HW_COMM_PAD de
| hw32x.h). Le VDP Mega Drive reste libre (fond noir).
|
| Carte de la ROM :
|   000000h  vecteurs 68000, en-tête Mega Drive (100h)
|   000200h  table de sauts (la ROM de vecteurs 32X saute ici + 6 x n)
|   0003C0h  en-tête MARS : où trouver l'image SH2, où la copier
|   0003F0h  ICD : programme de démarrage et de sécurité 32X (Sega),
|            1040 octets (icd_mars.bin, non fourni avec le kit)
|   000800h  suite du programme 68000
|   010000h  image SH2 (sh2.ld)
|
| Après l'ICD, la cartouche n'est plus en 000000h mais en 880000h : tout
| le code ci-dessous n'utilise que des branchements relatifs (bra, bsr,
| bcc) et des adresses de registres, jamais l'adresse d'une étiquette.
|
| Donnés par le Makefile (--defsym) : SH2_SIZE (taille de l'image SH2,
| multiple de 4), ROM_SIZE, et en option AUTOPAD. Le nom du jeu vient de
| title.inc, écrit par le Makefile (variable TITLE).
| =========================================================================

        .equ    CART,       0x880000    | la cartouche, vue après l'ICD

        .equ    CTRL1,      0xA10009
        .equ    DATA1,      0xA10003
        .equ    Z80_BUSREQ, 0xA11100
        .equ    VDP_DATA,   0xC00000
        .equ    VDP_CTRL,   0xC00004

        .equ    ADAPTER,    0xA15100    | octet fort : FM (bit 7)
        .equ    COMM0,      0xA15120    | COMM0-1 (mot long)

        .equ    M_OK,       0x4D5F4F4B  | 'M_OK'

        .text
        .org    0

| ---- Vecteurs 68000 ------------------------------------------------------
| Au reset, la cartouche est encore en 000000h : PC = boot. Ensuite, la
| ROM de vecteurs 32X remplace ces vecteurs par la table de sauts (200h).

        .long   0x01000000              | SSP
        .long   boot                    | PC
        .rept   62
        .long   CART+exception_rte
        .endr

| ---- En-tête Mega Drive --------------------------------------------------

        .org    0x100
        .ascii  "SEGA 32X        "      | Console
        .ascii  "(C)KIT  2026.NOV"      | Copyright
        .include "title.inc"            | Domestique et étranger (2 x 48)
        .ascii  "GM MK-0000 -00"        | Numéro de série
        .word   0                       | Somme de contrôle (non vérifiée)
        .ascii  "J               "      | Entrées-sorties : manette
        .long   0, ROM_SIZE-1           | ROM
        .long   0xFF0000, 0xFFFFFF      | RAM
        .ascii  "            "          | Modem (12)
        .ascii  "                                                    " | Mémo (52)
        .ascii  "JUE             "      | Pays

| ---- Table de sauts ------------------------------------------------------
| Entrée 0 : reset ; les autres : exceptions et interruptions, inutilisées
| (SR = 2700h, tout masqué).

        .org    0x200
        jmp     CART+icd
        .rept   63
        jmp     CART+exception_rte
        .endr

boot:
        cmp.l   #0x4D415253, 0xA130EC   | 'MARS' : 32X présente ?
        bne.w   no_32x
        bra.w   icd

| ---- En-tête MARS --------------------------------------------------------

        .org    0x3C0
        .ascii  "MARS CHECK MODE "      | Nom du module
        .long   0                       | Version
        .long   0x10000                 | Source : image SH2 dans la ROM
        .long   0                       | Destination : début de la SDRAM
        .long   SH2_SIZE                | Taille
        .long   0x06000240              | PC de départ du Master
        .long   0x06000240              | PC de départ du Slave
        .long   0x06000000              | VBR du Master
        .long   0x06000120              | VBR du Slave

| ---- ICD (Sega), 1040 octets ---------------------------------------------

        .org    0x3F0
icd:
        .incbin "icd_mars.bin"
        .if     . - icd - 1040
        .error  "icd_mars.bin doit faire 1040 octets"
        .endif

| L'ICD revient ici : retenue à 0 si la 32X a démarré
        bcc.w   init_32x

no_32x:
        bra.s   no_32x

exception_rte:
        rte

| ---- Démarrage -----------------------------------------------------------

init_32x:
        move.w  #0x2700, %sr            | aucune interruption

        | Manette 1 : TH (PD6) en sortie, le reste en entrée
        move.w  #0x100, Z80_BUSREQ
        move.b  #0x40, CTRL1
        move.b  #0x40, DATA1            | TH haut au repos
        move.w  #0, Z80_BUSREQ

        | VDP Mega Drive : mode 5, H40, affichage, fond noir (couleur 0).
        | Il reste disponible pour autre chose : la 32X s'affiche devant.
        lea     VDP_CTRL, %a1
        move.w  #0x8004, (%a1)          | pas de H_INT
        move.w  #0x8144, (%a1)          | affichage, mode 5, pas de V_INT
        move.w  #0x8700, (%a1)          | couleur de fond : palette 0, 0
        move.w  #0x8C81, (%a1)          | H40
        move.w  #0x8F02, (%a1)          | auto-incrément 2
        move.l  #0xC0000000, (%a1)      | CRAM 0
        move.w  #0x0000, VDP_DATA       | noir

        | Attendre la BootROM 32X, puis donner le VDP 32X aux SH2
1:      cmp.l   #M_OK, COMM0
        bne.s   1b
        ori.b   #0x80, ADAPTER          | FM = 1

        | Attendre que le Master ait démarré (il efface 'M_OK')
1:      cmp.l   #M_OK, COMM0
        beq.s   1b

| ---- Boucle : une lecture de manette par image ----------------------------

main_loop:
        | Attendre le début du VBlank du VDP Mega Drive (bit 3 du statut)
1:      btst    #3, VDP_CTRL+1
        bne.s   1b                      | encore dans le VBlank précédent
1:      btst    #3, VDP_CTRL+1
        beq.s   1b                      | affichage en cours

        .ifdef  AUTOPAD
        move.w  #AUTOPAD, %d0           | test : manette simulée (make AUTOPAD=)
        .else
        bsr.s   read_pad
        .endif
        move.w  %d0, COMM0              | COMM_PAD
        bra.s   main_loop

| Lit la manette 1 (3 boutons)
| Sortie : d0.w = SACBRLDU, 1 = appuyé (HW_PAD_* de hw32x.h)
read_pad:
        lea     DATA1, %a0
        move.w  #0x100, Z80_BUSREQ
        move.b  #0x40, (%a0)            | TH haut
        nop
        nop
        move.b  (%a0), %d0              | ?1CBRLDU
        move.b  #0x00, (%a0)            | TH bas
        nop
        nop
        move.b  (%a0), %d1              | ?0SA00DU
        move.b  #0x40, (%a0)            | TH haut au repos
        move.w  #0, Z80_BUSREQ

        andi.w  #0x3F, %d0              | 00CBRLDU
        andi.w  #0x30, %d1              | 00SA0000
        lsl.w   #2, %d1                 | SA000000
        or.w    %d1, %d0                | SACBRLDU
        not.b   %d0                     | 1 = appuyé
        rts

        .org    0x10000                 | l'image SH2 suit (Makefile)

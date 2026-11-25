#ifndef HW32X_H
#define HW32X_H

/* =========================================================================
   kit32x : accès au matériel 32X depuis le C des SH2.

   Adresses vues du SH2 (32X Hardware Manual). Toutes celles-ci sont
   « cache-through » (bit 29 à 1) : jamais de copie en cache.

     2000 4000h  registres système (FM, interruptions, COMM)
     2000 4100h  registres du VDP 32X (mode, FBCR...)
     2000 4200h  palette, 256 mots
     2200 0000h  la ROM de la cartouche (lecture seule)
     2400 0000h  frame buffer de dessin (DRAM)
     2402 0000h  le même, en zone overwrite (octets nuls ignorés)
     0600 0000h  SDRAM (256 Ko), avec cache ; 2600 0000h sans cache

   Règles du VDP 32X :
   - frame buffer et palette : accès 16 bits (ou 32) ; jamais de memcpy
     ni de memset (qui écrivent octet par octet) ;
   - palette : en mode Blank, ou pendant les blanks ;
   - l'échange de frame buffer (FS) n'est pris en compte qu'au VBlank :
     hw_flip() s'en occupe.
   ========================================================================= */

#include <stddef.h>
#include <stdint.h>

/* ---- Registres système ---------------------------------------------- */

#define HW_ADAPTER (*(volatile uint16_t *)0x20004000) /* FM : bit 15 */
#define HW_VINT_CLEAR (*(volatile uint16_t *)0x20004016)

/* COMM0-7 : huit mots partagés par le 68000 et les deux SH2. Le kit en
   réserve un : COMM[HW_COMM_PAD], écrit par le 68000 à chaque image. */
#define HW_COMM ((volatile uint16_t *)0x20004020)
#define HW_COMM_PAD 0

/* ---- VDP 32X -------------------------------------------------------- */

#define HW_BITMAP_MODE (*(volatile uint16_t *)0x20004100)
#define HW_SHIFT (*(volatile uint16_t *)0x20004102)
#define HW_FBCR (*(volatile uint16_t *)0x2000410A)
#define HW_PALETTE ((volatile uint16_t *)0x20004200)

/* Mode (bits 0-1 de HW_BITMAP_MODE) */
#define HW_MODE_BLANK 0
#define HW_MODE_PACKED 1 /* un octet par pixel, palette de 256 couleurs */
#define HW_MODE_DIRECT 2 /* un mot par pixel, 0bbbbbgggggrrrrr */
#define HW_MODE_RLE 3

/* FBCR */
#define HW_FBCR_VBLK 0x8000 /* VBlank en cours */
#define HW_FBCR_HBLK 0x4000 /* HBlank en cours */
#define HW_FBCR_PEN 0x2000  /* palette accessible */
#define HW_FBCR_FS 0x0001   /* frame buffer affiché */

/* ---- Frame buffer --------------------------------------------------- */

#define HW_SCREEN_WIDTH 320
#define HW_SCREEN_HEIGHT 224 /* NTSC (240 lignes : PAL seulement) */

/* Le frame buffer de dessin commence par la table de lignes : un mot par
   ligne, l'adresse (en mots) de ses pixels. hw_video_init() la remplit
   pour une image de 320 x 224 en mode Packed Pixel, les lignes à la suite
   à partir de HW_FB_LINES (en octets). */
#define HW_FB_LINES 512

#define HW_FB ((volatile uint8_t *)0x24000000)
#define HW_FB_OVERWRITE ((volatile uint8_t *)0x24020000)

/* Position du pixel (x, y) en octets depuis le début du frame buffer */
static inline uint32_t hw_fb_offset(int x, int y) {
  return (uint32_t)(HW_FB_LINES + y * HW_SCREEN_WIDTH + x);
}

/* Pointeurs sur le pixel (x, y), pour des écritures 16 bits (x pair) */
static inline volatile uint16_t *hw_fb16(int x, int y) {
  return (volatile uint16_t *)(HW_FB + hw_fb_offset(x, y));
}
static inline volatile uint16_t *hw_fb16_overwrite(int x, int y) {
  return (volatile uint16_t *)(HW_FB_OVERWRITE + hw_fb_offset(x, y));
}

/* ---- Manette ---------------------------------------------------------
   Lue par le 68000 (md68k.s) à chaque VBlank Mega Drive, 1 = appuyé. */

#define HW_PAD_UP 0x01
#define HW_PAD_DOWN 0x02
#define HW_PAD_LEFT 0x04
#define HW_PAD_RIGHT 0x08
#define HW_PAD_B 0x10
#define HW_PAD_C 0x20
#define HW_PAD_A 0x40
#define HW_PAD_START 0x80

static inline unsigned hw_pad(void) { return HW_COMM[HW_COMM_PAD]; }

/* ---- Mémoire partagée entre les SH2 ----------------------------------
   Chaque SH2 a son cache : ce qu'écrit l'un peut rester invisible de
   l'autre. Pour une donnée partagée, passer par son adresse sans cache :
   HW_UNCACHED(&x). HW_BARRIER() empêche gcc de déplacer les accès
   mémoire de part et d'autre (entre les données et le signal COMM). */

#define HW_UNCACHED(p) ((__typeof__(p))((uintptr_t)(p) | 0x20000000u))
#define HW_BARRIER() __asm__ volatile("" ::: "memory")

/* ---- Fonctions (hw32x.c) ---------------------------------------------- */

/* À appeler par le Master seul, au démarrage : table de lignes dans les
   deux frame buffers, palette (256 couleurs 0bbbbbgggggrrrrr), puis mode
   Packed Pixel. */
void hw_video_init(const uint16_t *palette);

/* Échange les frame buffers et attend que ce soit fait (au VBlank) :
   au plus un échange par image, 60 images/s au mieux. Master seul. */
void hw_flip(void);

/* gcc peut les appeler de lui-même (copie de structure) : pas de
   bibliothèque C, le kit les fournit. Jamais sur le frame buffer ni la
   palette (accès octet par octet). */
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);

#endif

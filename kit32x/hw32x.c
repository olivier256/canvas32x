/* =========================================================================
   kit32x : les quelques fonctions qui ne tiennent pas dans hw32x.h.
   ========================================================================= */

#include "hw32x.h"

/* Remplit la table de lignes du frame buffer de dessin */
static void initLineTable(void) {
  volatile uint16_t *table = (volatile uint16_t *)HW_FB;
  for (int y = 0; y < HW_SCREEN_HEIGHT; y++)
    table[y] = (uint16_t)(hw_fb_offset(0, y) / 2);
}

void hw_video_init(const uint16_t *palette) {
  /* Mode Blank : palette accessible à tout moment */
  HW_BITMAP_MODE = (uint16_t)((HW_BITMAP_MODE & ~3u) | HW_MODE_BLANK);

  /* Une table de lignes dans chacun des deux frame buffers. Même en mode
     Blank, l'échange n'est pas garanti immédiat : hw_flip() attend. */
  initLineTable();
  hw_flip();
  initLineTable();

  for (int i = 0; i < 256; i++)
    HW_PALETTE[i] = palette[i];

  HW_BITMAP_MODE = (uint16_t)((HW_BITMAP_MODE & ~3u) | HW_MODE_PACKED);
}

/* Une demande d'échange faite pendant le VBlank prend effet aussitôt.
   Si l'image a été dessinée en moins d'un VBlank, deux échanges auraient
   lieu dans le même : on attend donc la fin du VBlank, on demande
   l'échange (fait au VBlank suivant), puis on attend qu'il soit fait. */
void hw_flip(void) {
  while (HW_FBCR & HW_FBCR_VBLK)
    ;
  const uint16_t wanted = (uint16_t)((HW_FBCR ^ HW_FBCR_FS) & HW_FBCR_FS);
  HW_FBCR = wanted;
  while ((HW_FBCR & HW_FBCR_FS) != wanted)
    ;
}

void *memcpy(void *dst, const void *src, size_t n) {
  uint8_t *d = dst;
  const uint8_t *s = src;
  while (n--)
    *d++ = *s++;
  return dst;
}

void *memset(void *dst, int c, size_t n) {
  uint8_t *d = dst;
  while (n--)
    *d++ = (uint8_t)c;
  return dst;
}

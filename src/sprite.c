#include "sprite.h"
#include "hw32x.h"

/* Copie n pixels d'une ligne de sprite à l'offset 'offset' de la zone
   overwrite. Écritures 16 bits sur adresse paire : un premier pixel seul
   si la destination est impaire, des paires, un dernier pixel seul s'il en
   reste un. Un mot ou un pixel entièrement transparent n'est pas écrit
   (le VDP l'aurait ignoré, mais l'accès au bus aurait eu lieu). */
static void copyRow(uint32_t offset, const uint8_t *src, int n) {
  if (offset & 1) {
    if (*src != 0)
      HW_FB_OVERWRITE[offset] = *src;
    offset++;
    src++;
    n--;
  }

  volatile uint16_t *dst = (volatile uint16_t *)(HW_FB_OVERWRITE + offset);
  for (; n >= 2; n -= 2) {
    const uint16_t pair = (uint16_t)((src[0] << 8) | src[1]);
    if (pair != 0)
      *dst = pair;
    dst++;
    src += 2;
  }

  if (n > 0 && *src != 0)
    *(volatile uint8_t *)dst = *src;
}

void sprite_draw(const Lines *lines, const Sprite *sprite, int x, int y) {
  /* Partie visible, en coordonnées du sprite */
  const int colMin = x < 0 ? -x : 0;
  int colMax = sprite->width;
  if (x + colMax > HW_SCREEN_WIDTH)
    colMax = HW_SCREEN_WIDTH - x;

  int rowMin = y < 0 ? -y : 0;
  int rowMax = sprite->height;
  if (y + rowMax > HW_SCREEN_HEIGHT)
    rowMax = HW_SCREEN_HEIGHT - y;

  if (colMin >= colMax || rowMin >= rowMax)
    return;

  /* Première ligne visible qui revient à ce CPU */
  int row = rowMin;
  int phase = (y + row - lines->firstLine) % lines->lineStep;
  if (phase < 0)
    phase += lines->lineStep;
  if (phase != 0)
    row += lines->lineStep - phase;

  for (; row < rowMax; row += lines->lineStep)
    copyRow(hw_fb_offset(x + colMin, y + row),
            sprite->pixels + row * sprite->width + colMin, colMax - colMin);
}

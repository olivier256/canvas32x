#include "plane.h"
#include "hw32x.h"

/* =========================================================================
   Dessin d'une ligne de plan, en mots longs.

   Une ligne d'écran (320 pixels) est une suite de 80 mots longs, à une
   adresse multiple de 4. Côté source, la ligne du plan est la suite des
   lignes de tiles traversées : 16 octets par tile, soit 4 mots longs
   alignés (les tiles sont alignées sur 16, voir assets_types.h).

   Le défilement X décale l'une par rapport à l'autre de p = scrollX & 3
   octets. Chaque mot long écrit est donc fait de deux mots longs lus à la
   suite, prev et cur :
       p = 0 : cur                                  (copie directe)
       p = 1 : prev << 8  | cur >> 24
       p = 2 : prev << 16 | cur >> 16
       p = 3 : prev << 24 | cur >> 8
   Le SH2 est gros-boutiste : l'octet de poids fort est le pixel de gauche,
   en mémoire comme dans le frame buffer.

   Une écriture 32 bits dans le frame buffer fait deux transferts de 16
   bits sur le bus du VDP, comme deux écritures 16 bits, mais coûte deux
   fois moins d'instructions. Toutes les lectures viennent du cache.
   ========================================================================= */

/* Mot long lu dans des données déclarées en octets */
typedef uint32_t __attribute__((may_alias)) Long;

static inline __attribute__((always_inline)) uint32_t
funnel(uint32_t prev, uint32_t cur, int p) {
  if (p == 0)
    return cur;
  return (prev << (8 * p)) | (cur >> (32 - 8 * p));
}

/* Ligne de tile numéro n (adresse de son premier mot long) */
#define TILE_LINE(n) ((const Long *)(tileLine + (n) * TILE_PIXELS))

/* col : premier mot long du plan à lire (colonne / 4) ; longMask : largeur
   du plan en mots longs, moins 1. p est une constante à chaque appel :
   gcc produit une boucle par valeur, sans test à l'intérieur. */
static inline __attribute__((always_inline)) void
drawLine(volatile uint32_t *dst, const uint16_t *row, const uint8_t *tileLine,
         int col, int longMask, int p) {
  int remaining = HW_SCREEN_WIDTH / 4; /* mots longs à écrire */
  uint32_t prev = 0;

  /* Avec p != 0, le premier mot écrit a besoin d'un mot lu d'avance */
  if (p != 0) {
    prev = TILE_LINE(row[col >> 2])[col & 3];
    col = (col + 1) & longMask;
  }

  while (remaining > 0) {
    const Long *t = TILE_LINE(row[col >> 2]);
    int i = col & 3;

    if (i == 0 && remaining >= 4) {
      /* Tile entière : 4 mots longs, sans boucle */
      const uint32_t a = t[0], b = t[1], c = t[2], d = t[3];
      dst[0] = funnel(prev, a, p);
      dst[1] = funnel(a, b, p);
      dst[2] = funnel(b, c, p);
      dst[3] = funnel(c, d, p);
      prev = d;
      dst += 4;
      remaining -= 4;
      col = (col + 4) & longMask;
      continue;
    }

    /* Début ou fin de ligne : une partie de tile */
    for (; i < 4 && remaining > 0; i++, remaining--) {
      const uint32_t cur = t[i];
      *dst++ = funnel(prev, cur, p);
      prev = cur;
    }
    col = (col - (col & 3) + 4) & longMask;
  }
}

void plane_draw_line(int y, const Tilemap *map, const Tileset *tiles,
                     int scrollX, int worldY) {
  worldY &= (map->heightTiles << TILE_SHIFT) - 1;

  const uint16_t *row = map->cells + (worldY >> TILE_SHIFT) * map->widthTiles;
  const uint8_t *tileLine =
      tiles->pixels + (worldY & (TILE_SIZE - 1)) * TILE_SIZE;
  const int widthMask = (map->widthTiles << TILE_SHIFT) - 1;
  const int longMask = widthMask >> 2;

  const int x = scrollX & widthMask;
  const int col = x >> 2;
  volatile uint32_t *dst = (volatile uint32_t *)(HW_FB + hw_fb_offset(0, y));

  switch (x & 3) {
  case 0: drawLine(dst, row, tileLine, col, longMask, 0); break;
  case 1: drawLine(dst, row, tileLine, col, longMask, 1); break;
  case 2: drawLine(dst, row, tileLine, col, longMask, 2); break;
  default: drawLine(dst, row, tileLine, col, longMask, 3); break;
  }
}

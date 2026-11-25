#ifndef PLANE_H
#define PLANE_H

#include "assets_types.h"

/* =========================================================================
   Plans de tiles (16 x 16, voir assets_types.h).

   Un plan est une image de (widthTiles x 16) x (heightTiles x 16) pixels
   qui boucle dans les deux directions.
   ========================================================================= */

/* Dessine la ligne d'écran y : la ligne worldY du plan, à partir de sa
   colonne scrollX. worldY et scrollX peuvent être quelconques (négatifs,
   ou plus grands que le plan) : ils sont ramenés dans le plan. */
void plane_draw_line(int y, const Tilemap *map, const Tileset *tiles,
                     int scrollX, int worldY);

#endif

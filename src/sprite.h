#ifndef SPRITE_H
#define SPRITE_H

#include "assets_types.h"

/* =========================================================================
   Sprites, dessinés dans la zone overwrite du frame buffer : le VDP y
   ignore les octets nuls, l'index 0 est donc transparent.

   Les lignes d'écran sont partagées entre les deux SH2 : chacun ne dessine
   que les siennes, firstLine, firstLine + lineStep... (Master : 0, 2, 4...,
   Slave : 1, 3, 5...).
   ========================================================================= */

typedef struct {
  int firstLine;
  int lineStep;
} Lines;

/* (x, y) : coin haut-gauche, en pixels d'écran ; le sprite peut déborder
   ou être entièrement hors de l'écran. */
void sprite_draw(const Lines *lines, const Sprite *sprite, int x, int y);

#endif

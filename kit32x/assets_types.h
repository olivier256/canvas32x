#ifndef ASSETS_TYPES_H
#define ASSETS_TYPES_H

/* Lisible aussi par l'assembleur (.S) : seules les constantes lui sont
   visibles, les types C sont réservés au compilateur C. */
#ifndef __ASSEMBLER__
#include <stdint.h>
#endif

/* =========================================================================
   kit32x : types des données graphiques, telles que les produit img2c.

   Tous les pixels sont des index de palette sur 8 bits (mode Packed Pixel
   du VDP 32X). L'index 0 est transparent pour les sprites.
   ========================================================================= */

/* Les tiles sont carrées, de 16 x 16 pixels : une ligne de tile fait
   16 octets, soit exactement une ligne du cache du SH2. Chaque chargement
   de cache sert donc entièrement à la ligne d'écran en cours, quel que
   soit le CPU qui la dessine (avec des tiles 8 x 8, une ligne de cache
   contiendrait deux lignes de tile, dont l'une dessinée par l'autre CPU).
   Les tiles doivent pour cela commencer à une adresse multiple de 16
   (TILE_ALIGN), en SDRAM comme dans les données générées.
   C'est la seule constante à changer pour une autre taille (puissance de
   2) : img2c et le moteur la lisent tous deux ici. */
#define TILE_SHIFT 4
#define TILE_SIZE (1 << TILE_SHIFT)
#define TILE_PIXELS (TILE_SIZE * TILE_SIZE)
#define TILE_ALIGN 16 /* taille d'une ligne du cache SH2 */

/* Jeu de tiles partagé par les plans : 'count' tiles de TILE_PIXELS
   octets, rangées ligne par ligne. */
#define TILESET_MAX_TILES 65535

#ifndef __ASSEMBLER__
typedef struct {
  uint16_t count;
  const uint8_t *pixels;
} Tileset;
#endif

/* Plan : grille de widthTiles x heightTiles numéros de tile (16 bits),
   ligne par ligne. Les deux dimensions sont des puissances de 2, pour que
   le défilement boucle avec un simple masque. */
#ifndef __ASSEMBLER__
typedef struct {
  uint16_t widthTiles;
  uint16_t heightTiles;
  const uint16_t *cells;
} Tilemap;
#endif

/* Sprite : width x height pixels, ligne par ligne */
#ifndef __ASSEMBLER__
typedef struct {
  uint16_t width;
  uint16_t height;
  const uint8_t *pixels;
} Sprite;
#endif

/* Palette : 256 couleurs au format du VDP 32X, 0bbbbbgggggrrrrr
   (le bit 15, priorité, est laissé à 0). */
#define PALETTE_COLORS 256

/* Positions des champs, pour l'assembleur (vérifiées plus bas) */
#define TILEMAP_WIDTH_TILES 0
#define TILEMAP_HEIGHT_TILES 2
#define TILEMAP_CELLS 4
#define TILESET_PIXELS 4
#define SPRITE_WIDTH 0
#define SPRITE_HEIGHT 2
#define SPRITE_PIXELS 4

/* Le compilateur SH2 vérifie ces positions : si une structure change, la
   compilation s'arrête au lieu de produire un binaire faux. */
#if defined(__sh__) && !defined(__ASSEMBLER__)
#include <stddef.h>
#define ASSETS_CHECK(name, cond) typedef char assets_check_##name[(cond) ? 1 : -1]
ASSETS_CHECK(tw, offsetof(Tilemap, widthTiles) == TILEMAP_WIDTH_TILES);
ASSETS_CHECK(th, offsetof(Tilemap, heightTiles) == TILEMAP_HEIGHT_TILES);
ASSETS_CHECK(tc, offsetof(Tilemap, cells) == TILEMAP_CELLS);
ASSETS_CHECK(tp, offsetof(Tileset, pixels) == TILESET_PIXELS);
ASSETS_CHECK(sw, offsetof(Sprite, width) == SPRITE_WIDTH);
ASSETS_CHECK(sh, offsetof(Sprite, height) == SPRITE_HEIGHT);
ASSETS_CHECK(sp, offsetof(Sprite, pixels) == SPRITE_PIXELS);
#endif

#endif

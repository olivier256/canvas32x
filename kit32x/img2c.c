/* =========================================================================
   img2c (kit32x) : convertit des images BMP 8 bits indexées en données C
   pour les SH2 (types dans assets_types.h). Outil de l'hôte, compilé par
   le Makefile.

   Usage : img2c MANIFESTE SORTIE
     Produit SORTIE.h et SORTIE.c.

   Le manifeste est un fichier texte, une ligne par image :
       # commentaire
       plane   fond       fond.bmp
       sprite  vaisseau   vaisseau.bmp
   Les chemins sont relatifs au manifeste. Chaque ligne produit une
   variable 'asset_<nom>' (Tilemap pour un plan, Sprite pour un sprite).

   - Plan : l'image entière du niveau. Elle est découpée en tiles de
     TILE_SIZE x TILE_SIZE pixels (16 x 16, voir assets_types.h) ; les
     tiles identiques ne sont gardées qu'une fois, et tous les plans
     partagent le même jeu de tiles (asset_tiles, 65 535 au plus).
     Ses dimensions en tiles doivent être des puissances de 2.
   - Sprite : l'image telle quelle ; l'index 0 est transparent.
   - Palette (asset_palette) : celle de la première image, convertie au
     format 32X. Toutes les images doivent donner la même couleur aux
     index qu'elles utilisent.

   Formats acceptés : BMP Windows non compressé, 8 bits par pixel, de haut
   en bas ou de bas en haut. C'est ce qu'enregistrent GIMP (mode indexé),
   Aseprite, Photoshop ou Pillow en mode "P".
   ========================================================================= */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets_types.h" /* TILE_SIZE, TILESET_MAX_TILES */

#define MAX_ASSETS 64
#define MAX_PATH 1024

typedef struct {
  int width, height;
  uint8_t *pixels;     /* width * height, de haut en bas */
  uint8_t rgb[256][3]; /* palette du fichier */
  int colors;          /* entrées présentes dans le fichier */
} Image;

typedef enum { KIND_PLANE, KIND_SPRITE } Kind;

typedef struct {
  Kind kind;
  char name[64];
  char path[2 * MAX_PATH];
  Image image;
  uint16_t *cells; /* plans : numéros de tile */
} Asset;

static Asset assets[MAX_ASSETS];
static int assetCount = 0;

/* Jeu de tiles : tableau qui grandit, et table de hachage (adressage
   ouvert) pour retrouver vite une tile déjà vue. */
static uint8_t *tiles = NULL; /* tileCount * TILE_PIXELS octets */
static int tileCount = 0;
static int tileCapacity = 0;

#define HASH_SIZE (1 << 17) /* > 2 x TILESET_MAX_TILES */
static int32_t hashTable[HASH_SIZE]; /* numéro de tile + 1, 0 = libre */

static void fail(const char *fmt, const char *arg) {
  fprintf(stderr, "img2c : ");
  fprintf(stderr, fmt, arg);
  fprintf(stderr, "\n");
  exit(1);
}

/* ---- Lecture BMP --------------------------------------------------------- */

static uint32_t le32(const uint8_t *p) {
  return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t le16(const uint8_t *p) { return p[0] | (p[1] << 8); }

static void loadBMP(const char *path, Image *img) {
  FILE *f = fopen(path, "rb");
  if (f == NULL)
    fail("impossible d'ouvrir %s", path);

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *data = malloc((size_t)size);
  if (data == NULL || fread(data, 1, (size_t)size, f) != (size_t)size)
    fail("lecture impossible : %s", path);
  fclose(f);

  if (size < 54 || data[0] != 'B' || data[1] != 'M')
    fail("%s n'est pas un fichier BMP", path);

  uint32_t pixelOffset = le32(data + 10);
  uint32_t infoSize = le32(data + 14);
  int32_t width = (int32_t)le32(data + 18);
  int32_t height = (int32_t)le32(data + 22);
  uint16_t bpp = le16(data + 28);
  uint32_t compression = le32(data + 30);
  uint32_t colorsUsed = le32(data + 46);

  if (infoSize < 40)
    fail("%s : en-tête BMP trop ancien (OS/2)", path);
  if (bpp != 8)
    fail("%s : il faut une image 8 bits indexée (256 couleurs)", path);
  if (compression != 0)
    fail("%s : la compression RLE n'est pas prise en charge", path);

  int topDown = height < 0;
  if (topDown)
    height = -height;
  if (width <= 0 || height <= 0)
    fail("%s : dimensions invalides", path);

  img->width = width;
  img->height = height;
  img->colors = colorsUsed ? (int)colorsUsed : 256;
  if (img->colors > 256)
    fail("%s : palette de plus de 256 couleurs", path);

  memset(img->rgb, 0, sizeof img->rgb);
  const uint8_t *pal = data + 14 + infoSize;
  if (pal + img->colors * 4 > data + size)
    fail("%s : palette tronquée", path);
  for (int i = 0; i < img->colors; i++) {
    img->rgb[i][0] = pal[i * 4 + 2]; /* BMP : B, G, R, réservé */
    img->rgb[i][1] = pal[i * 4 + 1];
    img->rgb[i][2] = pal[i * 4 + 0];
  }

  /* Chaque ligne est complétée à un multiple de 4 octets */
  size_t stride = ((size_t)width + 3) & ~(size_t)3;
  if (pixelOffset + stride * (size_t)height > (size_t)size)
    fail("%s : pixels tronqués", path);

  img->pixels = malloc((size_t)width * height);
  if (img->pixels == NULL)
    fail("mémoire insuffisante pour %s", path);
  for (int y = 0; y < height; y++) {
    int srcY = topDown ? y : height - 1 - y;
    memcpy(img->pixels + (size_t)y * width,
           data + pixelOffset + (size_t)srcY * stride, (size_t)width);
  }
  free(data);
}

/* ---- Manifeste ----------------------------------------------------------- */

static void readManifest(const char *path) {
  FILE *f = fopen(path, "r");
  if (f == NULL)
    fail("impossible d'ouvrir le manifeste %s", path);

  /* Dossier du manifeste, pour les chemins relatifs */
  char dir[MAX_PATH];
  snprintf(dir, sizeof dir, "%s", path);
  char *slash = strrchr(dir, '/');
  if (slash != NULL)
    slash[1] = '\0';
  else
    dir[0] = '\0';

  char line[MAX_PATH];
  int lineNo = 0;
  while (fgets(line, sizeof line, f) != NULL) {
    lineNo++;
    char kind[16], name[64], file[MAX_PATH];
    char *hash = strchr(line, '#');
    if (hash != NULL)
      *hash = '\0';
    int n = sscanf(line, "%15s %63s %1023s", kind, name, file);
    if (n <= 0)
      continue;
    if (n != 3) {
      fprintf(stderr, "img2c : %s, ligne %d : 'type nom fichier' attendu\n",
              path, lineNo);
      exit(1);
    }
    if (assetCount == MAX_ASSETS)
      fail("trop d'images dans %s", path);

    Asset *a = &assets[assetCount++];
    if (strcmp(kind, "plane") == 0)
      a->kind = KIND_PLANE;
    else if (strcmp(kind, "sprite") == 0)
      a->kind = KIND_SPRITE;
    else
      fail("type inconnu '%s' (plane ou sprite)", kind);
    snprintf(a->name, sizeof a->name, "%s", name);
    if (snprintf(a->path, sizeof a->path, "%s%s", dir, file) >=
        (int)sizeof a->path)
      fail("chemin trop long : %s", file);
  }
  fclose(f);

  if (assetCount == 0)
    fail("%s ne déclare aucune image", path);
}

/* ---- Conversion ---------------------------------------------------------- */

/* Vérifie que l'image donne aux index qu'elle utilise les mêmes couleurs
   que la palette de référence. */
static void checkPalette(const Asset *a, const Image *ref) {
  uint8_t used[256] = {0};
  size_t n = (size_t)a->image.width * a->image.height;
  for (size_t i = 0; i < n; i++)
    used[a->image.pixels[i]] = 1;

  for (int c = 0; c < 256; c++) {
    if (!used[c])
      continue;
    if (memcmp(a->image.rgb[c], ref->rgb[c], 3) != 0) {
      fprintf(stderr,
              "img2c : %s : l'index %d vaut #%02X%02X%02X, mais "
              "#%02X%02X%02X dans la palette de référence (%s)\n",
              a->path, c, a->image.rgb[c][0], a->image.rgb[c][1],
              a->image.rgb[c][2], ref->rgb[c][0], ref->rgb[c][1],
              ref->rgb[c][2], assets[0].path);
      exit(1);
    }
  }
}

static int isPowerOf2(int v) { return v > 0 && (v & (v - 1)) == 0; }

/* FNV-1a */
static uint32_t hashTile(const uint8_t *tile) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < TILE_PIXELS; i++)
    h = (h ^ tile[i]) * 16777619u;
  return h;
}

static int findOrAddTile(const uint8_t *tile, const char *path) {
  uint32_t slot = hashTile(tile) & (HASH_SIZE - 1);
  while (hashTable[slot] != 0) {
    int i = hashTable[slot] - 1;
    if (memcmp(tiles + (size_t)i * TILE_PIXELS, tile, TILE_PIXELS) == 0)
      return i;
    slot = (slot + 1) & (HASH_SIZE - 1);
  }

  if (tileCount == TILESET_MAX_TILES)
    fail("plus de 65 535 tiles différentes (dernier plan : %s)", path);
  if (tileCount == tileCapacity) {
    tileCapacity = tileCapacity ? tileCapacity * 2 : 256;
    tiles = realloc(tiles, (size_t)tileCapacity * TILE_PIXELS);
    if (tiles == NULL)
      fail("mémoire insuffisante pour %s", path);
  }
  memcpy(tiles + (size_t)tileCount * TILE_PIXELS, tile, TILE_PIXELS);
  hashTable[slot] = tileCount + 1;
  return tileCount++;
}

static void cutPlane(Asset *a) {
  const Image *img = &a->image;
  if (img->width % TILE_SIZE || img->height % TILE_SIZE)
    fail("%s : largeur et hauteur doivent être des multiples de la taille "
         "des tiles (TILE_SIZE)",
         a->path);

  int w = img->width / TILE_SIZE;
  int h = img->height / TILE_SIZE;
  if (!isPowerOf2(w) || !isPowerOf2(h))
    fail("%s : le nombre de tiles en largeur et en hauteur doit être une "
         "puissance de 2 (8, 16, 32, 64...)",
         a->path);

  a->cells = malloc((size_t)w * h * sizeof *a->cells);
  if (a->cells == NULL)
    fail("mémoire insuffisante pour %s", a->path);

  uint8_t tile[TILE_PIXELS];
  for (int ty = 0; ty < h; ty++) {
    for (int tx = 0; tx < w; tx++) {
      for (int y = 0; y < TILE_SIZE; y++)
        memcpy(tile + y * TILE_SIZE,
               img->pixels + (size_t)(ty * TILE_SIZE + y) * img->width +
                   tx * TILE_SIZE,
               TILE_SIZE);
      a->cells[ty * w + tx] = (uint16_t)findOrAddTile(tile, a->path);
    }
  }
}

/* ---- Écriture ------------------------------------------------------------ */

static void writeBytes(FILE *f, const uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (i % 16 == 0)
      fprintf(f, "\n   ");
    fprintf(f, " 0x%02X,", p[i]);
  }
  fprintf(f, "\n");
}

static void writeWords(FILE *f, const uint16_t *p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (i % 12 == 0)
      fprintf(f, "\n   ");
    fprintf(f, " 0x%04X,", p[i]);
  }
  fprintf(f, "\n");
}

static void writeOutput(const char *base, const char *manifest) {
  char path[MAX_PATH];
  const char *guard = "ASSETS_H";

  /* En-tête */
  snprintf(path, sizeof path, "%s.h", base);
  FILE *h = fopen(path, "w");
  if (h == NULL)
    fail("impossible de créer %s", path);
  fprintf(h, "/* Généré par img2c depuis %s : ne pas modifier. */\n\n",
          manifest);
  fprintf(h, "#ifndef %s\n#define %s\n\n#include \"assets_types.h\"\n\n",
          guard, guard);
  fprintf(h, "extern const uint16_t asset_palette[PALETTE_COLORS];\n");
  fprintf(h, "extern const Tileset asset_tiles;\n\n");
  for (int i = 0; i < assetCount; i++)
    fprintf(h, "extern const %s asset_%s;\n",
            assets[i].kind == KIND_PLANE ? "Tilemap" : "Sprite",
            assets[i].name);
  fprintf(h, "\n#endif\n");
  fclose(h);

  /* Données */
  snprintf(path, sizeof path, "%s.c", base);
  FILE *c = fopen(path, "w");
  if (c == NULL)
    fail("impossible de créer %s", path);

  const char *slash = strrchr(base, '/');
  fprintf(c, "/* Généré par img2c depuis %s : ne pas modifier. */\n\n",
          manifest);
  fprintf(c, "#include \"%s.h\"\n\n", slash ? slash + 1 : base);

  /* Palette : RGB 8 bits -> 0bbbbbgggggrrrrr */
  const Image *ref = &assets[0].image;
  fprintf(c, "const uint16_t asset_palette[PALETTE_COLORS] = {");
  for (int i = 0; i < 256; i++) {
    unsigned r = ref->rgb[i][0] >> 3, g = ref->rgb[i][1] >> 3,
             b = ref->rgb[i][2] >> 3;
    if (i % 8 == 0)
      fprintf(c, "\n   ");
    fprintf(c, " 0x%04X,", r | (g << 5) | (b << 10));
  }
  fprintf(c, "\n};\n\n");

  /* Tiles alignées sur TILE_ALIGN (une ligne de cache SH2) : comme chaque
     tile fait TILE_PIXELS octets, multiple de 16, toutes le sont. */
  if (tileCount > 0) {
    fprintf(c, "static const uint8_t tilePixels[%d * TILE_PIXELS]\n"
               "    __attribute__((aligned(TILE_ALIGN))) = {",
            tileCount);
    for (int i = 0; i < tileCount; i++) {
      fprintf(c, "\n    /* tile %d */", i);
      writeBytes(c, tiles + (size_t)i * TILE_PIXELS, TILE_PIXELS);
    }
    fprintf(c, "};\n\nconst Tileset asset_tiles = {%d, tilePixels};\n\n",
            tileCount);
  } else {
    fprintf(c, "const Tileset asset_tiles = {0, 0};\n\n");
  }

  for (int i = 0; i < assetCount; i++) {
    const Asset *a = &assets[i];
    if (a->kind == KIND_PLANE) {
      int w = a->image.width / TILE_SIZE, hh = a->image.height / TILE_SIZE;
      fprintf(c, "/* %s */\nstatic const uint16_t %s_cells[%d * %d] = {",
              a->path, a->name, w, hh);
      writeWords(c, a->cells, (size_t)w * hh);
      fprintf(c, "};\n\nconst Tilemap asset_%s = {%d, %d, %s_cells};\n\n",
              a->name, w, hh, a->name);
    } else {
      int w = a->image.width, hh = a->image.height;
      fprintf(c,
              "/* %s */\nstatic const uint8_t %s_pixels[%d * %d]\n"
              "    __attribute__((aligned(4))) = {",
              a->path, a->name, w, hh);
      writeBytes(c, a->image.pixels, (size_t)w * hh);
      fprintf(c, "};\n\nconst Sprite asset_%s = {%d, %d, %s_pixels};\n\n",
              a->name, w, hh, a->name);
    }
  }
  fclose(c);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage : img2c MANIFESTE SORTIE  (produit SORTIE.h et "
                    "SORTIE.c)\n");
    return 1;
  }

  readManifest(argv[1]);

  for (int i = 0; i < assetCount; i++) {
    loadBMP(assets[i].path, &assets[i].image);
    checkPalette(&assets[i], &assets[0].image);
    if (assets[i].kind == KIND_PLANE)
      cutPlane(&assets[i]);
  }

  writeOutput(argv[2], argv[1]);

  fprintf(stderr, "img2c : %d images, %d tiles -> %s.c / %s.h\n", assetCount,
          tileCount, argv[2], argv[2]);
  return 0;
}

/* =========================================================================
   Mini-démo de canvas32x : un plan qui défile, un sprite, la manette.

     Croix          déplace le vaisseau
     C (maintenu)   plus vite
     A (maintenu)   arrête le défilement
     B (maintenu)   défilement à l'envers

   Les deux SH2 dessinent la même image, chacun ses lignes : le Master
   les paires, le Slave les impaires.

     Master                               Slave
     ------                               -----
     lit la manette, calcule la scène
     publie la scène, COMM_FRAME = n  --> lit la scène
     dessine les lignes paires            dessine les lignes impaires
     attend COMM_SLAVE_DONE = n      <--  COMM_SLAVE_DONE = n
     échange les frame buffers (VBlank)
     image suivante                       attend COMM_FRAME = n + 1
   ========================================================================= */

#include "assets.h" /* généré par img2c depuis assets/assets.txt */
#include "hw32x.h"
#include "plane.h"
#include "sprite.h"

/* Registres COMM utilisés (COMM0 : manette, réservé par le kit) */
#define COMM_FRAME 1      /* Master -> Slave : image à dessiner (1, 2...) */
#define COMM_SLAVE_DONE 2 /* Slave -> Master : dernière image finie */

/* Tout ce qu'il faut pour dessiner une image */
typedef struct {
  int16_t scrollX;
  int16_t shipX, shipY;
} Scene;

/* Écrite par le Master, lue par le Slave : toujours par HW_UNCACHED() */
static Scene sharedScene;

static int clamp(int v, int min, int max) {
  return v < min ? min : (v > max ? max : v);
}

static void update(Scene *s, unsigned pad) {
  const int speed = (pad & HW_PAD_C) ? 4 : 1;
  const int dx = !!(pad & HW_PAD_RIGHT) - !!(pad & HW_PAD_LEFT);
  const int dy = !!(pad & HW_PAD_DOWN) - !!(pad & HW_PAD_UP);

  s->shipX = (int16_t)clamp(s->shipX + dx * speed, 0,
                            HW_SCREEN_WIDTH - asset_vaisseau.width);
  s->shipY = (int16_t)clamp(s->shipY + dy * speed, 0,
                            HW_SCREEN_HEIGHT - asset_vaisseau.height);

  if (!(pad & HW_PAD_A))
    s->scrollX = (int16_t)(s->scrollX + ((pad & HW_PAD_B) ? -1 : 1));
}

static void draw(const Scene *s, int cpu) {
  const Lines lines = {.firstLine = cpu, .lineStep = 2};

  /* Le plan fait 256 lignes ; on en montre les 224 du bas */
  for (int y = lines.firstLine; y < HW_SCREEN_HEIGHT; y += lines.lineStep)
    plane_draw_line(y, &asset_fond, &asset_tiles, s->scrollX, y + 32);

  sprite_draw(&lines, &asset_vaisseau, s->shipX, s->shipY);
}

void master_main(void) {
  hw_video_init(asset_palette);

  Scene scene = {.scrollX = 0, .shipX = 40, .shipY = 96};
  uint16_t frame = 0;

  for (;;) {
    if (++frame == 0) /* 0 veut dire « pas encore d'image » */
      frame = 1;

    update(&scene, hw_pad());

    *HW_UNCACHED(&sharedScene) = scene;
    HW_BARRIER();
    HW_COMM[COMM_FRAME] = frame;

    draw(&scene, 0);

    while (HW_COMM[COMM_SLAVE_DONE] != frame)
      ;
    hw_flip();
  }
}

void slave_main(void) {
  uint16_t last = 0;

  for (;;) {
    uint16_t frame;
    while ((frame = HW_COMM[COMM_FRAME]) == last)
      ;
    HW_BARRIER();
    const Scene scene = *HW_UNCACHED(&sharedScene);

    draw(&scene, 1);

    HW_BARRIER();
    HW_COMM[COMM_SLAVE_DONE] = frame;
    last = frame;
  }
}

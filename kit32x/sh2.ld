/* =========================================================================
   Carte mémoire SH2 (kit32x).

   La BootROM 32X copie l'image de la cartouche (ROM 10000h, voir md68k.s)
   en SDRAM (0600 0000h), puis l'exécute en place : code, données, tiles,
   tilemaps, sprites. La BSS suit l'image ; les piles sont en haut de la
   SDRAM (Master : 0604 0000h, Slave : 0603 C000h, voir sh2_start.S).

     0600 0000h  vecteurs, _start (0600 0240h), .text, .rodata, .data
                 puis la BSS
     0603 8000h  pile du Slave  (16 Ko, descend depuis 0603 C000h)
     0603 C000h  pile du Master (16 Ko, descend depuis 0604 0000h)

   La SDRAM fait 256 Ko : image + BSS doivent tenir dans les 224 premiers.
   L'éditeur de liens s'arrête sinon. Pour plus de données, les laisser en
   ROM (vue du SH2 en 0200 0000h + adresse cartouche) : voir hw32x.h.
   ========================================================================= */

OUTPUT_ARCH(sh)
ENTRY(__start)

MEMORY {
  sdram (rwx) : ORIGIN = 0x06000000, LENGTH = 0x38000
}

SECTIONS {
  .vectors 0x06000000 : { KEEP(*(.vectors)) } > sdram
  ASSERT(__start == 0x06000240, "_start doit etre en 0600 0240h (en-tete MARS)")

  .text   : { *(.text .text.*) } > sdram
  .rodata : { *(.rodata .rodata.*) } > sdram
  .data   : { *(.data .data.*) } > sdram
  . = ALIGN(4);
  __image_end = .;

  .bss (NOLOAD) : {
    __bss_start = .;
    *(.bss .bss.*) *(COMMON)
    . = ALIGN(4);
    __bss_end = .;
  } > sdram

  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}

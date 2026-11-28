/* =========================================================================
   kit32x : démarrage des deux SH2 (début de l'image copiée en SDRAM).

   La BootROM 32X copie l'image de la cartouche (ROM 10000h) en SDRAM
   (0600 0000h), règle le VBR de chaque CPU d'après l'en-tête MARS
   (Master : 0600 0000h, Slave : 0600 0120h), puis fait
   sauter les deux CPU en 0600 0240h : ici.

     0000h  table de vecteurs du Master (72 vecteurs)
     0120h  table de vecteurs du Slave  (72 vecteurs)
     0240h  _start, commun aux deux CPU

   Poignée de main avec le 68000 et la BootROM (registres COMM) :
     BootROM : COMM0-1 = 'M_OK', COMM2-3 = 'S_OK'
     68000   : attend 'M_OK', met FM à 1 (les SH2 ont le VDP 32X)
     Master  : attend FM, attend 'S_OK', efface la BSS, efface COMM0-1
     Slave   : attend que COMM0-1 ne vaille plus 'M_OK', efface COMM2-3
     68000   : attend que COMM0-1 ne vaille plus 'M_OK', puis écrit la
               manette dans COMM0 à chaque image
   Même ordre que le programme 68000 (md68k.s).

   Aucune interruption n'est utilisée : SR.I = 15 (tout masqué).
   Le C fournit master_main() et slave_main(), qui ne reviennent pas.
   ========================================================================= */

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)
#define SYMBOL(name) CONCAT(__USER_LABEL_PREFIX__, name)

        .section .vectors, "ax"

/* ---- Tables de vecteurs ------------------------------------------------
   Une exception (adresse impaire, instruction illégale...) mène à
   exception_halt : le CPU s'arrête là, ce qui se voit dans le débogueur
   de l'émulateur (PC sur exception_halt). */

        .macro VECTORS sp
        .long   SYMBOL(_start)          /* 0 : reset, PC */
        .long   \sp                     /* 1 : reset, SP */
        .long   SYMBOL(_start)          /* 2 : reset manuel, PC */
        .long   \sp                     /* 3 : reset manuel, SP */
        .rept   72 - 4
        .long   exception_halt
        .endr
        .endm

master_vectors:                         /* 0600 0000h */
        VECTORS 0x06040000
slave_vectors:                          /* 0600 0120h */
        VECTORS 0x0603C000

/* ---- Point d'entrée commun (0600 0240h) -------------------------------- */

        .global SYMBOL(_start)
SYMBOL(_start):
        mov     #0xF0, r0               /* SR.I = 15 : aucune interruption */
        ldc     r0, sr

        /* Restrictions sur le FRT (32X Hardware Manual, « Restrictions
           Concerning SH2 Interrupt ») */
        mov.l   frt_base, r1            /* FFFF FE10h */
        mov     #1, r0
        mov.b   r0, @r1                 /* TIER : bit 0 toujours à 1 */
        mov     #0xE2, r0
        mov.b   r0, @(7, r1)            /* TOCR : OCRA, sortie 1 */
        mov     #0, r0
        mov.b   r0, @(4, r1)            /* OCRA = 0001h (deux octets) */
        mov     #1, r0
        mov.b   r0, @(5, r1)
        mov     #0, r0
        mov.b   r0, @(6, r1)            /* TCR */
        mov     #1, r0
        mov.b   r0, @(1, r1)            /* FTCSR : FRC remis à 0 sur OCRA */
        mov     #0, r0
        mov.b   r0, @(2, r1)            /* FRC = 0 */
        mov.b   r0, @(3, r1)

        /* Cache : coupé, purgé, puis rallumé (4 voies, 4 Ko) */
        mov.l   CCR, r1
        mov     #0, r0
        mov.b   r0, @r1
        mov     #0x10, r0               /* Set CP : Cache purge */
        mov.b   r0, @r1
        mov     #0x01, r0               /* Set CE : Cache Enabled */
        mov.b   r0, @r1

        /* Quel CPU ? Le VBR du Slave vaut 0600 0120h (bit 5 à 1) */
        stc     vbr, r0
		mov.l	@(4,r0),r15
        tst     #0x20, r0
        bf      start_slave

/* ---- Master ------------------------------------------------------------ */

start_master:

        mov.l   IMR, r1            /* 2000 4000h */
.waitForFMSet:
1:      mov.b   @r1, r0
        tst     #0x80, r0
        bt      .waitForFMSet

        mov.l   s_ok, r2
.waitForS_OK:
1:      mov.l   @(0x24, r1), r0
        cmp/eq  r2, r0
        bf      .waitForS_OK

        /* Effacer la BSS (variables C non initialisées) */
        mov.l   bss_start, r2
        mov.l   bss_end, r3
        mov     #0, r0
        bra     2f
        nop
1:      mov.l   r0, @r2
        add     #4, r2
2:      cmp/hs  r3, r2
        bf      1b

        /* Clear CommPort 0-1 */
        mov     #0, r0
        mov.l   r0, @(0x20, r1)

        mov.l   master_main, r0
        jsr     @r0
        nop
        bra     exception_halt          /* master_main ne revient jamais */
        nop

/* ---- Slave ------------------------------------------------------------- */

start_slave:

        mov.l   IMR, r1
        mov.l   m_ok, r2
.waitForMStarted:
1:      mov.l   @(0x20, r1), r0
        cmp/eq  r2, r0
        bt      .waitForMStarted

        /* Clear Comm Port 2-3 */
        mov     #0, r0
        mov.l   r0, @(0x24, r1)

        mov.l   slave_main, r0
        jsr     @r0
        nop

exception_halt:
        bra     exception_halt
        nop



        .align  4
frt_base:       .long   0xFFFFFE10

/*
Cache Control Register
W1 W0 — CP TW OD ID CE
*/
CCR:            .long   0xFFFFFE92

/*
Interrupt Mask Register (2000_4000h)
FM - - - - ADEN CART HEN   - - - - V H CMD PWM
    FM: VDP Access Authorization
        0: MEGA DRIVE (initial value)
        1: SH2
*/
IMR:            .long   0x20004000
m_ok:	        .ascii  "M_OK"
s_ok:           .ascii  "S_OK"
bss_start:      .long   __bss_start   /* symboles du script ld */
bss_end:        .long   __bss_end
master_main:    .long   SYMBOL(master_main)
slave_main:     .long   SYMBOL(slave_main)

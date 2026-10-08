/* The play state on the CPS3 display (draw.h). Drawing model: tools/viewlevel.py (checked against the HD runner's
 * frames) plus the Draw events of HD 1.2.2 (refs/hd/src/objects/<o>/Draw_0.gml, scripts/characterDrawEvent);
 * tools/drawmodel.py is the host model the MAME frames are compared with (scripts/game_check.sh).
 *
 * GameMaker draw rules used here:
 *   - draw_self / draw_sprite_ext(sprite_index, image_index, x, y, image_xscale, ..): frame floor(image_index) mod
 *     frames, mirrored about x when image_xscale < 0 (other scales, image_angle, image_blend and image_alpha are not
 *     drawn: counted in draw_st.unsup); draw_sprite(s, -1, x, y): the instance's image_index, not mirrored;
 *   - positions as tools/viewlevel.py: x and y truncated to whole pixels;
 *   - order: depth descending, then newest first (instance id descending; tools/drawmodel.py: the runner's frames
 *     with overlapping oBlood at one depth); tile_add tiles before instances of their depth; a tilemap band draws at
 *     its depth's place, before the sprites of the same depth.
 * SH-2: no soft-float in the per-instance tests (depths and the coarse view test compare float bit patterns as
 * ordered integers); x / y / image_index are converted for the instances in view only. */
#include "cps3.h"
#include "sprites.h"
#include "drawtab.h"
#include "draw.h"
#include "hud.h"
#include "hudart.h"
#include "fade.h"
#include "pint.h"
#include "front.h"
#include "pmsg.h"

#define UNIT(m)      CPS3V_MAP_UNIT(m)
#define BLANK        (GFX_FIRST_TILE - 1u)        /* the flash's blank tile (empty tilemap cells) */
#define VIEW_W       320
#define VIEW_H       240
#define SCREEN_H     224
#define NMAPS        3                            /* tilemaps 1..3 (0: the background) */
#define MAPC_MAX     2400                         /* cells of the largest room (rOlmec 42 x 55 = 2,310) */
#define ENT_MAX      1024
#define TSPR_MAX     1024
#define DRAW_ENTRIES_MAX 1000                     /* the frame's sublist area: 1,024 entries less group alignment */
#ifdef DRAW_HOST                                  /* tests/game/host.c: the display list on the host */
#include <stdio.h>
static uint16_t zoom_x_host;
#define ZOOM_X       zoom_x_host
#else
#define ZOOM_X       (*(volatile uint16_t *)0x040c006eu)
#endif

struct draw_stats draw_st;
#ifdef DRAW_PROFILE
#define PROF(k) do { uint32_t t_ = cps3t_ticks(); draw_st.prof[k] = (t_ - prof_t) & 0xffff; prof_t = t_; } while (0)
#define PROF0() (prof_t = cps3t_ticks())
static uint32_t prof_t;
#else
#define PROF(k) ((void)0)
#define PROF0() ((void)0)
#endif
uint8_t draw_hud_on = 1;
uint8_t draw_smooth;

/* ---- the play state's instance fields: every read of struct pin goes through these (src/game splits the struct:
   the GML variables of struct pin_ext, through pin's ext; only this block follows such changes) ---------------- */
#define I_ALIVE(i)   (PW.in[i].alive)
#define I_VISIBLE(i) (PW.in[i].visible)
#define I_OBJ(i)     (PW.in[i].obj)
#define I_ID(i)      (PW.in[i].id)
#define I_SPR(i)     (PW.in[i].spr)
#define I_MASK(i)    (PW.in[i].mask)
#define I_IMG(i)     (PW.in[i].img)
#define I_X(i)       (PW.in[i].x)
#define I_Y(i)       (PW.in[i].y)
#define I_DEPTH(i)   (PW.in[i].depth)
#define I_XSCALE(i)  (PW.in[i].xscale)
#define I_YSCALE(i)  (PW.in[i].yscale)
#define I_ANGLE(i)   (PW.in[i].angle)
#define I_FACING(i)  (pin_ext[PW.in[i].ext].facing)
#define I_COST(i)    (pin_ext[PW.in[i].ext].cost)
#define I_CIMG(i)    (pin_ext[PW.in[i].ext].cimg)
#define I_STATUS(i)  (pin_ext[PW.in[i].ext].status)
#define I_HASGUN(i)  (pin_ext[PW.in[i].ext].hasGun)
#define I_ROLLED(i)  ((void)(i), 0)                  /* oDice.rolled: not in the play state (dice house not translated) */
#define I_HP(i)      (pin_ext[PW.in[i].ext].hp)
#define I_TRIGGER(i) (pin_ext[PW.in[i].ext].trigger)          /* oDamselKiss: kissed (pdamsel.c) */
/* globals and other play state read here */
#define S_DARKLEVEL  (G.darkLevel)
#define S_DARKNESS   (PLEV.darkness)
#define S_BLOODLEVEL (PMSG.bloodLevel)             /* global.bloodLevel */
#define S_MSGTIMER   (PMSG.timer)                  /* global.messageTimer */
#define S_MSG1       (PMSG.m1)                     /* global.message1 / 2 as drawn */
#define S_MSG2       (PMSG.m2)

/* ---- float helpers (no soft-float) ------------------------------------------------------------------------ */
static inline uint32_t fbits(float f) { union { float f; uint32_t u; } c; c.f = f; return c.u; }
/* a key that orders as the float does (NaN aside) */
static inline uint32_t fkey(float f) { uint32_t u = fbits(f); return (u & 0x80000000u) ? ~u : (u | 0x80000000u); }
/* image_xscale / yscale / angle are floats (struct pin): tested on their bit patterns, as the doubles they convert to
   would be (float -> double is exact: +-1, +-0 and the sign carry over), with no ___extendsfdf2 call */
#define F_ONE   0x3f800000u
#define F_MONE  0xbf800000u
static inline int scale_is_pm1(float f) { uint32_t u = fbits(f); return u == F_ONE || u == F_MONE; }
static inline int fl_neg(float f) { return (fbits(f) & 0x80000000u) != 0; }
static inline int fl_zero(float f) { return (fbits(f) & 0x7fffffffu) == 0; }
/* bit32[k] = 1 << k: the SH-2 has no barrel shifter, so GCC makes a variable shift a libgcc call (___ashlsi3 /
   ___lshrsi3); a load from this table replaces it. For a float of unbiased exponent e in -1 .. 22 and mantissa m
   (implicit bit set), the 64-bit product m x bit32[9 + e] holds m >> (23 - e) in its high word and the bits shifted
   out, left-aligned, in its low word: one dmulu.l in place of the variable shifts */
static const uint32_t bit32[32] = {
    1u << 0, 1u << 1, 1u << 2, 1u << 3, 1u << 4, 1u << 5, 1u << 6, 1u << 7, 1u << 8, 1u << 9, 1u << 10, 1u << 11,
    1u << 12, 1u << 13, 1u << 14, 1u << 15, 1u << 16, 1u << 17, 1u << 18, 1u << 19, 1u << 20, 1u << 21, 1u << 22,
    1u << 23, 1u << 24, 1u << 25, 1u << 26, 1u << 27, 1u << 28, 1u << 29, 1u << 30, 1u << 31,
};
/* (int32_t)f, truncation toward 0, by integer operations (no soft-float call) */
static int32_t ftoi(float f)
{
    uint32_t u = fbits(f), m = (u & 0x7fffffu) | 0x800000u;
    int e = (int)((u >> 23) & 255) - 127;
    int32_t v;
    if (e < 0) return 0;
    v = e >= 23 ? (int32_t)(m << (e - 23 > 7 ? 7 : e - 23)) : (int32_t)(((uint64_t)m * bit32[e + 9]) >> 32);
    return (u & 0x80000000u) ? -v : v;
}
/* the pixel a sprite at float coordinate f starts on: GameMaker's quad covers the pixels whose centre is at or after
   f, i.e. ceil(f - 0.5) (round half down; build/trace/g_p7_dark_s18: oFlareSpark at y 112.8 on row 113). Integer
   operations only: for e < 23, ip = m >> (23 - e) and the fraction's bits left-aligned in lo (the half is
   0x80000000) */
static int32_t fpix(float f)
{
    uint32_t u = fbits(f), m = (u & 0x7fffffu) | 0x800000u, lo;
    int e = (int)((u >> 23) & 255) - 127;
    int32_t ip;
    uint64_t p;
    if (e < -1) return 0;                         /* |f| < 0.5 */
    if (e >= 23) {
        ip = (int32_t)(m << (e - 23 > 7 ? 7 : e - 23));
        return (u & 0x80000000u) ? -ip : ip;
    }
    p = (uint64_t)m * bit32[e + 9];
    ip = (int32_t)(p >> 32);
    lo = (uint32_t)p;
    if (u & 0x80000000u) return -(ip + (lo >= 0x80000000u));
    return ip + (lo > 0x80000000u);
}

/* ---- the frame's state ------------------------------------------------------------------------------------ */
static int vx, vy;                                /* the view's top-left (room pixels) */
#ifdef DRAW_HOST
extern uint32_t host_sprram[];                    /* tests/game/host.c: sprite RAM, decoded by its cps3v_object */
#define SPR_AT(a)    (&host_sprram[(a) >> 2])
typedef uint32_t spr_word;
#else
#define SPR_AT(a)    ((volatile uint32_t *)(0x04000000u + (a)))
typedef volatile uint32_t spr_word;
#endif
/* default 0 (CPU stores) until the DMAC's writes into sprite RAM are checked on jtcps3 (tests/dmac on MiSTer);
   SPRDMA=1 builds the DMAC path */
#ifndef DRAW_SPRDMA
#define DRAW_SPRDMA 0
#endif
/* the entry writer's state, in one struct so that a function reaches all of it from one base register (words first:
   mov.l @(disp,Rn) reaches 60 bytes). run_p: the next entry; ent_lim: see ent_room; cur_pal: the colour code of
   entries (dark levels: DRAW_PAL until oLevel's place, then DRAW_PAL_LIT); ox, oy: screen offset (a sprite at room
   (x, y) is at (x - ox, y - oy)); smooth motion: mid_on while this frame's midpoint list is built, each entry going
   to it at (mdx, mdy) from its own place; a piece is drawn when on screen in either list (clip bounds cxl..cxh,
   cyl..cyh); run_n: entries in the open run; ent_n: sublist entries this frame */
#if DRAW_SPRDMA
typedef uint32_t ew_word;
#else
typedef spr_word ew_word;
#endif
static struct ew {
    ew_word *run_p, *ent_lim;
    uint32_t cur_pal;
    int ox, oy, mdx, mdy, cxl, cxh, cyl, cyh;
    uint32_t run_n, ent_n;
    int ocx, ocy;                                 /* smooth motion: the midpoint camera less this frame's */
    uint8_t mid_on;
} EW = { .cur_pal = DRAW_PAL, .cxh = VIEW_W, .cyh = SCREEN_H };
/* smooth motion (draw.h): vmx, vmy: the view test's margin (16 px, plus the camera's half step); set_mid: the
   midpoint offset of the entries that follow and the clip bounds (the initial EW values are set_mid(0, 0)'s) */
static int vmx = 16, vmy = 16;
static inline __attribute__((always_inline)) void set_mid(int dx, int dy)
{
    if (dx == EW.mdx && dy == EW.mdy) return;    /* (the bounds are set from dx, dy here only: unchanged) */
    EW.mdx = dx;
    EW.mdy = dy;
    EW.cxh = VIEW_W - (dx < 0 ? dx : 0);
    EW.cxl = -(dx > 0 ? dx : 0);
    EW.cyh = SCREEN_H - (dy < 0 ? dy : 0);
    EW.cyl = -(dy > 0 ? dy : 0);
}
/* dark levels: oLevel's black rectangle at alpha oLevel.darkness (objects/oLevel/Draw_0.gml) as a fade of colour
   code DRAW_PAL (tools/darkfade.py: the palette faded at alpha byte a8, by palette DMA at VBlank); what is drawn
   after oLevel (depth below -2, and the HUD) uses DRAW_PAL_LIT, an unfaded copy */
static int frame_a8, shown_a8;
typedef char hud_faded_codes[HUD_PAL_FADED == DRAW_PAL_HUDDARK && HUD_PAL_FADED_YELLOW == DRAW_PAL_HUDDARK_YELLOW ? 1 : -1];
int16_t draw_dark_force = -1;

/* tilemaps: one depth each; base: the tile_add cells, want: base + terrain this frame, shown: what the tilemap
   holds (written by draw_vblank). Values: tile number, 0 = empty (BLANK). Kept outside main RAM in MAME builds
   (DRAW_MAPS_SECTION, see tests/game/sprbss.ld) */
#ifndef DRAW_MAPS_SECTION
#define DRAW_MAPS_SECTION
#endif
#ifndef DRAW_CACHE_SECTION
#define DRAW_CACHE_SECTION
#endif
static uint16_t mbase[NMAPS][MAPC_MAX] DRAW_MAPS_SECTION;
static uint16_t mwant[NMAPS][MAPC_MAX] DRAW_MAPS_SECTION;
static uint16_t mshown[NMAPS][MAPC_MAX] DRAW_MAPS_SECTION;
/* the cells whose mwant changed since the last draw_vblank (cell_fix): draw_vblank compares those only, so a frame
   costs nothing for the cells that stay. mfull: every room cell compared (room start, the tile layers changed, the
   queue full). Rooms of more than 64 cells a side (a 64 x 64 tilemap holds two room cells in one) keep the old way:
   the cells on screen compared each frame */
#define MQ_MAX 512
static uint8_t mqueued[NMAPS][MAPC_MAX] DRAW_MAPS_SECTION;
static uint16_t mq[MQ_MAX];                       /* m * MAPC_MAX + cell */
static int mqn;
static uint8_t mfull;
/* terrain on the tilemaps, kept up to date from src/game's dirty marks (pw_draw_dirty): per map cell the instances
   whose frame is that cell's tile (chead, then cnext); the one drawn first (largest draw key: the newest) is the
   cell's tile in mwant unless a tile_add tile has the cell; the others (texc) are drawn as sprites over it */
static int16_t chead[NMAPS][MAPC_MAX] DRAW_MAPS_SECTION;
/* (the per-instance claim arrays are touched only for instances whose draw state changed: DRAW_CACHE_SECTION, sprite
   RAM in the test builds, keeps main RAM for the play state) */
static int16_t cnext[PIN_MAX] DRAW_CACHE_SECTION;
static int16_t ccell[PIN_MAX] DRAW_MAPS_SECTION;   /* m * MAPC_MAX + cell, -1 when not on a cell */
static uint16_t ctile[PIN_MAX] DRAW_MAPS_SECTION;  /* (ccell, ctile: in the maps' section, the cache's area being full
                                                     in tests/gametime: its results follow area A at 0x0402e000) */
/* oItem's cimg (its Draw event's price-tag frame counter; the play code keeps oDamsel's only): per instance slot,
   with the id it belongs to; counted at each draw_frame for every visible item with a price, as the Draw event is
   run for every visible instance */
static uint8_t icimg[PIN_MAX] DRAW_MAPS_SECTION;
static int32_t icid[PIN_MAX] DRAW_MAPS_SECTION;
static uint32_t mdepth_key[NMAPS];                /* fkey of the map's depth */
static float mdepth[NMAPS];
static int nmaps;                                 /* tilemaps in use (1..3 of them) */
static int cols, rows, ncells;                    /* the room in 16-px cells */
static int wc0, wc1, wr0, wr1;                    /* the cells the screen shows (view lines 8..231): the tilemap
                                                     cells kept up to date (others are written when they come in) */
static int bg_spr = -1, bg_shown = -1;            /* the background sprite (SPR_*), and the one tilemap 0 holds */
static uint8_t bg_dirty, maps_cleared, frame_pending, frame_mid;
static int32_t built_rooms = -1;                  /* play_rooms_entered at the last tilemap build */
static int built_room = -1;

/* tile_add tiles not on a tilemap: single-tile sprites */
struct tspr { uint32_t dkey; int16_t seq; uint16_t tile; int16_t x, y; };
static struct tspr tspr[TSPR_MAX] DRAW_MAPS_SECTION;
static int ntspr;
static uint32_t tdel[GTILES_MAX / 32 + 1];        /* tile_delete'd gtiles */
static uint8_t tiles_dirty;

/* the frame's drawables: instances (i >= 0) and tile sprites (i = -1 - k) */
struct ent { uint32_t dkey; int32_t id; int16_t i; uint8_t dk; };   /* dk: draw_kind[obj] (instances) */
static struct ent ents[ENT_MAX];
static uint16_t ord[ENT_MAX];                     /* ents in drawing order */

/* ---- display list helpers --------------------------------------------------------------------------------- */
/* The frame's sprite entries are written straight into sprite RAM (docs/DRAW.md): a run of entries in draw.c's own
   sublist area, closed into one main-list record (cps3v_object at position 0, each entry's own colour code) before a
   tilemap band, the front end's text, the HUD and at 511 entries. Two 16 KB areas used in turn (the SDK's rule for
   its own sublists: the list on screen is never rewritten), at the end of CPS3V_PRE_A (0x38000-0x3ffff; the test
   builds keep arrays from the area's start: tests/game/sprbss.ld). The entry words are those of cps3v_sprite (word0
   tile / flip / colour, word1 position, word2 size). */
#define RUN_AREA     (CPS3V_PRE_A_END - 0x8000u)  /* .. CPS3V_PRE_A_END */
#define RUN_SIZE     0x4000u
/* smooth motion: the midpoint list's entries at the same place MID_OFF bytes lower (0x30000-0x37fff), in the same
   records */
#define MID_OFF      0x8000u
static uint32_t run_at, run_end, run_odd;         /* the open run's start (byte); the area's end */
static uint32_t w2tab[5][5];                      /* word 2 by width and height in tiles (1, 2, 4) */
/* Word 3 of an entry is always 0: every entry slot of the run areas (16-byte steps from RUN_AREA - MID_OFF) has it
   written once (draw_boot), so ent_put writes words 0-2 only. */
#if DRAW_SPRDMA
/* DRAW_SPRDMA: the entries are built in main RAM (sbuf, two halves of SB_ENT entries: the run area's next SB_ENT
   slots, and with smooth motion the midpoint list's) and sent to sprite RAM by the SH-2 DMAC in 16-byte units (CHCR
   0x5e01: destination and source incrementing, 16-byte units, auto request, cycle steal; TCR counts longwords: MAME
   sh7604.cpp count &= ~3, -4 a unit; jtframe sh7604 DMAC.sv one count a longword beat), channel 0 the run area,
   channel 1 the midpoint area. A half is sent when full (the other is written meanwhile; its own transfer, started
   a half earlier, is waited for first) and at the end of the frame's runs (draw_sprdma_end); draw_list_send waits
   for the last transfer before the list DMA. The SH-2 cache is write-through, so the DMAC reads what was stored.
   Slots skipped by run_close's 256-byte alignment are sent with whatever the half held (they are not in a run). */
#define SB_ENT       8
#define SB_BYTES     (SB_ENT * 16u)
static uint32_t sbuf[2][2][SB_ENT * 4] __attribute__((aligned(16)));   /* [half][run, midpoint][words] */
static uint32_t *sb_lim;                          /* the end of the half being written (EW.run_p: its next entry) */
static uint32_t sb_base;                          /* the sprite RAM byte its first slot goes to */
static int sb_h;                                  /* the half being written */
static uint8_t sb_mid;                            /* this frame's halves have midpoint entries (mid_on) */
#define SB_MIDW      (SB_ENT * 4)                 /* words from a run entry to its midpoint entry */
#ifndef DRAW_HOST
#define DMAC_SAR(c)  (*(volatile uint32_t *)(0xffffff80u + 0x10u * (c)))
#define DMAC_DAR(c)  (*(volatile uint32_t *)(0xffffff84u + 0x10u * (c)))
#define DMAC_TCR(c)  (*(volatile uint32_t *)(0xffffff88u + 0x10u * (c)))
#define DMAC_CHCR(c) (*(volatile uint32_t *)(0xffffff8cu + 0x10u * (c)))
#define DMAC_DMAOR   (*(volatile uint32_t *)0xffffffb0u)
static uint8_t ch_busy[2];
static void dmac_wait(int c)                      /* the channel's transfer has ended (TE); DE and TE cleared */
{
    if (!ch_busy[c]) return;
    while (!(DMAC_CHCR(c) & 2)) ;
    DMAC_CHCR(c) = 0;
    ch_busy[c] = 0;
}
static void dmac_start(int c, const uint32_t *src, uint32_t spr, uint32_t bytes)
{
    dmac_wait(c);
    DMAC_SAR(c) = (uint32_t)src;
    DMAC_DAR(c) = 0x04000000u + spr;
    DMAC_TCR(c) = bytes / 4;
    DMAC_CHCR(c) = 0x5e01u;
    ch_busy[c] = 1;
}
#endif
#ifdef DRAW_HOST
static void sb_host_copy(uint32_t n)              /* the host's transfer: n bytes of the half to host_sprram */
{
    uint32_t k;
    for (k = 0; k < n / 4; k++) {
        host_sprram[sb_base / 4 + k] = sbuf[sb_h][0][k];
        if (sb_mid) host_sprram[(sb_base - MID_OFF) / 4 + k] = sbuf[sb_h][1][k];
    }
}
#endif
/* sends the half being written, slots [sb_base, top), and goes on in the other half from sprite RAM byte next */
static void sb_send(uint32_t top, uint32_t next)
{
    uint32_t n = top - sb_base;
    if (n) {
        __asm__ volatile("" ::: "memory");       /* the entries are stored before the transfer starts */
#ifdef DRAW_HOST
        sb_host_copy(n);
#else
        dmac_start(0, sbuf[sb_h][0], sb_base, n);
        if (sb_mid) dmac_start(1, sbuf[sb_h][1], sb_base - MID_OFF, n);
#endif
        sb_h ^= 1;
    }
    sb_base = next;
    EW.run_p = sbuf[sb_h][0];
    sb_lim = EW.run_p + SB_ENT * 4;
}
static void sb_full(void) { sb_send(sb_base + SB_BYTES, sb_base + SB_BYTES); }
/* the next entry goes to sprite RAM byte a (a run's start: at or past the last entry's end) */
static void sb_seek(uint32_t a)
{
    if (a - sb_base >= SB_BYTES) sb_send(sb_base + 4 * (uint32_t)(EW.run_p - sbuf[sb_h][0]), a);
    else EW.run_p = sbuf[sb_h][0] + (a - sb_base) / 4;
}
static void draw_sprdma_end(void)                 /* after the frame's last run: what is left is sent */
{
    uint32_t top = sb_base + 4 * (uint32_t)(EW.run_p - sbuf[sb_h][0]);
    sb_send(top, top);
}
static void draw_sprdma_sync(void)                /* every transfer has ended */
{
#ifndef DRAW_HOST
    dmac_wait(0);
    dmac_wait(1);
#endif
}
#else
static void draw_sprdma_end(void) {}
static void draw_sprdma_sync(void) {}
#endif

/* run_p may take entries below ent_lim with none of ent_put's checks firing (fewer than DRAW_ENTRIES_MAX entries,
   room for the entry in the run area, fewer than 511 in the run): piece_put's fast path. ent_room sets it after every
   change of ent_n, run_n or run_at other than piece_put's own entries (ent_put's keep it valid: one entry and one
   step of run_p, or run_close's ent_room). DRAW_SPRDMA: not used, every entry by ent_put */
#if DRAW_SPRDMA
static void ent_room(void) {}
#else
static void ent_room(void)
{
    int32_t r = 511 - (int32_t)EW.run_n, a = ((int32_t)run_end - (int32_t)run_at) / 16 - (int32_t)EW.run_n,
            b = DRAW_ENTRIES_MAX - (int32_t)EW.ent_n;
    if (a < r) r = a;
    if (b < r) r = b;
    EW.ent_lim = EW.run_p + 4 * (r > 0 ? r : 0);
}
#endif

static void run_begin(void)
{
    run_odd ^= 1;
    run_at = RUN_AREA + (run_odd ? RUN_SIZE : 0);
    run_end = run_at + RUN_SIZE;
    EW.run_n = 0;
#if DRAW_SPRDMA
    sb_base = run_at;
    EW.run_p = sbuf[sb_h][0];
    sb_lim = EW.run_p + SB_ENT * 4;
#else
    EW.run_p = SPR_AT(run_at);
#endif
    ent_room();
}
static void run_close(void)
{
    if (!EW.run_n) return;
#if DRAW_SPRDMA && defined(DRAW_HOST)
    sb_host_copy(4 * (uint32_t)(EW.run_p - sbuf[sb_h][0]));   /* the host's cps3v_object decodes the run at once */
#endif
    cps3v_object(run_at, EW.run_n, 0, 0, -1);
    run_at = (run_at + EW.run_n * 16 + 255) & ~255u;
    EW.run_n = 0;
#if DRAW_SPRDMA
    sb_seek(run_at);
#else
    EW.run_p = SPR_AT(run_at);
#endif
    ent_room();
}
/* the sprite cps3v_sprite(px, py, w, h, tile, pal, flip) would write (word 3, 0, is already there) */
static inline __attribute__((always_inline)) void ent_put(int px, int py, unsigned w, unsigned h, uint32_t tile,
                                                          uint32_t pal, uint32_t flip)
{
    uint32_t w0, w2;
    if (EW.ent_n >= DRAW_ENTRIES_MAX || run_at + EW.run_n * 16 + 16 > run_end) {
        draw_st.dropped++;
        return;
    }
    if (EW.run_n == 511) run_close();
#if DRAW_SPRDMA
    if (EW.run_p == sb_lim) sb_full();
    uint32_t *e = EW.run_p;
#else
    spr_word *e = EW.run_p;
#endif
    w0 = tile << 17 | flip | pal;
    w2 = w2tab[w][h];
    e[0] = w0;
    e[1] = ((uint32_t)(px + 8 * (int)w - 1) & 0x3ff) << 16 | ((uint32_t)(1006 - py - 8 * (int)h) & 0x3ff);
    e[2] = w2;
    if (EW.mid_on) {
#if DRAW_SPRDMA
        uint32_t *m = e + SB_MIDW;
#else
        spr_word *m = e - MID_OFF / 4;
#endif
        m[0] = w0;
        m[1] = ((uint32_t)(px + EW.mdx + 8 * (int)w - 1) & 0x3ff) << 16 |
               ((uint32_t)(1006 - py - EW.mdy - 8 * (int)h) & 0x3ff);
        m[2] = w2;
    }
    EW.run_p = e + 4;
    EW.run_n++;
    EW.ent_n++;
}

/* piece pc's entry at screen (px, py): piece_put inline when run_p is below ent_lim (the entry words as ent_put's),
   else piece_out (ent_put's checks, then ent_lim again) */
static __attribute__((noipa)) void piece_out(int px, int py, const struct piecedef *pc, int flip)
{
    ent_put(px, py, pc->w, pc->h, pc->tile, EW.cur_pal, flip ? CPS3V_FLIPX : 0);
    ent_room();
}
#if !DRAW_SPRDMA
/* the entry (w x h tiles at screen (px, py)) as ent_put writes it, when EW.run_p < EW.ent_lim */
static inline __attribute__((always_inline)) void ent_words(int px, int py, unsigned w, unsigned h, uint32_t tile,
                                                            int flip)
{
    spr_word *e = EW.run_p;
    uint32_t w0 = tile << 17 | (flip ? CPS3V_FLIPX : 0) | EW.cur_pal, w2 = w2tab[w][h];
    e[0] = w0;
    e[1] = ((uint32_t)(px + 8 * (int)w - 1) & 0x3ff) << 16 | ((uint32_t)(1006 - py - 8 * (int)h) & 0x3ff);
    e[2] = w2;
    if (EW.mid_on) {
        spr_word *m = e - MID_OFF / 4;
        m[0] = w0;
        m[1] = ((uint32_t)(px + EW.mdx + 8 * (int)w - 1) & 0x3ff) << 16 |
               ((uint32_t)(1006 - py - EW.mdy - 8 * (int)h) & 0x3ff);
        m[2] = w2;
    }
    EW.run_p = e + 4;
    EW.run_n++;
    EW.ent_n++;
}
#endif
static inline __attribute__((always_inline)) void piece_put(int px, int py, const struct piecedef *pc, int flip)
{
#if DRAW_SPRDMA
    piece_out(px, py, pc, flip);
#else
    if (EW.run_p >= EW.ent_lim) {
        piece_out(px, py, pc, flip);
        return;
    }
    ent_words(px, py, pc->w, pc->h, pc->tile, flip);
#endif
}

/* frame f (framedefs) with its origin at screen (x, y); flip: mirrored about x (image_xscale -1). frame_out_n: any
   number of pieces; frame_out: one piece inline (97.8 % of frame draws, docs/DRAW.md section 2) with every call a
   tail call (no registers saved), more by frame_out_n */
static inline __attribute__((always_inline)) int piece_at(const struct piecedef *pc, int x, int y, int flip, int *px,
                                                          int *py)   /* the piece's place; 0 when off the clip */
{
    int w = 16 * pc->w;
    *px = flip ? x - pc->dx - w : x + pc->dx;
    *py = y + pc->dy;
    return !(*px >= EW.cxh || *py >= EW.cyh || *px + w <= EW.cxl || *py + 16 * pc->h <= EW.cyl);
}
static __attribute__((noinline)) void frame_out_n(const struct framedef *fd, int x, int y, int flip)
{
    const struct piecedef *pc = &piecedefs[fd->piece], *end = pc + fd->npieces;
    int px, py;
    for (; pc < end; pc++)
        if (piece_at(pc, x, y, flip, &px, &py)) piece_put(px, py, pc, flip);
}
static void frame_out(int f, int x, int y, int flip)
{
    const struct framedef *fd = &framedefs[f];
    const struct piecedef *pc;
    int px, py;
    if (fd->npieces != 1) {
        frame_out_n(fd, x, y, flip);
        return;
    }
    pc = &piecedefs[fd->piece];
    if (piece_at(pc, x, y, flip, &px, &py)) piece_put(px, py, pc, flip);
}

/* sprite s (SPR_*), image img (whole), at room (x, y) */
static void spr_out(int s, int32_t img, int x, int y, int flip)
{
    const struct sprdef *sd;
    if (s < 0) {
        draw_st.noart++;
        return;
    }
    sd = &sprdefs[s];
    if (img < 0) img = 0;
    if ((uint32_t)img >= sd->nframes) img = (int32_t)((uint32_t)img % sd->nframes);
    frame_out(sd->frame + img, x - EW.ox, y - EW.oy, flip);
}

/* global.sSmallCollectNew frame k (tools/hudart.py: 8 x 10 at a tile's top-left, origin 4, 4) at room (x, y) */
static void collect_out(int k, int x, int y)
{
    int px = x - HUD_COLLECT_XORIG - EW.ox, py = y - HUD_COLLECT_YORIG - EW.oy;
    if (px >= EW.cxh || py >= EW.cyh || px + 16 <= EW.cxl || py + 16 <= EW.cyl) return;
    ent_put(px, py, 1, 1, HUD_TILE_COLLECT(k), EW.cur_pal == DRAW_PAL ? DRAW_PAL_HUDDARK : HUD_PAL, 0);
}

static void band_out(int tm)
{
    if (EW.ent_n >= DRAW_ENTRIES_MAX) {
        draw_st.dropped++;
        return;
    }
    run_close();
    cps3v_group();
    cps3v_band(tm, 0, CPS3V_H);
    cps3v_group();
    EW.ent_n += 2;                                   /* 224 lines: two band entries of at most 128 lines */
    ent_room();
}

/* ---- tilemaps ---------------------------------------------------------------------------------------------- */
/* the tile of the 16 x 16 cell at (left, top) of frame 0 of sprite s; 0 if the cell is not a whole piece cell */
static uint16_t cell_tile(int s, int left, int top)
{
    const struct sprdef *sd = &sprdefs[s];
    const struct framedef *fd = &framedefs[sd->frame];
    const struct piecedef *pc = &piecedefs[fd->piece], *end = pc + fd->npieces;
    for (; pc < end; pc++) {
        int px = left - (pc->dx + sd->xorig), py = top - (pc->dy + sd->yorig);
        if (px >= 0 && py >= 0 && px < 16 * pc->w && py < 16 * pc->h && !(px & 15) && !(py & 15))
            return (uint16_t)(pc->tile + (px >> 4) * pc->h + (py >> 4));
    }
    return 0;
}

static int map_of_depth(float d)
{
    uint32_t k = fkey(d);
    int m;
    for (m = 0; m < nmaps; m++)
        if (mdepth_key[m] == k) return m;
    return -1;
}

/* a float that is a multiple of 16 in [0, 1024): its value / 16, else -1 (integer operations only). Value =
   m x 2^(e - 150) for biased exponent e and m with the implicit bit; a multiple of 16 when the low 154 - e bits of
   m are 0, and value / 16 = m >> (154 - e) */
static int f16(uint32_t u)
{
    uint32_t m = (u & 0x7fffffu) | 0x800000u;
    if (!u) return 0;
    switch (u >> 23) {                            /* sign 0: 16 .. 1023 */
    case 131: return (m & 0x7fffffu) ? -1 : (int)(m >> 23);
    case 132: return (m & 0x3fffffu) ? -1 : (int)(m >> 22);
    case 133: return (m & 0x1fffffu) ? -1 : (int)(m >> 21);
    case 134: return (m & 0x0fffffu) ? -1 : (int)(m >> 20);
    case 135: return (m & 0x07ffffu) ? -1 : (int)(m >> 19);
    case 136: return (m & 0x03ffffu) ? -1 : (int)(m >> 18);
    default: return -1;
    }
}

/* the single 16 x 16 tile of instance p's frame when it is a terrain cell: returns the tile, the cell in *c (column
   and row in t_cx, t_cy) */
static int t_cx, t_cy;
static uint16_t terrain_cell(int pi, int *c)
{
    int s, cx, cy;
    const struct sprdef *sd;
    const struct framedef *fd;
    const struct piecedef *pc;
    uint32_t f = 0;
    if (!(draw_kind[I_OBJ(pi)] & DK_SOLID) || I_SPR(pi) < 0 || (s = draw_spr[I_SPR(pi)]) < 0) return 0;
    if (fbits(I_XSCALE(pi)) != F_ONE || fbits(I_YSCALE(pi)) != F_ONE || !fl_zero(I_ANGLE(pi)))
        return 0;
    sd = &sprdefs[s];
    if (sd->nframes > 1) {
        int32_t i = ftoi(I_IMG(pi));
        f = i < 0 ? 0 : (uint32_t)i % sd->nframes;
    }
    fd = &framedefs[sd->frame + f];
    if (fd->npieces != 1) return 0;
    pc = &piecedefs[fd->piece];
    if (pc->w != 1 || pc->h != 1 || (pc->dx & 15) || (pc->dy & 15)) return 0;
    cx = f16(fbits(I_X(pi)));
    cy = f16(fbits(I_Y(pi)));
    if (cx < 0 || cy < 0) return 0;
    cx += pc->dx >> 4;
    cy += pc->dy >> 4;
    if (cx < 0 || cy < 0 || cx >= cols || cy >= rows) return 0;
    *c = cy * cols + cx;
    t_cx = cx;
    t_cy = cy;
    return pc->tile;
}

/* the transform terrain cells and the cached draws need: image_xscale, image_yscale 1 (xscale -1 allowed for draws,
   *flip), image_angle 0, by bit patterns */
static inline __attribute__((always_inline)) int plain_transform(int pi, int *flip)
{
    uint32_t xh = fbits(I_XSCALE(pi));
    if ((xh != F_ONE && xh != F_MONE) || fbits(I_YSCALE(pi)) != F_ONE || !fl_zero(I_ANGLE(pi)))
        return 0;
    *flip = xh == F_MONE;
    return 1;
}

/* per slot, a draw_self frame computed from fields that rarely change, with those fields as bit patterns (x, y,
   image_index, sprite; the transform checked as plain_transform each time; m = flip). kind: 0 none; TC_NOART the
   sprite has no art; TC_FRAME frame c (framedefs) at room (cx, cy), drawn by frame_out; TC_PIECE the same, frame c
   being one piece: its place as frame_out gives it (rx = cx + dx, or cx - dx - 16 pw mirrored; ry = cy + dy; when
   they fit int16), pw x ph tiles, tile. 32 bytes (an index is a shift); the bytes before the int16s (mov.b reaches
   15 bytes, mov.w 30) */
#define TC_NOART 1
#define TC_FRAME 2
#define TC_PIECE 3
struct tcache { uint32_t xb, yb, ib; uint8_t kind, pw, ph; int8_t m; int16_t spr, cx, cy, rx, ry, c; uint16_t tile; };
typedef char tcache_size[sizeof(struct tcache) == 32 ? 1 : -1];
static struct tcache tcache[PIN_MAX] DRAW_CACHE_SECTION;

/* smooth motion: per slot, the instance drawn there at the last draw (id, whole-pixel x / y, draw count) */
struct hist { int32_t id; int16_t x, y; uint16_t stamp; };
static struct hist hist[PIN_MAX] DRAW_CACHE_SECTION;
static uint16_t dstamp;                           /* draw_frame calls (smooth motion) */
static int pvx = -1000, pvy = -1000;              /* the last draw's camera */
/* fpix(x), fpix(y) as inst_mid hands them to inst_out and cached_fill: x << 16 | y (16 bits each) when both are in
   -32767 .. 32767, else NOXY (the draw does not change x, y) */
#define NOXY 0x80008000u
static inline uint32_t xy_pack(int x, int y)
{
    return x < -32767 || x > 32767 || y < -32767 || y > 32767 ? NOXY : (uint32_t)(uint16_t)x << 16 | (uint16_t)y;
}
/* instance i is drawn now: its midpoint offset (halfway back to its last draw's place: dx / 2, as the camera's ocx)
   when mid_on, and its place kept for the next draw; returns xy_pack(fpix(x), fpix(y)) */
static uint32_t inst_mid(int i, uint16_t stamp)       /* stamp: dstamp (a local: hist's uint16_t stores would make GCC
                                                     load dstamp again for each instance) */
{
    struct hist *h = &hist[i];
    const struct tcache *c = &tcache[i];
    int32_t id = I_ID(i);
    uint32_t xb = fbits(I_X(i)), yb = fbits(I_Y(i));
    int x, y, dx = 0, dy = 0;
    /* x, y = fpix(x), fpix(y): the slot cache's cx, cy when it holds them for these bit patterns (any kind: filled
       from them) and they fit its int16 (|x|, |y| < 16384: fpix within +-16384) */
    if (c->kind && c->xb == xb && c->yb == yb && (xb & 0x7fffffffu) < 0x46800000u && (yb & 0x7fffffffu) < 0x46800000u) {
        x = c->cx;
        y = c->cy;
    } else {
        x = fpix(I_X(i));
        y = fpix(I_Y(i));
    }
    if (h->id == id && h->stamp == (uint16_t)(stamp - 1)) {   /* (stores only what changes: h->id is id, and */
        dx = h->x - x;                                          /* with dx, dy 0 h->x, h->y are x, y) */
        dy = h->y - y;
        if (dx | dy) {
            h->x = (int16_t)x;
            h->y = (int16_t)y;
        }
        if (dx < -DRAW_MID_JUMP || dx > DRAW_MID_JUMP || dy < -DRAW_MID_JUMP || dy > DRAW_MID_JUMP) dx = dy = 0;
    } else {
        h->id = id;
        h->x = (int16_t)x;
        h->y = (int16_t)y;
    }
    h->stamp = stamp;
    if (EW.mid_on) set_mid(dx / 2 - EW.ocx, dy / 2 - EW.ocy);
    return xy_pack(x, y);
}

/* the tile_add layers into mbase / tspr (gtiles less the deleted ones). A layer draws its tiles in element order,
   newest first (layer_get_all_elements; checked against the runner's frame: build/trace/g_p4_exit559_s559 record
   300, two bgExtras at one place): gtiles from the last; the first drawn of a cell takes the tilemap */
static void build_tiles(void)
{
    int k, m;
    for (m = 0; m < nmaps; m++)
        for (k = 0; k < ncells; k++) mbase[m][k] = 0;
    ntspr = 0;
    for (k = gntiles - 1; k >= 0; k--) {
        const struct gtile *t = &gtiles[k];
        int s = t->bg >= 0 ? draw_spr[t->bg] : -1, i, j;
        if (s < 0 || (tdel[k >> 5] >> (k & 31) & 1)) continue;
        m = map_of_depth((float)t->depth);
        for (j = 0; j < t->h / 16; j++)
            for (i = 0; i < t->w / 16; i++) {
                uint16_t tile = cell_tile(s, t->left + 16 * i, t->top + 16 * j);
                int X = t->x + 16 * i, Y = t->y + 16 * j, c;
                if (!tile) continue;
                c = (Y >> 4) * cols + (X >> 4);
                if (m >= 0 && !((X | Y) & 15) && X >= 0 && Y >= 0 && (X >> 4) < cols && (Y >> 4) < rows &&
                    !mbase[m][c]) {
                    mbase[m][c] = tile;
                } else if (ntspr < TSPR_MAX) {
                    struct tspr *e = &tspr[ntspr++];
                    e->dkey = fkey((float)t->depth);
                    e->seq = (int16_t)(gntiles - 1 - k);   /* drawing order */
                    e->tile = tile;
                    e->x = (int16_t)X;
                    e->y = (int16_t)Y;
                }
            }
    }
    tiles_dirty = 0;
}

static void claims_reset(void);

/* a room started: the background, the three tilemap depths (most cells: tiles and terrain), the tile layers */
static void build_room(void)
{
    float dep[24];
    int cnt[24], nd = 0, k, m;
    cols = PW.room_w / 16;
    rows = PW.room_h / 16;
    if (cols > 64) cols = 64;
    if (cols * rows > MAPC_MAX) rows = MAPC_MAX / cols;
    ncells = cols * rows;
    bg_spr = PW.room == 1 /* rIntro */ ? SPR_backgroundNight :
             (PW.room == R_rEnd2 || PW.room == R_rEnd3 || PW.room == R_rCredits2) ? -1 :   /* no tiled layer */
             (PW.room == R_rOlmec || (G.levelType == 3 && !front_on)) ? SPR_bgTemple : SPR_bgCave;
    if (bg_spr != bg_shown) bg_dirty = 1;
    /* cells per depth */
    for (k = 0; k < gntiles; k++) {
        float d = (float)gtiles[k].depth;
        int n = (gtiles[k].w / 16) * (gtiles[k].h / 16), j;
        for (j = 0; j < nd && dep[j] != d; j++) ;
        if (j == nd) {
            if (nd == 24) continue;
            dep[nd] = d;
            cnt[nd++] = 0;
        }
        cnt[j] += n;
    }
    nmaps = 0;                                    /* terrain_cell needs cols / rows only */
    for (k = 0; k < PW.n; k++) {
        int pi = k;
        int c, j;
        if (!I_ALIVE(pi) || !I_VISIBLE(pi) || !terrain_cell(pi, &c)) continue;
        for (j = 0; j < nd && dep[j] != I_DEPTH(pi); j++) ;
        if (j == nd) {
            if (nd == 24) continue;
            dep[nd] = I_DEPTH(pi);
            cnt[nd++] = 0;
        }
        cnt[j]++;
    }
    /* the NMAPS depths with most cells, deepest first */
    for (m = 0; m < NMAPS; m++) {
        int best = -1;
        for (k = 0; k < nd; k++)
            if (cnt[k] > 0 && (best < 0 || cnt[k] > cnt[best])) best = k;
        if (best < 0) break;
        mdepth[nmaps++] = dep[best];
        cnt[best] = 0;
    }
    for (m = 0; m < nmaps; m++)                   /* deepest first */
        for (k = m + 1; k < nmaps; k++)
            if (fkey(mdepth[k]) > fkey(mdepth[m])) {
                float t = mdepth[k];
                mdepth[k] = mdepth[m];
                mdepth[m] = t;
            }
    for (m = 0; m < nmaps; m++) {
        mdepth_key[m] = fkey(mdepth[m]);
        for (k = 0; k < MAPC_MAX; k++) mshown[m][k] = 0xffff;   /* every cell written at the next draw_vblank */
    }
    mfull = 1;
    for (k = 0; k < GTILES_MAX / 32 + 1; k++) tdel[k] = 0;
    build_tiles();
    maps_cleared = 0;
    built_rooms = play_rooms_entered;
    built_room = PW.room;
    for (k = 0; k < PIN_MAX; k++) tcache[k].kind = 0;
    claims_reset();
    draw_st.room_builds++;
}

void draw_tile_delete(int depth, int x, int y)
{
    int k;
    for (k = gntiles - 1; k >= 0; k--) {          /* layer_get_all_elements: newest first */
        const struct gtile *t = &gtiles[k];
        if (t->depth != depth || (tdel[k >> 5] >> (k & 31) & 1)) continue;
        if (x >= t->x && x < t->x + t->w && y >= t->y && y < t->y + t->h) {
            tdel[k >> 5] |= bit32[k & 31];
            tiles_dirty = 1;
            return;
        }
    }
}

/* ---- instances --------------------------------------------------------------------------------------------- */
/* spr_local[GSPR_*]: the sprite has art and draws within 16 px of its origin (any frame, mirrored or not);
   dk_local[kind]: the Draw event draws only the sprite about its origin (and the price tag at y - 16 .. y - 7) */
static uint8_t spr_local[GSPR_COUNT];
/* spr_wide[GSPR_*]: larger than the coarse view test's margins (320 x 240): reaches the view from outside its window
   (the ending's backgrounds: sBGEnd3, 480 px, rEnd3's oBGEnd3 at x -368 covers x -368 .. 111) */
static uint8_t spr_wide[GSPR_COUNT];
static const uint8_t dk_local[DK_FRONT + 1] = { [DK_SELF] = 1, [DK_DAMSEL] = 1, [DK_ITEM] = 1, [DK_PLAIN] = 1,
                                               [DK_TODO] = 1 };

static int32_t img_of(int pi) { return ftoi(I_IMG(pi)); }
static int is_exit_spr(int s) { return s == GSPR_sPExit || s == GSPR_sDamselExit || s == GSPR_sTunnelExit; }

/* draw_self (flip = image_xscale -1) or draw_sprite(sprite_index, -1, x, y) (flip 0) of instance pi, through the
   slot cache (kind 2) when the transform is plain */
/* (cached_out's paths that call: kept out of it, so that a cache hit, every call made a tail call, saves no
   registers) */
static __attribute__((noinline)) void cached_frame(const struct tcache *e)   /* TC_NOART, TC_FRAME */
{
    if (e->kind == TC_NOART) {
        draw_st.noart++;
        return;
    }
    frame_out(e->c, e->cx - EW.ox, e->cy - EW.oy, e->m);
}
static inline __attribute__((always_inline)) void cached_draw(const struct tcache *e)
{
    int px, py;
    if (e->kind != TC_PIECE) {
        cached_frame(e);
        return;
    }
    px = e->rx - EW.ox;                           /* frame_out's one-piece path on the cached place */
    py = e->ry - EW.oy;
    if (px >= EW.cxh || py >= EW.cyh || px + 16 * e->pw <= EW.cxl || py + 16 * e->ph <= EW.cyl) return;
#if !DRAW_SPRDMA
    if (EW.run_p < EW.ent_lim) {
        ent_words(px, py, e->pw, e->ph, e->tile, e->m);
        return;
    }
#endif
    piece_out(px, py, &piecedefs[framedefs[e->c].piece], e->m);
}
static __attribute__((noinline)) void cached_odd(int pi, int mirror)   /* not plain_transform */
{
    if (mirror) draw_st.unsup++;
    spr_out(draw_spr[I_SPR(pi)], img_of(pi), fpix(I_X(pi)), fpix(I_Y(pi)), mirror && fl_neg(I_XSCALE(pi)));
}
/* the slot's entry from the fields (xy: inst_mid's fpix(x), fpix(y), or NOXY) */
static __attribute__((noinline)) void cached_fill(int pi, int flip, uint32_t xy)
{
    struct tcache *e = &tcache[pi];
    int32_t img = img_of(pi);
    int s, kind;
    e->xb = fbits(I_X(pi)); e->yb = fbits(I_Y(pi)); e->ib = fbits(I_IMG(pi));
    e->spr = I_SPR(pi);
    e->m = (int8_t)flip;
    if (xy != NOXY) {                             /* inst_mid's fpix(x), fpix(y) */
        e->cx = (int16_t)(xy >> 16);
        e->cy = (int16_t)xy;
    } else {
        e->cx = (int16_t)fpix(I_X(pi));
        e->cy = (int16_t)fpix(I_Y(pi));
    }
    s = draw_spr[I_SPR(pi)];
    kind = TC_NOART;
    if (s >= 0) {
        const struct sprdef *sd = &sprdefs[s];
        const struct framedef *fd;
        if (img < 0) img = 0;
        if ((uint32_t)img >= sd->nframes) img = (int32_t)((uint32_t)img % sd->nframes);
        e->c = (int16_t)(sd->frame + img);
        kind = TC_FRAME;
        fd = &framedefs[e->c];
        if (fd->npieces == 1) {
            const struct piecedef *pc = &piecedefs[fd->piece];
            int rx = flip ? e->cx - pc->dx - 16 * pc->w : e->cx + pc->dx, ry = e->cy + pc->dy;
            if (rx == (int16_t)rx && ry == (int16_t)ry) {
                e->rx = (int16_t)rx;
                e->ry = (int16_t)ry;
                e->pw = pc->w;
                e->ph = pc->h;
                e->tile = pc->tile;
                kind = TC_PIECE;
            }
        }
    }
    e->kind = (uint8_t)kind;
    cached_draw(e);
}
static void cached_out(int pi, int mirror, uint32_t xy)
{
    const struct tcache *e = &tcache[pi];
    int flip;
    if (!plain_transform(pi, &flip)) {
        cached_odd(pi, mirror);
        return;
    }
    flip &= mirror;
    if (!e->kind || e->xb != fbits(I_X(pi)) || e->yb != fbits(I_Y(pi)) || e->ib != fbits(I_IMG(pi)) ||
        e->spr != I_SPR(pi) || e->m != flip) {
        cached_fill(pi, flip, xy);
        return;
    }
    cached_draw(e);
}
static void self_out(int pi, int x, int y)
{
    if (I_SPR(pi) < 0) return;
    if (!scale_is_pm1(I_XSCALE(pi)) || fbits(I_YSCALE(pi)) != F_ONE || !fl_zero(I_ANGLE(pi)))
        draw_st.unsup++;
    spr_out(draw_spr[I_SPR(pi)], img_of(pi), x, y, fl_neg(I_XSCALE(pi)));
}
static void plain_out(int pi, int x, int y)
{
    if (I_SPR(pi) >= 0) spr_out(draw_spr[I_SPR(pi)], img_of(pi), x, y, 0);
}

/* scripts/characterDrawEvent (oPlayer1's Draw); image_xscale was set by the play code's ev_draw */
static void player_out(int pi, int x, int y)
{
    int32_t a = img_of(pi);
    int drawn = 1;
    if (PL.blinkToggle == 1) return;
    if ((PL.state == CLIMBING || is_exit_spr(I_SPR(pi))) && PG.hasJetpack && !PL.whipping) {
        self_out(pi, x, y);
        spr_out(SPR_sJetpackBack, a, x, y, 0);
        drawn = 0;
    } else if (PG.hasJetpack && PL.facing == RIGHT)
        spr_out(SPR_sJetpackRight, a, x - 4, y - 1, 0);
    else if (PG.hasJetpack)
        spr_out(SPR_sJetpackLeft, a, x + 4, y - 1, 0);
    if (drawn) {
        if (PL.redColor > 0) draw_st.unsup++;     /* make_color_rgb(200 + redColor, 0, 0) blend: not drawn */
        self_out(pi, x, y);
    }
    if (PL.facing == RIGHT) {
        if (PL.holdArrow == ARROW_NORM) spr_out(SPR_sArrowRight, a, x + 4, y + 1, 0);
        else if (PL.holdArrow == ARROW_BOMB) spr_out(SPR_sBombArrowRight, PL.holdArrowToggle ? 0 : 1, x + 4, y + 2, 0);
    } else if (PL.facing == LEFT) {
        if (PL.holdArrow == ARROW_NORM) spr_out(SPR_sArrowLeft, a, x - 4, y + 1, 0);
        else if (PL.holdArrow == ARROW_BOMB) spr_out(SPR_sBombArrowLeft, PL.holdArrowToggle ? 0 : 1, x - 4, y + 2, 0);
    }
}

/* objects/oPDummy/Draw_0.gml (the transition room's player; image_xscale set by ptrans_draw) */
static void pdummy_out(int pi, int x, int y)
{
    static const int16_t held[PICK_COUNT] = {
        [PICK_ROCK] = SPR_sRock, [PICK_JAR] = SPR_sJar, [PICK_SKULL] = SPR_sSkull, [PICK_FISHBONE] = SPR_sFishBone,
        [PICK_ARROW] = SPR_sArrowRight, [PICK_MACHETE] = SPR_sMacheteRight, [PICK_MATTOCK] = SPR_sMattockRight,
        [PICK_MATTOCKHEAD] = SPR_sMattockHead, [PICK_PISTOL] = SPR_sPistolRight, [PICK_WEBCANNON] = SPR_sWebCannonR,
        [PICK_TELEPORTER] = SPR_sTeleporter, [PICK_SHOTGUN] = SPR_sShotgunRight, [PICK_BOW] = SPR_sBowRight,
        [PICK_FLARE] = SPR_sFlare, [PICK_SCEPTRE] = SPR_sSceptreRight, [PICK_KEY] = SPR_sKeyRight,
    };
    int32_t a = img_of(pi);
    int ex = is_exit_spr(I_SPR(pi));
    if (ex) {
        self_out(pi, x, y);
        if (PG.hasJetpack) spr_out(SPR_sJetpackBack, a, x, y, 0);
        return;
    }
    if (PG.hasJetpack) spr_out(SPR_sJetpackRight, a, x - 4, y - 1, 0);
    self_out(pi, x, y);
    if (G.pickupItem > PICK_NONE && G.pickupItem < PICK_OTHER && held[G.pickupItem])
        spr_out(held[G.pickupItem], a, x + 4, y + 2, 0);
}

/* objects/oJaws/Draw_0.gml */
static void jaws_out(int pi, int x, int y)
{
    int b;
    plain_out(pi, x, y);
    if (I_SPR(pi) == GSPR_sJawsLeft) {
        b = I_HP(pi) < 10 ? SPR_sJawsBody3L : I_HP(pi) < 20 ? SPR_sJawsBody2L : SPR_sJawsBody1L;
        spr_out(b, 0, x + 16, y, 0);
    } else if (I_SPR(pi) == GSPR_sJawsRight) {
        b = I_HP(pi) < 10 ? SPR_sJawsBody3R : I_HP(pi) < 20 ? SPR_sJawsBody2R : SPR_sJawsBody1R;
        spr_out(b, 0, x - 48, y, 0);
    }
}

static void inst_out(int i, int dk, uint32_t xy)  /* dk: draw_kind[I_OBJ(i)] (ents' copy); xy: inst_mid's */
{
    int pi = i, x = 0, y = 0;
    dk &= ~DK_SOLID;
    if (dk != DK_SELF && dk != DK_PLAIN && dk != DK_ITEM && dk != DK_NONE) {
        if (xy != NOXY) {                         /* inst_mid's fpix(x), fpix(y) */
            x = (int16_t)(xy >> 16);
            y = (int16_t)xy;
        } else {
            x = fpix(I_X(pi));
            y = fpix(I_Y(pi));
        }
    }
    switch (dk) {
    case DK_NONE: break;
    case DK_TODO: draw_st.todo++; self_out(pi, x, y); break;
    case DK_SELF: if (I_SPR(pi) >= 0) cached_out(pi, 1, xy); break;
    case DK_DAMSEL:                               /* objects/oDamsel/Draw_0.gml: the price tag at cimg, which */
        self_out(pi, x, y);                        /* the play code's ev_draw has counted on already */
        if (I_COST(pi) > 0) collect_out(I_CIMG(pi) ? I_CIMG(pi) - 1 : 9, x, y - 12);
        break;
    case DK_ITEM:                                 /* objects/oItem/Draw_0.gml (cimg counted in draw_frame) */
        if (I_SPR(pi) >= 0) cached_out(pi, 0, xy);
        if (I_COST(pi) > 0) collect_out(icimg[i] ? icimg[i] - 1 : 9, fpix(I_X(pi)), fpix(I_Y(pi)) - 12);
        break;
    case DK_ENEMY:                                /* objects/oEnemy/Draw_0.gml (oEnemy: LEFT 0, RIGHT 1) */
        if (I_SPR(pi) < 0) break;
        if (I_FACING(pi) == 1) spr_out(draw_spr[I_SPR(pi)], img_of(pi), x + 16, y, 1);
        else spr_out(draw_spr[I_SPR(pi)], img_of(pi), x, y, 0);
        break;
    case DK_SHOP:                                 /* objects/oShopkeeper/Draw_0.gml (IDLE 0, FOLLOW 5) */
        if (I_SPR(pi) >= 0) {
            if (I_FACING(pi) == 1) spr_out(draw_spr[I_SPR(pi)], img_of(pi), x + 16, y, 1);
            else spr_out(draw_spr[I_SPR(pi)], img_of(pi), x, y, 0);
        }
        if (I_HASGUN(pi) && I_STATUS(pi) != 0 && I_STATUS(pi) != 5) {
            if (I_FACING(pi) == 0) spr_out(SPR_sShotgunLeft, 0, x + 6, y + 10, 0);
            else spr_out(SPR_sShotgunRight, 0, x + 10, y + 10, 0);
        }
        break;
    case DK_PLAIN: if (I_SPR(pi) >= 0) cached_out(pi, 0, xy); break;
    case DK_DICE:                                 /* objects/oDice/Draw_0.gml */
        self_out(pi, x, y);
        if (!I_ROLLED(pi) && PL.bet > 0) spr_out(SPR_sRedArrowDown, 0, x, y - 12, 0);
        break;
    case DK_PDUMMY: pdummy_out(pi, x, y); break;
    case DK_JAWS: jaws_out(pi, x, y); break;
    case DK_PLAYER: player_out(pi, x, y); break;
    case DK_FRONT: run_close(); front_draw(pi, EW.ox, EW.oy); break;   /* src/front: the attract rooms' Draw-event
                                                                     text (SDK entries: after the run's record) */
    }
}

/* ---- HUD ---------------------------------------------------------------------------------------------------- */
static uint8_t hud_held_of(int t)
{
    switch (t) {
    case T_NONE: return HUD_HELD_NONE;
    case T_ROCK: return HUD_HELD_ROCK;
    case T_JAR: return HUD_HELD_JAR;
    case T_SKULL: return HUD_HELD_SKULL;
    case T_FISHBONE: return HUD_HELD_FISHBONE;
    case T_ARROW: return HUD_HELD_ARROW;
    case T_MACHETE: return HUD_HELD_MACHETE;
    case T_MATTOCK: return HUD_HELD_MATTOCK;
    case T_MATTOCKHEAD: return HUD_HELD_MATTOCKHEAD;
    case T_PISTOL: return HUD_HELD_PISTOL;
    case T_WEBCANNON: return HUD_HELD_WEBCANNON;
    case T_TELEPORTER: return HUD_HELD_TELEPORTER;
    case T_SHOTGUN: return HUD_HELD_SHOTGUN;
    case T_BOW: return HUD_HELD_BOW;
    case T_SCEPTRE: return HUD_HELD_SCEPTRE;
    case T_FLARE: return HUD_HELD_FLARE;
    case T_KEY: return HUD_HELD_KEY;
    default: return HUD_HELD_OTHER;
    }
}

static char *cat(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *catn(char *d, int32_t n) { char b[12]; return cat(d, hud_itoa(n, b)); }

/* the lowest alive slot of object o (PW.n when none), as the scan for (k = 0; k < PW.n && !(I_ALIVE(k) &&
   I_OBJ(k) == o); k++) finds it, from o's list: pw_ohead / pw_inext hold exactly the alive instances of o (pworld.c
   links a slot where it sets alive, unlinks it where it clears it; slots below PW.n), so the lowest slot on the
   list is the scan's. The host build checks it against the scan */
static int first_of(int o)
{
    int k, best = PW.n;
    for (k = pw_ohead[o]; k >= 0; k = pw_inext[k])
        if (k < best) best = k;
#ifdef DRAW_HOST
    for (k = 0; k < PW.n && !(I_ALIVE(k) && I_OBJ(k) == o); k++) ;
    if (k != best) fprintf(stderr, "draw: first_of(%d) %d, scan %d\n", o, best, k);
#endif
    return best;
}

/* objects/oTransition/Draw_64.gml (English, room_offset 0): the level's end screen */
static void transition_out(void)
{
    int32_t t[4];
    char b[48], *e;
    int32_t s, s2, k;
    if (PW.room < R_rTransition1 || PW.room > R_rTransition4) return;
    if (first_of(OBJ_oTransition) == PW.n || !ptrans_gui(t)) return;
    k = first_of(OBJ_oDamselKiss);                /* oDamselKiss.kissed: "MY HERO!" */
    if (k < PW.n && I_TRIGGER(k)) hud_text_centered("MY HERO!", HUD_FONT_SMALL, 0, 0, 216);
    e = b;
    if (G.currLevel - 1 < 1) cat(b, "TUTORIAL CAVE COMPLETED!");
    else { e = cat(b, "LEVEL "); e = catn(e, G.currLevel - 1); cat(e, " COMPLETED!"); }
    hud_text(b, HUD_FONT_SMALL, 1, 32, 48);
    hud_text("TIME  = ", HUD_FONT_SMALL, 0, 32, 64);
    hud_text("LOOT  = ", HUD_FONT_SMALL, 0, 32, 80);
    hud_text("KILLS = ", HUD_FONT_SMALL, 0, 32, 96);
    hud_text("MONEY = ", HUD_FONT_SMALL, 0, 32, 112);
    if (t[0] >= 1 && !t[2]) hud_text("NONE", HUD_FONT_SMALL, 0, 96, 80);
    if (t[0] > -2) {                              /* m:ss / m2:ss2 of global.xtime and global.time (ms) */
        s = PG.xtime / 1000;
        s2 = PG.time / 1000;
        e = cat(hud_mss(b, s), " / ");
        hud_mss(e, s2);
        hud_text(b, HUD_FONT_SMALL, 0, 96, 64);
    }
    if (t[0] == 2) {
        if (!t[3]) hud_text("NONE", HUD_FONT_SMALL, 0, 96, 96);
        e = cat(b, "$");
        e = catn(e, t[1]);
        e = cat(e, " / $");
        catn(e, PG.money);
        hud_text(b, HUD_FONT_SMALL, 0, 96, 112);
    }
}

/* scripts/showEndMessage, a level's part (oGame Draw GUI after showMessages; English, room_offset 0): the game-over
   panel while oPlayer1 is dead. The prompt names the attack button as such (docs/GAMELOOP.md section 3) */
static void end_out(void)
{
    char b[16];
    if (PL.idx == NOONE || !I_ALIVE(PL.idx) || !PL.dead || !isLevel()) return;
    if (PGAME.drawStatus > 0) hud_text_centered("GAME OVER", HUD_FONT_LARGE, 1, 0, 32 + 16);
    if (PGAME.drawStatus > 1) hud_text_centered("FINAL SCORE:", HUD_FONT_SMALL, 1, 0, 64 + 16);
    if (PGAME.drawStatus > 2) {
        b[0] = '$';
        hud_itoa(PGAME.moneyCount, b + 1);
        hud_text_centered(b, HUD_FONT_LARGE, 0, 0, 72 + 16);
        hud_text_centered("PRESS ATTACK FOR HIGH SCORES.", HUD_FONT_SMALL, 1, 0, 120);
    }
}

/* scrDrawHUD / showMessages' state from the play state (docs/ARCADE.md §4) */
static void hud_out(void)
{
    static struct hud_state h;
    int k, game = -1;
    h.visible = PG.drawHUD && PL.idx != NOONE && I_ALIVE(PL.idx);
    h.life = PG.plife;
    h.bombs = PG.bombs;
    h.ropes = PG.rope;
    h.money = PG.money;
    h.collect = PG.collect;
    h.sticky_bombs = PG.hasStickyBombs;
    h.held = hud_held_of(PL.idx != NOONE ? PL.pickupItemType : T_NONE);
    h.items = (PG.hasUdjatEye ? HUD_UDJAT : 0) | (PG.hasAnkh ? HUD_ANKH : 0) | (PG.hasCrown ? HUD_CROWN : 0) |
              (PG.hasKapala ? HUD_KAPALA : 0) | (G.hasSpectacles ? HUD_SPECTACLES : 0) |
              (PG.hasGloves ? HUD_GLOVES : 0) | (PG.hasMitt ? HUD_MITT : 0) |
              (PG.hasSpringShoes ? HUD_SPRINGSHOES : 0) | (PG.hasSpikeShoes ? HUD_SPIKESHOES : 0) |
              (PG.hasCape ? HUD_CAPE : 0) | (PG.hasJetpack ? HUD_JETPACK : 0) | (PG.hasCompass ? HUD_COMPASS : 0) |
              (PG.hasParachute ? HUD_PARACHUTE : 0);
    h.udjat_blink = PG.udjatBlink;
    h.blood_level = S_BLOODLEVEL;
    h.arrows = PG.arrows;
    if ((k = first_of(OBJ_oGame)) < PW.n) game = k;   /* oGame.image_index */
    h.anim = game >= 0 ? ftoi(I_IMG(game)) : 0;
    if (PW.room == R_rOlmec) {                    /* scrDrawHUD :8: global.exitX / Y in rOlmec */
        h.exit_x = 640;
        h.exit_y = 544;
    } else {
        h.exit_x = G.exitX;
        h.exit_y = G.exitY;
    }
    h.view_x = PW.xview;
    h.view_y = PW.yview;
    h.message_timer = S_MSGTIMER;                 /* showMessages: global.message1 / 2 as drawn (src/game pmsg.h) */
    for (k = 0; k < 2; k++) {
        const struct pmsg *m = k ? &S_MSG2 : &S_MSG1;
        struct hud_msg *d = k ? &h.message2 : &h.message1;
        int j;
        for (j = 0; j <= HUD_MSG_MAX && j <= PMSG_MAX; j++) d->text[j] = m->text[j];
        d->text[HUD_MSG_MAX] = 0;
        d->yellow[0] = m->yellow[0];
        d->yellow[1] = m->yellow[1];
    }
    hud_draw(&h, DRAW_PAL_LIT);
}

/* ---- the frame ---------------------------------------------------------------------------------------------- */
static inline int ent_before(const struct ent *a, const struct ent *b)
{
    return a->dkey > b->dkey || (a->dkey == b->dkey && a->id > b->id);
}

/* ---- terrain claims and the instances the frame looks at --------------------------------------------------- */
#define CAND_W (PIN_MAX / 32 + 1)
static uint32_t texc[CAND_W];                     /* on a cell, not its tile: a sprite */
static uint32_t tother[CAND_W];                   /* drawable and not on a cell */
/* the instances drawn as sprites that draw within 16 px of their origin (spr_local, dk_local): in 64-px blocks by
   origin (clamped), so that a frame looks only at the blocks around the screen */
#define BLK_W 16
#define BLK_H 16
static int16_t bhead[BLK_H][BLK_W];
static int16_t bnext[PIN_MAX] DRAW_CACHE_SECTION, bpos[PIN_MAX] DRAW_CACHE_SECTION;   /* bpos: by * BLK_W + bx, -1 none */
static uint32_t cand[CAND_W];
static uint8_t hb8[256];                           /* the highest set bit of a byte */
/* in a block: a local sprite, not an oItem (its Draw event counts the price tag's frames on every frame) */
static int blk_local(int i)
{
    uint8_t dk = draw_kind[I_OBJ(i)] & ~DK_SOLID;
    return I_SPR(i) >= 0 && spr_local[I_SPR(i)] && dk_local[dk] && dk != DK_ITEM;
}
static int blk_of(float f, int n)
{
    int32_t v = ftoi(f);
    return v < 0 ? 0 : (uint32_t)v >> 6 >= (uint32_t)n ? n - 1 : (int)((uint32_t)v >> 6);
}

static int32_t dkey_of(int i) { return front_on ? front_drawkey(i) : I_ID(i); }

/* cell c of map m: its tile (the tile_add tile, else the claimant drawn first) and which claimants are sprites.
   The frame's old rule, kept: drawn first = largest draw key, then the higher slot */
static void cell_fix(int m, int c)
{
    int k, best = -1;
    int32_t bk = 0;
    if (!mbase[m][c])
        for (k = chead[m][c]; k >= 0; k = cnext[k]) {
            int32_t d = dkey_of(k);
            if (best < 0 || d > bk || (d == bk && k > best)) {
                best = k;
                bk = d;
            }
        }
    mwant[m][c] = mbase[m][c] ? mbase[m][c] : best >= 0 ? ctile[best] : 0;
    if (!mqueued[m][c]) {
        if (mqn < MQ_MAX) {
            mqueued[m][c] = 1;
            mq[mqn++] = (uint16_t)(m * MAPC_MAX + c);
        } else
            mfull = 1;
    }
    for (k = chead[m][c]; k >= 0; k = cnext[k])
        if (k == best) texc[k >> 5] &= ~bit32[k & 31];
        else texc[k >> 5] |= bit32[k & 31];
}

/* claim_update's unlink and link, the claim having changed: cellt the new cell | its tile << 16 (cell < 7,200:
   never 0xffffffff) or 0xffffffff none, blk the new block or -1 */
static __attribute__((noinline)) void claim_move(int i, int drawn, uint32_t cellt, int blk)
{
    uint32_t bit = bit32[i & 31];
    int cell = cellt == 0xffffffffu ? -1 : (int)(cellt & 0xffff);
    if (ccell[i] >= 0) {
        int om = (uint16_t)ccell[i] / MAPC_MAX, oc = (uint16_t)ccell[i] % MAPC_MAX;   /* ccell >= 0 */
        int16_t *pp = &chead[om][oc];
        while (*pp != i) pp = &cnext[*pp];
        *pp = cnext[i];
        ccell[i] = -1;
        texc[i >> 5] &= ~bit;
        cell_fix(om, oc);
    }
    tother[i >> 5] &= ~bit;
    if (bpos[i] >= 0) {
        int16_t *pp = &bhead[(uint16_t)bpos[i] / BLK_W][(uint16_t)bpos[i] % BLK_W];   /* bpos >= 0 */
        while (*pp != i) pp = &bnext[*pp];
        *pp = bnext[i];
        bpos[i] = -1;
    }
    if (!drawn) return;
    if (cell >= 0) {
        int m = cell / MAPC_MAX, c = cell % MAPC_MAX;
        cnext[i] = chead[m][c];
        chead[m][c] = (int16_t)i;
        ccell[i] = (int16_t)cell;
        ctile[i] = (uint16_t)(cellt >> 16);
        cell_fix(m, c);
    } else if (blk >= 0) {
        bnext[i] = bhead[(uint16_t)blk / BLK_W][(uint16_t)blk % BLK_W];
        bhead[(uint16_t)blk / BLK_W][(uint16_t)blk % BLK_W] = (int16_t)i;
        bpos[i] = (int16_t)blk;
    } else
        tother[i >> 5] |= bit;
}

/* the claim of instance i from its current fields: a terrain cell of a tilemap (terrain_cell: a solid's single
   16 x 16 tile, unscaled, on a cell; its depth one of the maps') when visible, else a drawable in a block (blk_local)
   or not (tother); none when not drawn. An instance holds at most one claim. When the claim is the one it holds,
   nothing is unlinked: a block or tother claim needs no change (no list's order is used: cell_fix's choice and the
   candidate bits do not depend on it), a cell claim keeps its place and the cell is fixed again with the tile (the
   slot may hold a new instance: another draw key); cell_fix(m, c) after an unlink and relink in the same cell gave
   the same mwant, texc bits and queue */
static void claim_update(int i)
{
    uint32_t bit = bit32[i & 31];
    int c = 0, m = -1, cell = -1, blk = -1, drawn, dk = DK_NONE;
    uint16_t t = 0;
    drawn = i < PW.n && I_ALIVE(i) && I_VISIBLE(i) && (dk = draw_kind[I_OBJ(i)]) != DK_NONE;
    if (drawn) {                                  /* (terrain_cell is 0 when dk has no DK_SOLID: tested first) */
        if (nmaps && (dk & DK_SOLID) && (t = terrain_cell(i, &c)) != 0 && (m = map_of_depth(I_DEPTH(i))) >= 0)
            cell = m * MAPC_MAX + c;
        else if (blk_local(i))
            blk = blk_of(I_Y(i), BLK_H) * BLK_W + blk_of(I_X(i), BLK_W);
    }
    if (cell >= 0 ? ccell[i] == cell : blk >= 0 ? bpos[i] == blk :
        ccell[i] < 0 && bpos[i] < 0 && (tother[i >> 5] & bit ? drawn : !drawn)) {
        if (cell >= 0) {
            ctile[i] = t;
            cell_fix(m, c);
        }
        return;
    }
    claim_move(i, drawn, cell >= 0 ? (uint32_t)cell | (uint32_t)t << 16 : 0xffffffffu, blk);
}

/* a room was built: every instance claimed again */
static void claims_reset(void)
{
    int m, k;
    const int16_t *dl;
    for (m = 0; m < NMAPS; m++)
        for (k = 0; k < MAPC_MAX; k++) {
            chead[m][k] = NOONE;
            mwant[m][k] = m < nmaps && k < ncells ? mbase[m][k] : 0;
        }
    for (k = 0; k < PIN_MAX; k++) ccell[k] = bpos[k] = -1;
    for (k = 0; k < BLK_W * BLK_H; k++) bhead[k / BLK_W][k % BLK_W] = NOONE;
    for (k = 0; k < CAND_W; k++) texc[k] = tother[k] = 0;
    for (k = 0; k < PW.n; k++) claim_update(k);
    pw_draw_dirty(&dl);
    pw_draw_dirty_clear();
}

/* the tile_add tiles changed (mbase): every cell again */
static void claims_refix(void)
{
    int m, k;
    for (m = 0; m < nmaps; m++)
        for (k = 0; k < ncells; k++) cell_fix(m, k);
}

/* fkey((float)v) without the soft-float conversion: below 2^24 in magnitude an int is a float exactly (exponent
   the highest set bit e, mantissa |v| x 2^(23 - e)); larger values (rounded) by the conversion. Needs hb8 (draw_boot) */
static uint32_t ifkey(int v)
{
    uint32_t a = v < 0 ? 0u - (uint32_t)v : (uint32_t)v, u;
    int e;
    if (a >= 1u << 24) return fkey((float)v);
    if (!a) return 0x80000000u;                   /* fkey(0.0f) */
    e = a >> 16 ? 16 + hb8[a >> 16] : a >> 8 ? 8 + hb8[a >> 8] : hb8[a];
    u = (uint32_t)(e + 127) << 23 | ((a * bit32[23 - e]) & 0x7fffffu);
    return v < 0 ? ~(u | 0x80000000u) : u | 0x80000000u;
}

/* the instances the frame looks at (cand bits; draw_frame takes them newest, highest slot, first): the drawables
   not on a cell or in a block, the cell claimants drawn as sprites (a cell's tile is drawn by its tilemap), and the
   blocks' instances around the screen (a local sprite shows only for its origin within (vx - 16, vx + 336) x
   (vy - 8, vy + 248)) */
/* v >> 6 (floor division by 64) without GCC's arithmetic-shift helper chain (___ashiftrt_r4_6 .. _1: the SH-2 shifts
   by 1, 2, 8 and 16 only): logical shifts of v or of -1 - v, which is the same floor for every int32 v */
static inline int asr6(int32_t v) { return v < 0 ? -1 - (int)((uint32_t)(-1 - v) >> 6) : (int)((uint32_t)v >> 6); }
static void scan_candidates(void)
{
    const int16_t *dl;
    int nd = pw_draw_dirty(&dl), k, bx, by;
    int bx0 = asr6(vx - vmx), bx1 = asr6(vx + VIEW_W + vmx), by0 = asr6(vy + DRAW_CROP - vmy);
    int by1 = asr6(vy + DRAW_CROP + SCREEN_H + vmy);
    for (k = 0; k < nd; k++) claim_update(dl[k]);
    pw_draw_dirty_clear();
#ifdef DRAW_HOST
    for (k = 0; k < PIN_MAX; k++) {               /* host check: the claims are those of the current fields */
        int c = 0, m = -1, want = -1, other = 0, blk = -1;
        uint16_t t = 0;
        if (k < PW.n && I_ALIVE(k) && I_VISIBLE(k) && draw_kind[I_OBJ(k)] != DK_NONE) {
            if (nmaps && (t = terrain_cell(k, &c)) != 0 && (m = map_of_depth(I_DEPTH(k))) >= 0) want = m * MAPC_MAX + c;
            else if (blk_local(k))
                blk = blk_of(I_Y(k), BLK_H) * BLK_W + blk_of(I_X(k), BLK_W);
            else other = 1;
        }
        if (ccell[k] != want || (want >= 0 && ctile[k] != t) || ((tother[k >> 5] >> (k & 31) & 1) != other) ||
            bpos[k] != blk)
            fprintf(stderr, "draw: claim of %d (obj %d) stale: cell %d want %d, block %d want %d\n", k,
                    k < PW.n ? I_OBJ(k) : -1, ccell[k], want, bpos[k], blk);
    }
#endif
    for (k = 0; k < CAND_W; k++) cand[k] = texc[k] | tother[k];
    if (bx0 < 0) bx0 = 0;
    if (by0 < 0) by0 = 0;
    if (bx1 > BLK_W - 1) bx1 = BLK_W - 1;
    if (by1 > BLK_H - 1) by1 = BLK_H - 1;
    for (by = by0; by <= by1; by++)
        for (bx = bx0; bx <= bx1; bx++)
            for (k = bhead[by][bx]; k >= 0; k = bnext[k]) cand[k >> 5] |= bit32[k & 31];
}

/* the candidates in view (scan_candidates' bits, newest first) as drawables from ents[n] on; returns the new count.
   bnd: the 16-px test's sxlo, sxhi, sylo, syhi and the coarse window's xlo, xhi, ylo, yhi (draw_frame). Its own
   function: the loop's values stay in registers */
static __attribute__((noinline)) int scan_ents(int n, const uint32_t *bnd)
{
    uint32_t v;
    int k, w;
    for (w = CAND_W - 1; w >= 0; w--) {           /* newest first: the sort below then moves little */
        for (v = cand[w]; v;) {
            int b = v >> 16 ? (v >> 24 ? 24 + hb8[v >> 24] : 16 + hb8[v >> 16 & 255])
                            : (v >> 8 ? 8 + hb8[v >> 8 & 255] : hb8[v & 255]);
            int pi;
            uint32_t kx, ky;
            uint8_t dk;
            v &= ~bit32[b];
            pi = k = 32 * w + b;
            if (!I_ALIVE(pi) || !I_VISIBLE(pi)) continue;
            dk = draw_kind[I_OBJ(pi)];
            if (dk == DK_NONE) continue;
            if (dk == DK_ITEM && I_COST(pi) > 0) {       /* oItem Draw: cimg += 1, 0 after 9 */
                if (icid[k] != I_ID(pi)) {
                    icid[k] = I_ID(pi);
                    icimg[k] = 0;
                }
                icimg[k] = icimg[k] >= 9 ? 0 : icimg[k] + 1;
            }
            kx = fkey(I_X(pi));
            ky = fkey(I_Y(pi));
            if (kx <= bnd[0] || kx >= bnd[1] || ky <= bnd[2] || ky >= bnd[3]) {   /* not within 16 px of the screen: */
                if (I_SPR(pi) < 0 || (spr_local[I_SPR(pi)] && dk_local[dk & ~DK_SOLID])) continue;   /* draws nothing */
                if (kx < bnd[4] || kx >= bnd[5] || ky < bnd[6] || ky >= bnd[7]) {   /* outside the window: */
                    const struct sprdef *sd;           /* only a sprite larger than its margins reaches the view */
                    float x = I_X(pi), y = I_Y(pi);
                    if (I_SPR(pi) < 0 || !spr_wide[I_SPR(pi)]) continue;
                    sd = &sprdefs[draw_spr[I_SPR(pi)]];
                    if (x <= (float)(vx - sd->w) || x >= (float)(vx + VIEW_W + sd->w) || y <= (float)(vy - sd->h) ||
                        y >= (float)(vy + VIEW_H + sd->h)) continue;
                }
            }
            if (n < ENT_MAX) {
                ents[n].dkey = fkey(I_DEPTH(pi));
                ents[n].id = front_on ? front_drawkey(pi) : I_ID(pi);
                ents[n].i = (int16_t)k;
                ents[n].dk = dk;
                n++;
            }
        }
    }
    return n;
}

/* the drawables in order (ord), with the tilemap bands at their depths' places; dark: from oLevel's place (lkey,
   lid) on, the unfaded colour code. Its own function: the loop's values stay in registers */
static __attribute__((noinline)) void list_ents(int n, int dark, uint32_t lkey, int32_t lid)
{
    int k, band = 0, smooth;
    uint16_t stamp;
    stamp = dstamp;
    smooth = draw_smooth;
    for (k = 0; k < n; k++) {
        const struct ent *e = &ents[ord[k]];
        if (dark && EW.cur_pal == DRAW_PAL && (e->dkey < lkey || (e->dkey == lkey && e->id < lid)))
            EW.cur_pal = DRAW_PAL_LIT;               /* after oLevel's rectangle */
        while (band < nmaps && mdepth_key[band] >= e->dkey) band_out(1 + band++);
        if (e->i >= 0) {
            inst_out(e->i, e->dk, smooth ? inst_mid(e->i, stamp) : NOXY);
        } else {
            const struct tspr *t = &tspr[-1 - e->i];
            int px = t->x - EW.ox, py = t->y - EW.oy;
            struct piecedef pc = { 0, 0, 1, 1, t->tile };
            if (EW.mid_on) set_mid(-EW.ocx, -EW.ocy);
            if (px + 16 > EW.cxl && px < EW.cxh && py + 16 > EW.cyl && py < EW.cyh) piece_out(px, py, &pc, 0);
        }
    }
    while (band < nmaps) band_out(1 + band++);
}

void draw_new_game(void)
{
    int k;
    built_rooms = -1;
    for (k = 0; k < PIN_MAX; k++) icid[k] = 0;   /* the price-tag counters as at boot: a new game reuses ids */
}

void draw_boot(void)
{
    int m, k;
    for (k = 0; k < GSPR_COUNT; k++) {
        int sp = draw_spr[k];
        const struct sprdef *sd = sp >= 0 ? &sprdefs[sp] : 0;
        spr_local[k] = sd && sd->w <= 16 && sd->h <= 16 && sd->xorig >= 0 && sd->xorig <= 16 && sd->yorig >= 0 &&
                       sd->yorig <= 16;
        spr_wide[k] = sd && (sd->w > 320 || sd->h > 240);
    }
    for (k = 1; k < 256; k++) hb8[k] = (uint8_t)(hb8[k >> 1] + (k > 1));
    for (m = 0; m <= NMAPS; m++) cps3v_tilemap(m, 0, 0, UNIT(m), 0);
    {                                             /* cps3v.c word2: sizes 1, 2, 4 tiles = codes 1, 2, 3 */
        static const uint8_t code[5] = { 0, 1, 2, 0, 3 };
        int w, h;
        for (w = 1; w <= 4; w *= 2)
            for (h = 1; h <= 4; h *= 2)
                w2tab[w][h] = ((uint32_t)(16 * h - 1) << 24) | ((uint32_t)(16 * w - 1) << 16) | (code[h] << 2) | code[w];
    }
    bg_shown = -1;
    built_rooms = -1;
    {                                             /* word 3 of every run and midpoint entry slot: 0 (ent_put) */
        uint32_t a;
        for (a = RUN_AREA - MID_OFF; a < RUN_AREA + 2 * RUN_SIZE; a += 16) SPR_AT(a)[3] = 0;
    }
#if DRAW_SPRDMA && !defined(DRAW_HOST)
    (void)DMAC_DMAOR;                             /* the DMAC on (DME; AE / NMIF read, then cleared) */
    DMAC_DMAOR = 0;
    DMAC_DMAOR = 1;
#endif
}

void draw_frame(void)
{
    int k, n = 0;
    uint32_t xlo, xhi, ylo, yhi;
    PROF0();
    draw_list_sync();                             /* the last list DMA has copied the main list */
    draw_st.frames++;
    draw_st.todo = draw_st.unsup = draw_st.noart = 0;
    EW.ent_n = 0;
    run_begin();
    EW.mid_on = 0;
    EW.ocx = EW.ocy = 0;
    if (built_rooms != play_rooms_entered || built_room != PW.room) {
        build_room();
        pvx = -1000;                              /* no midpoint across a room change */
    }
    if (tiles_dirty) {
        build_tiles();
        claims_refix();
    }
    vx = PW.xview;
    vy = PW.yview;
    EW.ox = vx;
    EW.oy = vy + DRAW_CROP;
    if (draw_smooth) {                            /* the midpoint camera; none after a jump */
        int dx = pvx - vx, dy = pvy - vy;
        dstamp++;
        if (dx >= -DRAW_MID_CAM && dx <= DRAW_MID_CAM && dy >= -DRAW_MID_CAM && dy <= DRAW_MID_CAM) {
            EW.mid_on = 1;
            EW.ocx = dx / 2;
            EW.ocy = dy / 2;
        }
        pvx = vx;
        pvy = vy;
    }
    vmx = 16 + (EW.ocx < 0 ? -EW.ocx : EW.ocx);
    vmy = 16 + (EW.ocy < 0 ? -EW.ocy : EW.ocy);
    set_mid(-EW.ocx, -EW.ocy);
#if DRAW_SPRDMA
    sb_mid = EW.mid_on;                              /* before the first entry */
#endif
    ZOOM_X = DRAW_ZOOM_X;
    /* tilemaps: base + terrain in the screen's cells; first claimant of a cell by creation order keeps it, the
       others are sprites */
    wc0 = vx >> 4;
    wc1 = (vx + VIEW_W - 1) >> 4;
    wr0 = (vy + DRAW_CROP) >> 4;
    wr1 = (vy + DRAW_CROP + SCREEN_H - 1) >> 4;
    if (wc1 >= cols) wc1 = cols - 1;
    if (wr1 >= rows) wr1 = rows - 1;
    PROF(0);
    /* coarse view test on the float bits: x in [vx - 320, vx + 640), y in [vy - 240, vy + 480); terrain only in the
       screen's cells */
    xlo = ifkey(vx - 320);
    xhi = ifkey(vx + 640);
    ylo = ifkey(vy - 240);
    yhi = ifkey(vy + 480);
    {
    /* sprites of at most 16 x 16 with the origin inside them draw within x - 16 .. x + 16: in view only for x in
       (vx - 16, vx + 336), y in (vy + 8 - 16, vy + 248) (smooth motion: wider by the camera's half step) */
    uint32_t sxlo = ifkey(vx - vmx), sxhi = ifkey(vx + VIEW_W + vmx);
    uint32_t sylo = ifkey(vy + DRAW_CROP - vmy), syhi = ifkey(vy + DRAW_CROP + SCREEN_H + vmy);
    for (k = 0; k < ntspr; k++) {                 /* tile sprites in view (first: their ids are the largest) */
        const struct tspr *t = &tspr[k];
        if (t->x <= vx - vmx || t->x >= vx + VIEW_W + vmx - 16 || t->y <= vy - vmy || t->y >= vy + VIEW_H + vmy - 16)
            continue;
        if (n < ENT_MAX) {
            ents[n].dkey = t->dkey;
            ents[n].id = 0x70000000 - (int32_t)t->seq;   /* before every instance of the depth */
            ents[n].i = (int16_t)(-1 - k);
            n++;
        }
    }
    {
        const uint32_t bnd[8] = { sxlo, sxhi, sylo, syhi, xlo, xhi, ylo, yhi };
        scan_candidates();
        n = scan_ents(n, bnd);
    }
    }
    PROF(1);
    /* sort: depth descending, then id descending. The drawables arrive in id order (tile sprites, then instances
       newest first), so a stable counting sort on the depth keys (few distinct depths) leaves only the rare
       displaced terrain owners out of place, which the insertion pass after it puts back */
    {
        static uint32_t keys[32];
        static uint16_t cnt[33];
        static uint8_t kx[ENT_MAX];
        int nk = 0, j, lj = -1;
        uint32_t ld = 0;
        for (k = 0; k < n; k++) {
            uint32_t d = ents[k].dkey;
            if (lj >= 0 && d == ld)               /* the last entry's key (keys[lj]; the keys are distinct) */
                j = lj;
            else {
                for (j = 0; j < nk && keys[j] != d; j++) ;
                if (j == nk) {
                    if (nk == 32) { nk = -1; break; }     /* more than 32 depths: insertion sort alone */
                    keys[nk++] = d;
                }
                ld = d;
                lj = j;
            }
            kx[k] = (uint8_t)j;
        }
        if (nk > 0) {
            uint8_t rank[32];
            for (j = 0; j < nk; j++) {           /* rank of each key, deepest (largest) first */
                int r = 0, q;
                for (q = 0; q < nk; q++) r += keys[q] > keys[j];
                rank[j] = (uint8_t)r;
            }
            for (j = 0; j <= nk; j++) cnt[j] = 0;
            for (k = 0; k < n; k++) cnt[rank[kx[k]] + 1]++;
            for (j = 1; j <= nk; j++) cnt[j] += cnt[j - 1];
            for (k = 0; k < n; k++) ord[cnt[rank[kx[k]]]++] = (uint16_t)k;
        } else
            for (k = 0; k < n; k++) ord[k] = (uint16_t)k;
    }
    if (front_on) {                               /* front rooms: room instances by layer position, not slot */
        static uint16_t tmp[ENT_MAX];             /* order: a bottom-up merge sort */
        int wdt, lo;
        uint16_t *a = ord, *b = tmp, *t;
        for (wdt = 1; wdt < n; wdt *= 2) {
            for (lo = 0; lo < n; lo += 2 * wdt) {
                int mid = lo + wdt < n ? lo + wdt : n, hi = lo + 2 * wdt < n ? lo + 2 * wdt : n;
                int i = lo, j = mid, o = lo;
                while (i < mid && j < hi) b[o++] = ent_before(&ents[a[j]], &ents[a[i]]) ? a[j++] : a[i++];
                while (i < mid) b[o++] = a[i++];
                while (j < hi) b[o++] = a[j++];
            }
            t = a; a = b; b = t;
        }
        if (a != ord)
            for (k = 0; k < n; k++) ord[k] = a[k];
    } else
    for (k = 1; k < n; k++) {                     /* insertion pass (nearly sorted: about n compares) */
        uint16_t o = ord[k];
        int j = k - 1;
        while (j >= 0 && ent_before(&ents[o], &ents[ord[j]])) {
            ord[j + 1] = ord[j];
            j--;
        }
        ord[j + 1] = o;
    }
    draw_st.sprites = (uint32_t)n;
    PROF(2);
    /* dark levels: oLevel's place in the order (its depth, then its id) and the alpha byte */
    {
        static int lvl = -1;
        int dark = 0;
        uint32_t lkey = 0;
        int32_t lid = 0;
        if (lvl < 0 || lvl >= PW.n || !I_ALIVE(lvl) || I_OBJ(lvl) != OBJ_oLevel)
            lvl = first_of(OBJ_oLevel);
        frame_a8 = 0;
        if (front_on) {                           /* the attract rooms' black rectangle (oIntro's fade) */
            int fa = 0, fi = front_fade(&fa);
            if (fi >= 0) {
                dark = 1;
                frame_a8 = fa;
                lkey = fkey(I_DEPTH(fi));
                lid = front_drawkey(fi);
            }
        } else if (lvl < PW.n && (draw_dark_force >= 0 || S_DARKLEVEL)) {
            double d = S_DARKNESS;
            dark = 1;
            frame_a8 = draw_dark_force >= 0 ? draw_dark_force : d <= 0 ? 0 : d >= 1 ? 255 : (int)(d * 255.0);
            lkey = fkey(I_DEPTH(lvl));
            lid = I_ID(lvl);
        }
        EW.cur_pal = DRAW_PAL;
    /* the list: background, then bands and drawables by depth */
    if (bg_spr >= 0) band_out(0);
    if (PW.room == R_rEnd3 || PW.room == R_rCredits2) {   /* the rooms' two bgClouds layers (480 x 200, not tiled */
        spr_out(SPR_bgClouds, 0, -160, 0, 0);             /* horizontally): layer 0 at x -160, then layer 1 at 160, */
        spr_out(SPR_bgClouds, 0, 160, 0, 0);              /* rEnd3's tiled vertically (rCredits2's not); rEnd2: */
        if (PW.room == R_rEnd3) spr_out(SPR_bgClouds, 0, 160, 200, 0);   /* its colour layer, under oEnd2BG */
    }
    list_ents(n, dark, lkey, lid);
    run_close();
    }
    draw_sprdma_end();                            /* the DMAC sends the rest while the HUD is drawn */
    set_mid(0, 0);
    frame_mid = EW.mid_on;
    PROF(3);
    if (draw_hud_on && !front_on) {
        hud_out();
        transition_out();
    }
    if (!front_on && pw_ohead[OBJ_oGame] >= 0) {  /* oGame Draw GUI */
        pmsg_frame();                             /* showMessages' countdown */
        if (draw_hud_on) end_out();               /* showEndMessage */
    }
    if (front_on) front_draw_gui();
    PROF(4);
    draw_sprdma_end();                            /* (nothing: no entries after the runs) */
    draw_st.entries = EW.ent_n;
    frame_pending = 1;
    if (EW.ent_n > draw_st.entries_max) draw_st.entries_max = EW.ent_n;
}

/* smooth motion, at the frame's first VBlank: its main list (sprite RAM 0, as built) is kept for draw_vbl_irq and
   becomes the midpoint list (the records of draw.c's runs pointed MID_OFF lower); 0 when it has more than MREC_MAX
   records (the frame's own list is shown) */
#define MREC_MAX 32
#define PPU_STATUS (*(volatile uint16_t *)0x240c000cu)   /* the uncached PPU status (cps3v.c R16) */
static uint32_t frec[MREC_MAX + 1][4];
static int frec_n, irq_vx, irq_vy, irq_nmaps;
static uint8_t pres_mid;
/* irq_due hands frec / frec_n / irq_* to draw_vbl_irq: they are plain, so a compiler barrier orders their writes
   before irq_due = 1 (main loop) and their reads after the test of irq_due (interrupt) */
static volatile uint8_t irq_due;
#define DRAW_BARRIER() __asm__ volatile("" ::: "memory")
static int mid_present(void)
{
    spr_word *s = SPR_AT(0);
    int r, k, j;
    for (r = 0; r < MREC_MAX && !(s[4 * r] & 0x80000000u); r++) ;
    if (r == MREC_MAX) return 0;
    for (k = 0; k <= r; k++)
        for (j = 0; j < 4; j++) frec[k][j] = s[4 * k + j];
    for (k = 0; k < r; k++) {
        uint32_t w0 = frec[k][0], a = (w0 & 0x7fffu) << 4;
        if (a >= RUN_AREA && a < RUN_AREA + 2 * RUN_SIZE) s[4 * k] = (w0 & ~0x7fffu) | ((a - MID_OFF) >> 4);
    }
    frec_n = r;
    irq_vx = vx;
    irq_vy = vy;
    irq_nmaps = nmaps;
    pres_mid = 1;
    return 1;
}

/* the list DMA: cps3v_vblank's register writes (Red Earth's sequence: the 8 global scrolls 0, 8 / 9 four times to
   0x82), without its wait for the copy (status 0x0c bit 0) and its last write (0 to 0x82): draw_list_sync does those
   at the next point that needs the copy done (the next draw's main list, the next VBlank's work), by then long
   ended, so the CPU does not wait for the DMA. The DMAC's entry transfers end first (draw_sprdma_sync). */
#define PPU_REG(o)   (*(volatile uint16_t *)(0x040c0000u + (o)))
static volatile uint8_t list_open;                /* the main loop and draw_vbl_irq both sync */
void draw_list_sync(void)
{
#ifndef DRAW_HOST
    int t;
    if (!list_open) return;
    for (t = 0; t < 10000 && (PPU_STATUS & 1); t++) ;
    PPU_REG(0x82) = 0;
    list_open = 0;
#endif
}
void draw_list_send(void)
{
#ifndef DRAW_HOST
    int k;
    draw_list_sync();
    draw_sprdma_sync();
    for (k = 0; k < 16; k++) PPU_REG(2 * k) = 0;
    for (k = 0; k < 4; k++) {
        PPU_REG(0x82) = 8;
        PPU_REG(0x82) = 9;
    }
    list_open = 1;
#endif
}

void draw_vblank(void)
{
    int m, r, c, k;
    uint32_t cells = 0;
    draw_list_sync();
    if (built_rooms < 0 || !frame_pending) return;
    frame_pending = 0;
    if (bg_dirty && bg_spr >= 0) {                /* the background sprite's 4 x 4 cells over the whole map */
        uint16_t bg[4][4];
        const struct sprdef *sd = &sprdefs[bg_spr];
        const struct framedef *fd = &framedefs[sd->frame];
        int p, mw = (sd->w >> 4) - 1, mh = (sd->h >> 4) - 1;   /* the period in cells: 16, 32 or 64 px */
        for (r = 0; r < 4; r++)
            for (c = 0; c < 4; c++) bg[c][r] = BLANK;
        for (p = fd->piece; p < fd->piece + fd->npieces; p++) {
            const struct piecedef *pc = &piecedefs[p];
            int i, j;
            for (i = 0; i < pc->w; i++)
                for (j = 0; j < pc->h; j++)
                    bg[(((pc->dx + sd->xorig) >> 4) + i) & mw][(((pc->dy + sd->yorig) >> 4) + j) & mh] =
                        (uint16_t)(pc->tile + i * pc->h + j);
        }
        for (r = 0; r < 64; r++)
            for (c = 0; c < 64; c++) cps3v_cell(UNIT(0), c, r, bg[c & mw][r & mh], DRAW_PAL, 0);
        bg_shown = bg_spr;
        bg_dirty = 0;
        cells += 4096;
    }
    if (!maps_cleared) {                          /* cells outside the room: empty */
        for (m = 0; m < NMAPS; m++)
            for (r = 0; r < 64; r++)
                for (c = 0; c < 64; c++)
                    if (m >= nmaps || c >= cols || r >= rows) cps3v_cell(UNIT(1 + m), c, r, BLANK, DRAW_PAL, 0);
        maps_cleared = 1;
    }
    if (rows > 64 || cols > 64)                   /* two room cells to a tilemap cell: the cells on screen */
        for (m = 0; m < nmaps; m++)
            for (r = wr0; r <= wr1; r++) {
                uint16_t *w = &mwant[m][r * cols], *s = &mshown[m][r * cols];
                for (c = wc0; c <= wc1; c++)
                    if (w[c] != s[c]) {
                        s[c] = w[c];
                        cps3v_cell(UNIT(1 + m), c, r, w[c] ? w[c] : BLANK, DRAW_PAL, 0);
                        cells++;
                    }
            }
    else if (mfull) {                             /* every room cell */
        for (m = 0; m < nmaps; m++)
            for (r = 0; r < rows; r++) {
                uint16_t *w = &mwant[m][r * cols], *s = &mshown[m][r * cols];
                for (c = 0; c < cols; c++)
                    if (w[c] != s[c]) {
                        s[c] = w[c];
                        cps3v_cell(UNIT(1 + m), c, r, w[c] ? w[c] : BLANK, DRAW_PAL, 0);
                        cells++;
                    }
            }
    } else
        for (k = 0; k < mqn; k++) {               /* the cells changed since the last VBlank */
            int q = mq[k], cc;
            m = q / MAPC_MAX;
            cc = q - m * MAPC_MAX;
            if (m < nmaps && cc < ncells && mwant[m][cc] != mshown[m][cc]) {
                mshown[m][cc] = mwant[m][cc];
                cps3v_cell(UNIT(1 + m), cc % cols, cc / cols, mwant[m][cc] ? mwant[m][cc] : BLANK, DRAW_PAL, 0);
                cells++;
            }
        }
    for (k = 0; k < mqn; k++) mqueued[mq[k] / MAPC_MAX][mq[k] % MAPC_MAX] = 0;
    mqn = 0;
    mfull = 0;
    draw_st.cells = cells;
    if (frame_a8 != shown_a8) {                   /* the faded palette (tools/darkfade.py table) */
        cps3dma_palette(DARK_FADE_AT + 512u * (uint32_t)frame_a8, DRAW_PAL * 256, 256, 0);
        cps3dma_palette(DARK_FADE_HUD_AT + 512u * (uint32_t)frame_a8, DRAW_PAL_HUDDARK * 256, 256, 0);
        cps3dma_palette(DARK_FADE_HUDY_AT + 512u * (uint32_t)frame_a8, DRAW_PAL_HUDDARK_YELLOW * 256, 256, 0);
        shown_a8 = frame_a8;
    }
    {
        int sx = vx, sy = vy;
        if (frame_mid && mid_present()) {
            sx = vx + EW.ocx;
            sy = vy + EW.ocy;
        }
        cps3v_tilemap(0, sx, sy + DRAW_CROP, UNIT(0), 1);
        for (m = 0; m < NMAPS; m++) cps3v_tilemap(1 + m, sx, sy + DRAW_CROP, UNIT(1 + m), m < nmaps);
    }
}

void draw_vblank_end(void)
{
    if (!pres_mid) return;
    pres_mid = 0;
    DRAW_BARRIER();
    irq_due = 1;
}

void draw_irq_off(void) { irq_due = 0; }

/* the frame's own list at the VBlank after its midpoint list. The main list (sprite RAM 0) may be half built by the
   next draw (this runs from the VBlank interrupt): the records it overwrites are put back once the list DMA has
   copied them (MAME: at the 8 / 9 writes; jtcps3: its busy bit may come up after cps3v_vblank's wait has looked, as
   the character DMA's does (cps3-testgame docs/CPS3.md), so it is waited for, then for its end) */
void draw_vbl_irq(void)
{
#ifndef DRAW_HOST
    uint32_t save[MREC_MAX + 1][4];
    spr_word *s = SPR_AT(0);
    int k, j, n, t;
    if (!irq_due) return;
    irq_due = 0;
    DRAW_BARRIER();
    draw_list_sync();                             /* (the step frame's list: long copied) */
    n = frec_n + 1;
    for (k = 0; k < n; k++)
        for (j = 0; j < 4; j++) {
            save[k][j] = s[4 * k + j];
            s[4 * k + j] = frec[k][j];
        }
    cps3v_tilemap(0, irq_vx, irq_vy + DRAW_CROP, UNIT(0), 1);
    for (k = 0; k < NMAPS; k++) cps3v_tilemap(1 + k, irq_vx, irq_vy + DRAW_CROP, UNIT(1 + k), k < irq_nmaps);
    cps3v_vblank();
    for (t = 0; t < 128 && !(PPU_STATUS & 1); t++) ;
    for (t = 0; t < 10000 && (PPU_STATUS & 1); t++) ;
    for (k = 0; k < n; k++)
        for (j = 0; j < 4; j++) s[4 * k + j] = save[k][j];
#endif
}

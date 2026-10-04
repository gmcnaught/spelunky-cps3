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

/* ---- the play state's instance fields: every read of struct pin goes through these (src/game splits the struct:
   the GML variables of struct pin_ext, through pin's ext; only this block follows such changes) ---------------- */
#define I_ALIVE(i)   (PW.in[i].alive)
#define I_VISIBLE(i) (PW.in[i].visible)
#define I_OBJ(i)     (PW.in[i].obj)
#define I_ID(i)      (PW.in[i].id)
#define I_SPR(i)     (PW.in[i].spr)
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
#define S_DARKNESS   (PLEV.darkness)              /* oLevel.darkness: src/game does not compute it yet (0) */

/* ---- float helpers (no soft-float) ------------------------------------------------------------------------ */
static inline uint32_t fbits(float f) { union { float f; uint32_t u; } c; c.f = f; return c.u; }
/* a key that orders as the float does (NaN aside) */
static inline uint32_t fkey(float f) { uint32_t u = fbits(f); return (u & 0x80000000u) ? ~u : (u | 0x80000000u); }
static inline uint32_t dr_hi(double d)
{
    union { double d; uint32_t w[2]; } c;
    c.d = d;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return c.w[1];
#else
    return c.w[0];
#endif
}
static inline uint32_t dr_lo(double d)
{
    union { double d; uint32_t w[2]; } c;
    c.d = d;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return c.w[0];
#else
    return c.w[1];
#endif
}
#define D_ONE_HI  0x3ff00000u
#define D_MONE_HI 0xbff00000u
static inline int scale_is_pm1(double d) { uint32_t h = dr_hi(d); return dr_lo(d) == 0 && (h == D_ONE_HI || h == D_MONE_HI); }
static inline int dr_neg(double d) { return (dr_hi(d) & 0x80000000u) != 0; }
static inline int dr_zero(double d) { return (dr_hi(d) & 0x7fffffffu) == 0 && dr_lo(d) == 0; }
/* a float that holds a whole number: its integer (truncation otherwise; the soft-float conversion) */
/* (int32_t)f, truncation toward 0, by integer operations (no soft-float call) */
static int32_t ftoi(float f)
{
    uint32_t u = fbits(f), m = (u & 0x7fffffu) | 0x800000u;
    int e = (int)((u >> 23) & 255) - 127;
    int32_t v;
    if (e < 0) return 0;
    v = e >= 23 ? (int32_t)(m << (e - 23 > 7 ? 7 : e - 23)) : (int32_t)(m >> (23 - e));
    return (u & 0x80000000u) ? -v : v;
}
/* the pixel a sprite at float coordinate f starts on: GameMaker's quad covers the pixels whose centre is at or after
   f, i.e. ceil(f - 0.5) (round half down; build/trace/g_p7_dark_s18: oFlareSpark at y 112.8 on row 113). Integer
   operations only */
static int32_t fpix(float f)
{
    uint32_t u = fbits(f), m = (u & 0x7fffffu) | 0x800000u, frac, half;
    int e = (int)((u >> 23) & 255) - 127, sh;
    int32_t ip;
    if (e < -1) return 0;                         /* |f| < 0.5 */
    if (e >= 23) ip = (int32_t)(m << (e - 23 > 7 ? 7 : e - 23));
    else {
        sh = 23 - e;                              /* 1 .. 24 */
        ip = (int32_t)(m >> sh);
        frac = m & ((1u << sh) - 1);
        half = 1u << (sh - 1);
        if (u & 0x80000000u) return -(ip + (frac >= half));
        return ip + (frac > half);
    }
    return (u & 0x80000000u) ? -ip : ip;
}

/* ---- the frame's state ------------------------------------------------------------------------------------ */
static int vx, vy;                                /* the view's top-left (room pixels) */
static int ox, oy;                                /* screen offset: sprite at room (x, y) is at (x - ox, y - oy) */
static uint32_t ent_n;                            /* sublist entries this frame */
/* dark levels: oLevel's black rectangle at alpha oLevel.darkness (objects/oLevel/Draw_0.gml) as a fade of colour
   code DRAW_PAL (tools/darkfade.py: the palette faded at alpha byte a8, by palette DMA at VBlank); what is drawn
   after oLevel (depth below -2, and the HUD) uses DRAW_PAL_LIT, an unfaded copy */
static uint32_t cur_pal = DRAW_PAL;
static int frame_a8, shown_a8;
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
static uint32_t mframe[NMAPS][MAPC_MAX / 32 + 1];            /* want cell taken by an instance this frame */
static int16_t mown[NMAPS][MAPC_MAX] DRAW_MAPS_SECTION;     /* that instance */
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
static uint8_t bg_dirty, maps_cleared, frame_pending;
static int32_t built_rooms = -1;                  /* play_rooms_entered at the last tilemap build */
static int built_room = -1;

/* tile_add tiles not on a tilemap: single-tile sprites */
struct tspr { uint32_t dkey; int16_t seq; uint16_t tile; int16_t x, y; };
static struct tspr tspr[TSPR_MAX] DRAW_MAPS_SECTION;
static int ntspr;
static uint32_t tdel[GTILES_MAX / 32 + 1];        /* tile_delete'd gtiles */
static uint8_t tiles_dirty;

/* the frame's drawables: instances (i >= 0) and tile sprites (i = -1 - k) */
struct ent { uint32_t dkey; int32_t id; int16_t i; };
static struct ent ents[ENT_MAX];
static uint16_t ord[ENT_MAX];                     /* ents in drawing order */

/* ---- display list helpers --------------------------------------------------------------------------------- */
static void piece_out(int px, int py, const struct piecedef *pc, int flip)
{
    if (ent_n >= DRAW_ENTRIES_MAX) {
        draw_st.dropped++;
        return;
    }
    cps3v_sprite(px, py, pc->w, pc->h, pc->tile, cur_pal, flip ? CPS3V_FLIPX : 0);
    ent_n++;
}

/* frame f (framedefs) with its origin at screen (x, y); flip: mirrored about x (image_xscale -1) */
static void frame_out(int f, int x, int y, int flip)
{
    const struct framedef *fd = &framedefs[f];
    const struct piecedef *pc = &piecedefs[fd->piece], *end = pc + fd->npieces;
    for (; pc < end; pc++) {
        int w = 16 * pc->w, px = flip ? x - pc->dx - w : x + pc->dx, py = y + pc->dy;
        if (px >= VIEW_W || py >= SCREEN_H || px + w <= 0 || py + 16 * pc->h <= 0)
            continue;
        piece_out(px, py, pc, flip);
    }
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
    frame_out(sd->frame + img, x - ox, y - oy, flip);
}

/* global.sSmallCollectNew frame k (tools/hudart.py: 8 x 10 at a tile's top-left, origin 4, 4) at room (x, y) */
static void collect_out(int k, int x, int y)
{
    int px = x - HUD_COLLECT_XORIG - ox, py = y - HUD_COLLECT_YORIG - oy;
    if (px >= VIEW_W || py >= SCREEN_H || px <= -16 || py <= -16) return;
    if (ent_n >= DRAW_ENTRIES_MAX) {
        draw_st.dropped++;
        return;
    }
    cps3v_sprite(px, py, 1, 1, HUD_TILE_COLLECT(k), cur_pal == DRAW_PAL ? DRAW_PAL_HUDDARK : HUD_PAL, 0);
    ent_n++;
}

static void band_out(int tm)
{
    if (ent_n >= DRAW_ENTRIES_MAX) {
        draw_st.dropped++;
        return;
    }
    cps3v_group();
    cps3v_band(tm, 0, CPS3V_H);
    cps3v_group();
    ent_n += 2;                                   /* 224 lines: two band entries of at most 128 lines */
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
    if (dr_hi(I_XSCALE(pi)) != D_ONE_HI || dr_lo(I_XSCALE(pi)) || dr_hi(I_YSCALE(pi)) != D_ONE_HI || dr_lo(I_YSCALE(pi)) ||
        !dr_zero(I_ANGLE(pi)))
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

/* terrain_cell and map_of_depth of an instance, remembered per slot with the fields they read (x, y, sprite,
   image, depth, scales, angle as bit patterns): a solid that did not change costs a few compares */
/* the transform terrain cells and the cached draws need: image_xscale, image_yscale 1 (xscale -1 allowed for draws,
   *flip), image_angle 0, by bit patterns */
static inline __attribute__((always_inline)) int plain_transform(int pi, int *flip)
{
    uint32_t xh = dr_hi(I_XSCALE(pi));
    if (dr_lo(I_XSCALE(pi)) || (xh != D_ONE_HI && xh != D_MONE_HI) || dr_hi(I_YSCALE(pi)) != D_ONE_HI ||
        dr_lo(I_YSCALE(pi)) || !dr_zero(I_ANGLE(pi)))
        return 0;
    *flip = xh == D_MONE_HI;
    return 1;
}

/* per slot, the result of a computation from fields that rarely change, with those fields as bit patterns
   (x, y, image_index, depth, sprite; the transform checked as plain_transform each time):
   kind 1: terrain_cell + map_of_depth (cx, cy, c, tile, m);  kind 2: a draw_self frame (cx, cy = room position,
   c = framedefs index, m = flip, tile = 1 when the sprite has art) */
struct tcache { uint32_t xb, yb, ib, db; int16_t spr, cx, cy, c; uint16_t tile; int8_t m; };
static struct tcache tcache[PIN_MAX] DRAW_CACHE_SECTION;
static uint8_t tcache_ok[PIN_MAX];                /* the kind cached (0 none) */
static uint16_t terrain_cached(int pi, int *c, int *m)
{
    struct tcache *e = &tcache[pi];
    uint32_t xb = fbits(I_X(pi)), yb = fbits(I_Y(pi)), ib = fbits(I_IMG(pi)), db = fbits(I_DEPTH(pi));
    int flip;
    if (!plain_transform(pi, &flip) || flip) return 0;
    if (tcache_ok[pi] != 1 || e->xb != xb || e->yb != yb || e->spr != I_SPR(pi) || e->ib != ib || e->db != db) {
        int cc = 0;
        e->xb = xb; e->yb = yb; e->ib = ib; e->db = db;
        e->spr = I_SPR(pi);
        e->tile = terrain_cell(pi, &cc);
        e->m = (int8_t)(e->tile ? map_of_depth(I_DEPTH(pi)) : -1);
        e->c = (int16_t)cc;
        e->cx = (int16_t)t_cx;
        e->cy = (int16_t)t_cy;
        tcache_ok[pi] = 1;
    }
    if (!e->tile || e->m < 0) return 0;
    *c = e->c;
    *m = e->m;
    t_cx = e->cx;
    t_cy = e->cy;
    return e->tile;
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
    bg_spr = (PW.room == R_rOlmec || G.levelType == 3) ? SPR_bgTemple : SPR_bgCave;
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
    for (k = 0; k < GTILES_MAX / 32 + 1; k++) tdel[k] = 0;
    build_tiles();
    maps_cleared = 0;
    built_rooms = play_rooms_entered;
    built_room = PW.room;
    for (k = 0; k < PIN_MAX; k++) tcache_ok[k] = 0;
    draw_st.room_builds++;
}

void draw_tile_delete(int depth, int x, int y)
{
    int k;
    for (k = gntiles - 1; k >= 0; k--) {          /* layer_get_all_elements: newest first */
        const struct gtile *t = &gtiles[k];
        if (t->depth != depth || (tdel[k >> 5] >> (k & 31) & 1)) continue;
        if (x >= t->x && x < t->x + t->w && y >= t->y && y < t->y + t->h) {
            tdel[k >> 5] |= 1u << (k & 31);
            tiles_dirty = 1;
            return;
        }
    }
}

/* ---- instances --------------------------------------------------------------------------------------------- */
/* spr_local[GSPR_*]: the sprite has art and draws within 16 px of its origin (any frame, mirrored or not);
   dk_local[kind]: the Draw event draws only the sprite about its origin (and the price tag at y - 16 .. y - 7) */
static uint8_t spr_local[GSPR_COUNT];
static const uint8_t dk_local[DK_TODO + 1] = { [DK_SELF] = 1, [DK_DAMSEL] = 1, [DK_ITEM] = 1, [DK_PLAIN] = 1,
                                               [DK_TODO] = 1 };

static int32_t img_of(int pi) { return ftoi(I_IMG(pi)); }
static int is_exit_spr(int s) { return s == GSPR_sPExit || s == GSPR_sDamselExit || s == GSPR_sTunnelExit; }

/* draw_self: image_xscale mirrors; other transforms counted */
/* draw_self (flip = image_xscale -1) or draw_sprite(sprite_index, -1, x, y) (flip 0) of instance pi, through the
   slot cache (kind 2) when the transform is plain */
static void cached_out(int pi, int mirror)
{
    struct tcache *e = &tcache[pi];
    uint32_t xb = fbits(I_X(pi)), yb = fbits(I_Y(pi)), ib = fbits(I_IMG(pi));
    int flip, s;
    if (!plain_transform(pi, &flip)) {
        if (mirror) draw_st.unsup++;
        s = draw_spr[I_SPR(pi)];
        spr_out(s, img_of(pi), fpix(I_X(pi)), fpix(I_Y(pi)), mirror && dr_neg(I_XSCALE(pi)));
        return;
    }
    flip &= mirror;
    if (tcache_ok[pi] != 2 || e->xb != xb || e->yb != yb || e->ib != ib || e->spr != I_SPR(pi) || e->m != flip) {
        int32_t img = img_of(pi);
        e->xb = xb; e->yb = yb; e->ib = ib;
        e->spr = I_SPR(pi);
        e->m = (int8_t)flip;
        e->cx = (int16_t)fpix(I_X(pi));
        e->cy = (int16_t)fpix(I_Y(pi));
        s = draw_spr[I_SPR(pi)];
        e->tile = s >= 0;
        if (s >= 0) {
            const struct sprdef *sd = &sprdefs[s];
            if (img < 0) img = 0;
            if ((uint32_t)img >= sd->nframes) img = (int32_t)((uint32_t)img % sd->nframes);
            e->c = (int16_t)(sd->frame + img);
        }
        tcache_ok[pi] = 2;
    }
    if (!e->tile) {
        draw_st.noart++;
        return;
    }
    frame_out(e->c, e->cx - ox, e->cy - oy, e->m);
}
static void self_out(int pi, int x, int y)
{
    if (I_SPR(pi) < 0) return;
    if (!scale_is_pm1(I_XSCALE(pi)) || dr_hi(I_YSCALE(pi)) != D_ONE_HI || dr_lo(I_YSCALE(pi)) || !dr_zero(I_ANGLE(pi)))
        draw_st.unsup++;
    spr_out(draw_spr[I_SPR(pi)], img_of(pi), x, y, dr_neg(I_XSCALE(pi)));
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

static void inst_out(int i)
{
    int pi = i, dk = draw_kind[I_OBJ(pi)] & ~DK_SOLID, x = 0, y = 0;
    if (dk != DK_SELF && dk != DK_PLAIN && dk != DK_ITEM && dk != DK_NONE) {
        x = fpix(I_X(pi));
        y = fpix(I_Y(pi));
    }
    switch (dk) {
    case DK_NONE: break;
    case DK_TODO: draw_st.todo++; self_out(pi, x, y); break;
    case DK_SELF: if (I_SPR(pi) >= 0) cached_out(pi, 1); break;
    case DK_DAMSEL:                               /* objects/oDamsel/Draw_0.gml: the price tag at cimg, which */
        self_out(pi, x, y);                        /* the play code's ev_draw has counted on already */
        if (I_COST(pi) > 0) collect_out(I_CIMG(pi) ? I_CIMG(pi) - 1 : 9, x, y - 12);
        break;
    case DK_ITEM:                                 /* objects/oItem/Draw_0.gml (cimg counted in draw_frame) */
        if (I_SPR(pi) >= 0) cached_out(pi, 0);
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
    case DK_PLAIN: if (I_SPR(pi) >= 0) cached_out(pi, 0); break;
    case DK_DICE:                                 /* objects/oDice/Draw_0.gml */
        self_out(pi, x, y);
        if (!I_ROLLED(pi) && PL.bet > 0) spr_out(SPR_sRedArrowDown, 0, x, y - 12, 0);
        break;
    case DK_PDUMMY: pdummy_out(pi, x, y); break;
    case DK_JAWS: jaws_out(pi, x, y); break;
    case DK_PLAYER: player_out(pi, x, y); break;
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

/* oTransition's drawLoot, moneyCount, isLoot, isKills (ptrans.c keeps them in a static struct): src/game is to
   provide this accessor; until then the transition's text is not drawn (returns 0) */
__attribute__((weak)) int ptrans_gui(int32_t *v) { (void)v; return 0; }

static char *cat(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *catn(char *d, int32_t n) { char b[12]; return cat(d, hud_itoa(n, b)); }

/* objects/oTransition/Draw_64.gml (English, room_offset 0): the level's end screen */
static void transition_out(void)
{
    int32_t t[4];
    char b[48], *e;
    int32_t s, s2, k;
    if (PW.room < R_rTransition1 || PW.room > R_rTransition4) return;
    for (k = 0; k < PW.n && !(I_ALIVE(k) && I_OBJ(k) == OBJ_oTransition); k++) ;
    if (k == PW.n || !ptrans_gui(t)) return;
    for (k = 0; k < PW.n; k++)                    /* oDamselKiss.kissed: "MY HERO!" */
        if (I_ALIVE(k) && I_OBJ(k) == OBJ_oDamselKiss) {
            if (I_TRIGGER(k)) hud_text_centered("MY HERO!", HUD_FONT_SMALL, 0, 0, 216);
            break;
        }
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
        e = catn(b, s / 60);
        e = cat(e, s % 60 < 10 ? ":0" : ":");
        e = catn(e, s % 60);
        e = cat(e, " / ");
        e = catn(e, s2 / 60);
        e = cat(e, s2 % 60 < 10 ? ":0" : ":");
        catn(e, s2 % 60);
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

/* scrDrawHUD / showMessages' state from the play state (docs/ARCADE.md §4) */
static void hud_out(void)
{
    static struct hud_state h;                    /* messages stay empty: the play code keeps no messages yet */
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
    h.blood_level = 0;                            /* global.bloodLevel: not in the play state yet */
    h.arrows = PG.arrows;
    for (k = 0; k < PW.n; k++)                    /* oGame.image_index */
        if (I_ALIVE(k) && I_OBJ(k) == OBJ_oGame) { game = k; break; }
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
    h.message_timer = 0;
    hud_draw(&h, DRAW_PAL_LIT);
}

/* ---- the frame ---------------------------------------------------------------------------------------------- */
static inline int ent_before(const struct ent *a, const struct ent *b)
{
    return a->dkey > b->dkey || (a->dkey == b->dkey && a->id > b->id);
}

void draw_new_game(void) { built_rooms = -1; }

void draw_boot(void)
{
    int m, k;
    for (k = 0; k < GSPR_COUNT; k++) {
        int sp = draw_spr[k];
        const struct sprdef *sd = sp >= 0 ? &sprdefs[sp] : 0;
        spr_local[k] = sd && sd->w <= 16 && sd->h <= 16 && sd->xorig >= 0 && sd->xorig <= 16 && sd->yorig >= 0 &&
                       sd->yorig <= 16;
    }
    for (m = 0; m <= NMAPS; m++) cps3v_tilemap(m, 0, 0, UNIT(m), 0);
    bg_shown = -1;
    built_rooms = -1;
}

void draw_frame(void)
{
    int k, m, n = 0, band = 0;
    uint32_t xlo, xhi, ylo, yhi;
    PROF0();
    draw_st.frames++;
    draw_st.todo = draw_st.unsup = draw_st.noart = 0;
    ent_n = 0;
    if (built_rooms != play_rooms_entered || built_room != PW.room) build_room();
    if (tiles_dirty) build_tiles();
    vx = PW.xview;
    vy = PW.yview;
    ox = vx;
    oy = vy + DRAW_CROP;
    ZOOM_X = DRAW_ZOOM_X;
    /* tilemaps: base + terrain in the screen's cells; first claimant of a cell by creation order keeps it, the
       others are sprites */
    wc0 = vx >> 4;
    wc1 = (vx + VIEW_W - 1) >> 4;
    wr0 = (vy + DRAW_CROP) >> 4;
    wr1 = (vy + DRAW_CROP + SCREEN_H - 1) >> 4;
    if (wc1 >= cols) wc1 = cols - 1;
    if (wr1 >= rows) wr1 = rows - 1;
    for (m = 0; m < nmaps; m++) {
        int r, c;
        for (r = wr0; r <= wr1; r++) {
            uint16_t *w = &mwant[m][r * cols], *b = &mbase[m][r * cols];
            for (c = wc0; c <= wc1; c++) w[c] = b[c];
        }
        for (k = 0; k < MAPC_MAX / 32 + 1; k++) mframe[m][k] = 0;
    }
    PROF(0);
    /* coarse view test on the float bits: x in [vx - 320, vx + 640), y in [vy - 240, vy + 480); terrain only in the
       screen's cells */
    xlo = fkey((float)(vx - 320));
    xhi = fkey((float)(vx + 640));
    ylo = fkey((float)(vy - 240));
    yhi = fkey((float)(vy + 480));
    {
    uint32_t txlo = fkey((float)(16 * wc0)), txhi = fkey((float)(16 * wc1 + 16));
    uint32_t tylo = fkey((float)(16 * wr0)), tyhi = fkey((float)(16 * wr1 + 16));
    /* sprites of at most 16 x 16 with the origin inside them draw within x - 16 .. x + 16: in view only for x in
       (vx - 16, vx + 336), y in (vy + 8 - 16, vy + 248) */
    uint32_t sxlo = fkey((float)(vx - 16)), sxhi = fkey((float)(vx + VIEW_W + 16));
    uint32_t sylo = fkey((float)(vy + DRAW_CROP - 16)), syhi = fkey((float)(vy + DRAW_CROP + SCREEN_H + 16));
    for (k = 0; k < ntspr; k++) {                 /* tile sprites in view (first: their ids are the largest) */
        const struct tspr *t = &tspr[k];
        if (t->x <= vx - 16 || t->x >= vx + VIEW_W || t->y <= vy - 16 || t->y >= vy + VIEW_H) continue;
        if (n < ENT_MAX) {
            ents[n].dkey = t->dkey;
            ents[n].id = 0x70000000 - (int32_t)t->seq;   /* before every instance of the depth */
            ents[n].i = (int16_t)(-1 - k);
            n++;
        }
    }
    for (k = PW.n - 1; k >= 0; k--) {             /* newest first: the sort below then moves little */
        int pi = k;
        uint32_t kx, ky;
        int c;
        uint16_t t;
        uint8_t dk;
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
        if (kx <= sxlo || kx >= sxhi || ky <= sylo || ky >= syhi) {   /* not within 16 px of the screen: */
            if (I_SPR(pi) < 0 || (spr_local[I_SPR(pi)] && dk_local[dk & ~DK_SOLID])) continue;   /* draws nothing */
            if (kx < xlo || kx >= xhi || ky < ylo || ky >= yhi) continue;
        }
        if ((dk & DK_SOLID) && nmaps && kx >= txlo && kx < txhi && ky >= tylo && ky < tyhi &&
            (t = terrain_cached(pi, &c, &m)) != 0 && t_cx >= wc0 && t_cx <= wc1 && t_cy >= wr0 && t_cy <= wr1) {
            uint32_t bit = 1u << (c & 31), *fw = &mframe[m][c >> 5];
            if (!mbase[m][c] && !(*fw & bit)) {
                mwant[m][c] = t;
                mown[m][c] = (int16_t)k;
                *fw |= bit;
                continue;
            }
            if (!mbase[m][c] && I_ID(mown[m][c]) < I_ID(pi)) {   /* the newer one is drawn first: it takes the */
                int o = mown[m][c];                               /* cell, the older one is drawn over it */
                mwant[m][c] = t;
                mown[m][c] = (int16_t)k;
                if (n < ENT_MAX) {
                    ents[n].dkey = fkey(I_DEPTH(o));
                    ents[n].id = I_ID(o);
                    ents[n].i = (int16_t)o;
                    n++;
                }
                continue;
            }
            /* taken by a tile, or by a newer instance: a sprite */
        }
        if (n < ENT_MAX) {
            ents[n].dkey = fkey(I_DEPTH(pi));
            ents[n].id = I_ID(pi);
            ents[n].i = (int16_t)k;
            n++;
        }
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
        int nk = 0, j;
        for (k = 0; k < n; k++) {
            uint32_t d = ents[k].dkey;
            for (j = 0; j < nk && keys[j] != d; j++) ;
            if (j == nk) {
                if (nk == 32) { nk = -1; break; }     /* more than 32 depths: insertion sort alone */
                keys[nk++] = d;
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
            for (lvl = 0; lvl < PW.n && !(I_ALIVE(lvl) && I_OBJ(lvl) == OBJ_oLevel); lvl++) ;
        frame_a8 = 0;
        if (lvl < PW.n && (draw_dark_force >= 0 || S_DARKLEVEL)) {
            double d = S_DARKNESS;
            dark = 1;
            frame_a8 = draw_dark_force >= 0 ? draw_dark_force : d <= 0 ? 0 : d >= 1 ? 255 : (int)(d * 255.0);
            lkey = fkey(I_DEPTH(lvl));
            lid = I_ID(lvl);
        }
        cur_pal = DRAW_PAL;
    /* the list: background, then bands and drawables by depth */
    band_out(0);
    for (k = 0; k < n; k++) {
        const struct ent *e = &ents[ord[k]];
        if (dark && cur_pal == DRAW_PAL && (e->dkey < lkey || (e->dkey == lkey && e->id < lid)))
            cur_pal = DRAW_PAL_LIT;               /* after oLevel's rectangle */
        while (band < nmaps && mdepth_key[band] >= e->dkey) band_out(1 + band++);
        if (e->i >= 0) inst_out(e->i);
        else {
            const struct tspr *t = &tspr[-1 - e->i];
            int px = t->x - ox, py = t->y - oy;
            struct piecedef pc = { 0, 0, 1, 1, t->tile };
            if (px > -16 && px < VIEW_W && py > -16 && py < SCREEN_H) piece_out(px, py, &pc, 0);
        }
    }
    while (band < nmaps) band_out(1 + band++);
    }
    PROF(3);
    if (draw_hud_on) {
        hud_out();
        transition_out();
    }
    PROF(4);
    draw_st.entries = ent_n;
    frame_pending = 1;
    if (ent_n > draw_st.entries_max) draw_st.entries_max = ent_n;
}

void draw_vblank(void)
{
    int m, r, c;
    uint32_t cells = 0;
    if (built_rooms < 0 || !frame_pending) return;
    frame_pending = 0;
    if (bg_dirty) {                               /* the background sprite's 4 x 4 cells over the whole map */
        uint16_t bg[4][4];
        const struct sprdef *sd = &sprdefs[bg_spr];
        const struct framedef *fd = &framedefs[sd->frame];
        int p;
        for (r = 0; r < 4; r++)
            for (c = 0; c < 4; c++) bg[c][r] = BLANK;
        for (p = fd->piece; p < fd->piece + fd->npieces; p++) {
            const struct piecedef *pc = &piecedefs[p];
            int i, j;
            for (i = 0; i < pc->w; i++)
                for (j = 0; j < pc->h; j++)
                    bg[(((pc->dx + sd->xorig) >> 4) + i) & 3][(((pc->dy + sd->yorig) >> 4) + j) & 3] =
                        (uint16_t)(pc->tile + i * pc->h + j);
        }
        for (r = 0; r < 64; r++)
            for (c = 0; c < 64; c++) cps3v_cell(UNIT(0), c, r, bg[c & 3][r & 3], DRAW_PAL, 0);
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
    draw_st.cells = cells;
    if (frame_a8 != shown_a8) {                   /* the faded palette (tools/darkfade.py table) */
        cps3dma_palette(DARK_FADE_AT + 512u * (uint32_t)frame_a8, DRAW_PAL * 256, 256, 0);
        cps3dma_palette(DARK_FADE_HUD_AT + 512u * (uint32_t)frame_a8, DRAW_PAL_HUDDARK * 256, 256, 0);
        shown_a8 = frame_a8;
    }
    cps3v_tilemap(0, vx, vy + DRAW_CROP, UNIT(0), 1);
    for (m = 0; m < NMAPS; m++) cps3v_tilemap(1 + m, vx, vy + DRAW_CROP, UNIT(1 + m), m < nmaps);
}

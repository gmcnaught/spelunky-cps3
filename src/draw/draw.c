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

/* ---- float helpers (no soft-float) ------------------------------------------------------------------------ */
static inline uint32_t fbits(float f) { union { float f; uint32_t u; } c; c.f = f; return c.u; }
/* a key that orders as the float does (NaN aside) */
static inline uint32_t fkey(float f) { uint32_t u = fbits(f); return (u & 0x80000000u) ? ~u : (u | 0x80000000u); }
static inline uint32_t dhi(double d)
{
    union { double d; uint32_t w[2]; } c;
    c.d = d;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return c.w[1];
#else
    return c.w[0];
#endif
}
static inline uint32_t dlo(double d)
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
static inline int scale_is_pm1(double d) { uint32_t h = dhi(d); return dlo(d) == 0 && (h == D_ONE_HI || h == D_MONE_HI); }
static inline int dneg(double d) { return (dhi(d) & 0x80000000u) != 0; }
static inline int dzero(double d) { return (dhi(d) & 0x7fffffffu) == 0 && dlo(d) == 0; }
/* a float that holds a whole number: its integer (truncation otherwise; the soft-float conversion) */
static inline int32_t ftoi(float f) { return (int32_t)f; }

/* ---- the frame's state ------------------------------------------------------------------------------------ */
static int vx, vy;                                /* the view's top-left (room pixels) */
static int ox, oy;                                /* screen offset: sprite at room (x, y) is at (x - ox, y - oy) */
static uint32_t ent_n;                            /* sublist entries this frame */

/* tilemaps: one depth each; base: the tile_add cells, want: base + terrain this frame, shown: what the tilemap
   holds (written by draw_vblank). Values: tile number, 0 = empty (BLANK). Kept outside main RAM in MAME builds
   (DRAW_MAPS_SECTION, see tests/game/sprbss.ld) */
#ifndef DRAW_MAPS_SECTION
#define DRAW_MAPS_SECTION
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
static struct tspr tspr[TSPR_MAX];
static int ntspr;
static uint32_t tdel[GTILES_MAX / 32 + 1];        /* tile_delete'd gtiles */
static uint8_t tiles_dirty;

/* the frame's drawables: instances (i >= 0) and tile sprites (i = -1 - k) */
struct ent { uint32_t dkey; int32_t id; int16_t i; };
static struct ent ents[ENT_MAX];

/* ---- display list helpers --------------------------------------------------------------------------------- */
static void piece_out(int px, int py, const struct piecedef *pc, int flip)
{
    if (ent_n >= DRAW_ENTRIES_MAX) {
        draw_st.dropped++;
        return;
    }
    cps3v_sprite(px, py, pc->w, pc->h, pc->tile, DRAW_PAL, flip ? CPS3V_FLIPX : 0);
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
    cps3v_sprite(px, py, 1, 1, HUD_TILE_COLLECT(k), HUD_PAL, 0);
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
static uint16_t terrain_cell(const struct pin *p, int *c)
{
    int s, cx, cy;
    const struct sprdef *sd;
    const struct framedef *fd;
    const struct piecedef *pc;
    uint32_t f = 0;
    if (!(draw_kind[p->obj] & DK_SOLID) || p->spr < 0 || (s = draw_spr[p->spr]) < 0) return 0;
    if (dhi(p->xscale) != D_ONE_HI || dlo(p->xscale) || dhi(p->yscale) != D_ONE_HI || dlo(p->yscale) ||
        !dzero(p->angle))
        return 0;
    sd = &sprdefs[s];
    if (sd->nframes > 1) {
        int32_t i = ftoi(p->img);
        f = i < 0 ? 0 : (uint32_t)i % sd->nframes;
    }
    fd = &framedefs[sd->frame + f];
    if (fd->npieces != 1) return 0;
    pc = &piecedefs[fd->piece];
    if (pc->w != 1 || pc->h != 1 || (pc->dx & 15) || (pc->dy & 15)) return 0;
    cx = f16(fbits(p->x));
    cy = f16(fbits(p->y));
    if (cx < 0 || cy < 0) return 0;
    cx += pc->dx >> 4;
    cy += pc->dy >> 4;
    if (cx < 0 || cy < 0 || cx >= cols || cy >= rows) return 0;
    *c = cy * cols + cx;
    t_cx = cx;
    t_cy = cy;
    return pc->tile;
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
        const struct pin *p = &PW.in[k];
        int c, j;
        if (!p->alive || !p->visible || !terrain_cell(p, &c)) continue;
        for (j = 0; j < nd && dep[j] != p->depth; j++) ;
        if (j == nd) {
            if (nd == 24) continue;
            dep[nd] = p->depth;
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
/* the sprite draws within 16 px of its origin (any frame, mirrored or not) */
static int small_spr(int s)
{
    const struct sprdef *sd = &sprdefs[s];
    return sd->w <= 16 && sd->h <= 16 && sd->xorig >= 0 && sd->xorig <= 16 && sd->yorig >= 0 && sd->yorig <= 16;
}

static int32_t img_of(const struct pin *p) { return ftoi(p->img); }
static int is_exit_spr(int s) { return s == GSPR_sPExit || s == GSPR_sDamselExit || s == GSPR_sTunnelExit; }

/* draw_self: image_xscale mirrors; other transforms counted */
static void self_out(const struct pin *p, int x, int y)
{
    if (p->spr < 0) return;
    if (!scale_is_pm1(p->xscale) || dhi(p->yscale) != D_ONE_HI || dlo(p->yscale) || !dzero(p->angle))
        draw_st.unsup++;
    spr_out(draw_spr[p->spr], img_of(p), x, y, dneg(p->xscale));
}
static void plain_out(const struct pin *p, int x, int y)
{
    if (p->spr >= 0) spr_out(draw_spr[p->spr], img_of(p), x, y, 0);
}

/* scripts/characterDrawEvent (oPlayer1's Draw); image_xscale was set by the play code's ev_draw */
static void player_out(const struct pin *p, int x, int y)
{
    int32_t a = img_of(p);
    int drawn = 1;
    if (PL.blinkToggle == 1) return;
    if ((PL.state == CLIMBING || is_exit_spr(p->spr)) && PG.hasJetpack && !PL.whipping) {
        self_out(p, x, y);
        spr_out(SPR_sJetpackBack, a, x, y, 0);
        drawn = 0;
    } else if (PG.hasJetpack && PL.facing == RIGHT)
        spr_out(SPR_sJetpackRight, a, x - 4, y - 1, 0);
    else if (PG.hasJetpack)
        spr_out(SPR_sJetpackLeft, a, x + 4, y - 1, 0);
    if (drawn) {
        if (PL.redColor > 0) draw_st.unsup++;     /* make_color_rgb(200 + redColor, 0, 0) blend: not drawn */
        self_out(p, x, y);
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
static void pdummy_out(const struct pin *p, int x, int y)
{
    static const int16_t held[PICK_COUNT] = {
        [PICK_ROCK] = SPR_sRock, [PICK_JAR] = SPR_sJar, [PICK_SKULL] = SPR_sSkull, [PICK_FISHBONE] = SPR_sFishBone,
        [PICK_ARROW] = SPR_sArrowRight, [PICK_MACHETE] = SPR_sMacheteRight, [PICK_MATTOCK] = SPR_sMattockRight,
        [PICK_MATTOCKHEAD] = SPR_sMattockHead, [PICK_PISTOL] = SPR_sPistolRight, [PICK_WEBCANNON] = SPR_sWebCannonR,
        [PICK_TELEPORTER] = SPR_sTeleporter, [PICK_SHOTGUN] = SPR_sShotgunRight, [PICK_BOW] = SPR_sBowRight,
        [PICK_FLARE] = SPR_sFlare, [PICK_SCEPTRE] = SPR_sSceptreRight, [PICK_KEY] = SPR_sKeyRight,
    };
    int32_t a = img_of(p);
    int ex = is_exit_spr(p->spr);
    if (ex) {
        self_out(p, x, y);
        if (PG.hasJetpack) spr_out(SPR_sJetpackBack, a, x, y, 0);
        return;
    }
    if (PG.hasJetpack) spr_out(SPR_sJetpackRight, a, x - 4, y - 1, 0);
    self_out(p, x, y);
    if (G.pickupItem > PICK_NONE && G.pickupItem < PICK_OTHER && held[G.pickupItem])
        spr_out(held[G.pickupItem], a, x + 4, y + 2, 0);
}

/* objects/oJaws/Draw_0.gml */
static void jaws_out(const struct pin *p, int x, int y)
{
    int b;
    plain_out(p, x, y);
    if (p->spr == GSPR_sJawsLeft) {
        b = p->hp < 10 ? SPR_sJawsBody3L : p->hp < 20 ? SPR_sJawsBody2L : SPR_sJawsBody1L;
        spr_out(b, 0, x + 16, y, 0);
    } else if (p->spr == GSPR_sJawsRight) {
        b = p->hp < 10 ? SPR_sJawsBody3R : p->hp < 20 ? SPR_sJawsBody2R : SPR_sJawsBody1R;
        spr_out(b, 0, x - 48, y, 0);
    }
}

static void inst_out(int i)
{
    const struct pin *p = &PW.in[i];
    int x = ftoi(p->x), y = ftoi(p->y);
    switch (draw_kind[p->obj] & ~DK_SOLID) {
    case DK_NONE: break;
    case DK_TODO: draw_st.todo++; self_out(p, x, y); break;
    case DK_SELF: self_out(p, x, y); break;
    case DK_DAMSEL:                               /* objects/oDamsel/Draw_0.gml: the price tag at cimg, which */
        self_out(p, x, y);                        /* the play code's ev_draw has counted on already */
        if (p->cost > 0) collect_out(p->cimg ? p->cimg - 1 : 9, x, y - 12);
        break;
    case DK_ITEM:                                 /* objects/oItem/Draw_0.gml (cimg counted in draw_frame) */
        plain_out(p, x, y);
        if (p->cost > 0) collect_out(icimg[i] ? icimg[i] - 1 : 9, x, y - 12);
        break;
    case DK_ENEMY:                                /* objects/oEnemy/Draw_0.gml (oEnemy: LEFT 0, RIGHT 1) */
        if (p->spr < 0) break;
        if (p->facing == 1) spr_out(draw_spr[p->spr], img_of(p), x + 16, y, 1);
        else spr_out(draw_spr[p->spr], img_of(p), x, y, 0);
        break;
    case DK_SHOP:                                 /* objects/oShopkeeper/Draw_0.gml (IDLE 0, FOLLOW 5) */
        if (p->spr >= 0) {
            if (p->facing == 1) spr_out(draw_spr[p->spr], img_of(p), x + 16, y, 1);
            else spr_out(draw_spr[p->spr], img_of(p), x, y, 0);
        }
        if (p->hasGun && p->status != 0 && p->status != 5) {
            if (p->facing == 0) spr_out(SPR_sShotgunLeft, 0, x + 6, y + 10, 0);
            else spr_out(SPR_sShotgunRight, 0, x + 10, y + 10, 0);
        }
        break;
    case DK_PLAIN: plain_out(p, x, y); break;
    case DK_DICE:                                 /* objects/oDice/Draw_0.gml */
        self_out(p, x, y);
        if (!p->rolled && PL.bet > 0) spr_out(SPR_sRedArrowDown, 0, x, y - 12, 0);
        break;
    case DK_PDUMMY: pdummy_out(p, x, y); break;
    case DK_JAWS: jaws_out(p, x, y); break;
    case DK_PLAYER: player_out(p, x, y); break;
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

/* scrDrawHUD / showMessages' state from the play state (docs/ARCADE.md §4) */
static void hud_out(void)
{
    static struct hud_state h;                    /* messages stay empty: the play code keeps no messages yet */
    int k, game = -1;
    h.visible = PG.drawHUD && PL.idx != NOONE && PW.in[PL.idx].alive;
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
        if (PW.in[k].alive && PW.in[k].obj == OBJ_oGame) { game = k; break; }
    h.anim = game >= 0 ? ftoi(PW.in[game].img) : 0;
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
    hud_draw(&h, DRAW_PAL);
}

/* ---- the frame ---------------------------------------------------------------------------------------------- */
static inline int ent_before(const struct ent *a, const struct ent *b)
{
    return a->dkey > b->dkey || (a->dkey == b->dkey && a->id > b->id);
}

void draw_new_game(void) { built_rooms = -1; }

void draw_boot(void)
{
    int m;
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
    for (k = PW.n - 1; k >= 0; k--) {             /* newest first: the sort below then moves little */
        const struct pin *p = &PW.in[k];
        uint32_t kx, ky;
        int c;
        uint16_t t;
        uint8_t dk;
        if (!p->alive || !p->visible) continue;
        dk = draw_kind[p->obj];
        if (dk == DK_NONE) continue;
        if (dk == DK_ITEM && p->cost > 0) {       /* oItem Draw: cimg += 1, 0 after 9 */
            if (icid[k] != p->id) {
                icid[k] = p->id;
                icimg[k] = 0;
            }
            icimg[k] = icimg[k] >= 9 ? 0 : icimg[k] + 1;
        }
        kx = fkey(p->x);
        ky = fkey(p->y);
        if (kx < xlo || kx >= xhi || ky < ylo || ky >= yhi) continue;
        if (p->spr >= 0 && (kx <= sxlo || kx >= sxhi || ky <= sylo || ky >= syhi)) {
            int sp = draw_spr[p->spr];
            if (sp >= 0 && small_spr(sp) && (dk & ~DK_SOLID) != DK_PLAYER && (dk & ~DK_SOLID) != DK_SHOP &&
                (dk & ~DK_SOLID) != DK_ENEMY && (dk & ~DK_SOLID) != DK_PDUMMY && (dk & ~DK_SOLID) != DK_DICE &&
                (dk & ~DK_SOLID) != DK_JAWS)
                continue;                         /* (Draw events that draw elsewhere or more are kept) */
        }
        if ((dk & DK_SOLID) && nmaps && kx >= txlo && kx < txhi && ky >= tylo && ky < tyhi &&
            (t = terrain_cell(p, &c)) != 0 && (m = map_of_depth(p->depth)) >= 0 &&
            t_cx >= wc0 && t_cx <= wc1 && t_cy >= wr0 && t_cy <= wr1) {
            uint32_t bit = 1u << (c & 31), *fw = &mframe[m][c >> 5];
            if (!mbase[m][c] && !(*fw & bit)) {
                mwant[m][c] = t;
                mown[m][c] = (int16_t)k;
                *fw |= bit;
                continue;
            }
            if (!mbase[m][c] && PW.in[mown[m][c]].id < p->id) {   /* the newer one is drawn first: it takes the */
                int o = mown[m][c];                               /* cell, the older one is drawn over it */
                mwant[m][c] = t;
                mown[m][c] = (int16_t)k;
                if (n < ENT_MAX) {
                    ents[n].dkey = fkey(PW.in[o].depth);
                    ents[n].id = PW.in[o].id;
                    ents[n].i = (int16_t)o;
                    n++;
                }
                continue;
            }
            /* taken by a tile, or by a newer instance: a sprite */
        }
        if (n < ENT_MAX) {
            ents[n].dkey = fkey(p->depth);
            ents[n].id = p->id;
            ents[n].i = (int16_t)k;
            n++;
        }
    }
    }
    for (k = 0; k < ntspr; k++) {                 /* tile sprites in view */
        const struct tspr *t = &tspr[k];
        if (t->x <= vx - 16 || t->x >= vx + VIEW_W || t->y <= vy - 16 || t->y >= vy + VIEW_H) continue;
        if (n < ENT_MAX) {
            ents[n].dkey = t->dkey;
            ents[n].id = 0x70000000 - (int32_t)t->seq;   /* before every instance of the depth */
            ents[n].i = (int16_t)(-1 - k);
            n++;
        }
    }
    PROF(1);
    /* insertion sort: depth descending, then id descending */
    for (k = 1; k < n; k++) {
        struct ent e = ents[k];
        int j = k - 1;
        while (j >= 0 && ent_before(&e, &ents[j])) {
            ents[j + 1] = ents[j];
            j--;
        }
        ents[j + 1] = e;
    }
    draw_st.sprites = (uint32_t)n;
    PROF(2);
    /* the list: background, then bands and drawables by depth */
    band_out(0);
    for (k = 0; k < n; k++) {
        const struct ent *e = &ents[k];
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
    PROF(3);
    if (draw_hud_on) {
        hud_out();
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
    cps3v_tilemap(0, vx, vy + DRAW_CROP, UNIT(0), 1);
    for (m = 0; m < NMAPS; m++) cps3v_tilemap(1 + m, vx, vy + DRAW_CROP, UNIT(1 + m), m < nmaps);
}

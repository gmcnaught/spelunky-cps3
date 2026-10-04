/* The collision grid of the shipping build (PLAN.md §1, docs/EQUIV.md): during play, the collision searches
 * (pcol_search / pcol_search_i: HandleCollision, collision_line / rectangle, instance_place, ...) visit the entries
 * that the runner's R-tree would hold (the same set, with the same rectangles, put in and taken out at the same
 * points) but newest first, from a uniform grid, instead of in the tree's order. Gameplay-equivalent, not
 * bit-exact: the order of hits differs where several overlap (explosion debris). The level generator keeps the tree
 * (bit-exact, gmode). -DPCOL_EXACT builds the R-tree in play too: the translation reference (test/host's playhost).
 *
 * Included by pcol.c once, after its entry tables (er, erw, ef) and search state (s_r, s_k, s_cb, s_ctx). Hooks in
 * pcol.c: PCOL_GRID_ON (a play-time entry or search uses the grid), pgrid_put / pgrid_out / pgrid_clear /
 * pgrid_search / pgrid_load.
 *
 * Layout. Cells of 2^PCOL_GRID_SHIFT px (default 16) over 0 .. PGRID_PX_W x 0 .. PGRID_PX_H (clamped at the edges:
 * any room size works). An entry is in the cell of its rectangle's left / top corner when it spans at most two
 * cells each way, else on the big list. A search reads the cells from one left / above its rectangle's left / top
 * cell to its right / bottom cell, and the big list; the hits are sorted newest first (insertion sort on the
 * creation number) and called back. Cell coordinates: integer rectangles (struct rbr w) by shifts; float ones by a
 * binary search of the cell boundaries' keys (no soft-float).
 */
#ifndef PCOL_EXACT

#ifndef PCOL_GRID_SHIFT
#define PCOL_GRID_SHIFT 4                       /* 16 px: SH-2 mean step 291 K (32 px 297 K, 64 px 328 K; docs/EQUIV.md) */
#endif
#define PGRID_PX_W 1024                         /* rLevel 640 + margin; wider rooms clamp to the last column */
#define PGRID_PX_H 768
#define PGRID_W ((PGRID_PX_W >> PCOL_GRID_SHIFT) + 1)
#define PGRID_H ((PGRID_PX_H >> PCOL_GRID_SHIFT) + 1)
#define PGRID_BIG (-2)
#define PGRID_NONE (-1)

static int16_t pg_head[PGRID_H * PGRID_W], pg_big = -1;
static int16_t pg_next[PIN_MAX], pg_prev[PIN_MAX], pg_cell[PIN_MAX];
static rk pg_bx[PGRID_W], pg_by[PGRID_H];        /* keys of the cell boundaries (index 0: -infinity) */
static uint8_t pg_init;
static int16_t pg_buf[PIN_MAX];
static int16_t pg_seq[PIN_MAX];

#define PCOL_GRID_ON (!gmode)

static int pg_clampx(int32_t c) { return c < 0 ? 0 : c >= PGRID_W ? PGRID_W - 1 : (int)c; }
static int pg_clampy(int32_t c) { return c < 0 ? 0 : c >= PGRID_H ? PGRID_H - 1 : (int)c; }

/* the cell index of a key: the last boundary <= k */
static int pg_bsearch(rk k, const rk *bd, int n)
{
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int m = (lo + hi + 1) >> 1;
        if (bd[m] <= k) lo = m; else hi = m - 1;
    }
    return lo;
}

/* the unclamped cell span of a rectangle (x0, y0, x1, y1), then clamped; returns 1 when it fits a cell entry */
static void pg_cells(const rk *r, int w, int *c)
{
    if (w) {
        c[0] = pg_clampx(r[0] >> PCOL_GRID_SHIFT); c[1] = pg_clampy(r[1] >> PCOL_GRID_SHIFT);
        c[2] = pg_clampx(r[2] >> PCOL_GRID_SHIFT); c[3] = pg_clampy(r[3] >> PCOL_GRID_SHIFT);
    } else {
        c[0] = pg_bsearch(r[0], pg_bx, PGRID_W); c[1] = pg_bsearch(r[1], pg_by, PGRID_H);
        c[2] = pg_bsearch(r[2], pg_bx, PGRID_W); c[3] = pg_bsearch(r[3], pg_by, PGRID_H);
    }
}

static void pgrid_clear(void)
{
    int k;
    if (!pg_init) {
        pg_bx[0] = pg_by[0] = (rk)0x80000000;
        for (k = 1; k < PGRID_W; k++) pg_bx[k] = ikey(k << PCOL_GRID_SHIFT);
        for (k = 1; k < PGRID_H; k++) pg_by[k] = ikey(k << PCOL_GRID_SHIFT);
        pg_init = 1;
    }
    for (k = 0; k < PGRID_H * PGRID_W; k++) pg_head[k] = -1;
    for (k = 0; k < PIN_MAX; k++) pg_cell[k] = PGRID_NONE;
    pg_big = -1;
}

/* the entry leaves the grid */
static void pgrid_out(int e)
{
    int c = pg_cell[e];
    if (c == PGRID_NONE) return;
    if (pg_prev[e] >= 0) pg_next[pg_prev[e]] = pg_next[e];
    else if (c == PGRID_BIG) pg_big = pg_next[e];
    else pg_head[c] = pg_next[e];
    if (pg_next[e] >= 0) pg_prev[pg_next[e]] = pg_prev[e];
    pg_cell[e] = PGRID_NONE;
}

/* the entry (er[e], erw[e] set) goes in, or moves */
static void pgrid_put(int e)
{
    int c[4], cell;
    int16_t *h;
    pg_cells(er[e], erw[e], c);
    cell = (c[2] - c[0] > 1 || c[3] - c[1] > 1) ? PGRID_BIG : c[1] * PGRID_W + c[0];
    if (cell == pg_cell[e]) return;                /* same cell: nothing to relink (order is by creation number) */
    pgrid_out(e);
    h = cell == PGRID_BIG ? &pg_big : &pg_head[cell];
    pg_cell[e] = (int16_t)cell;
    pg_prev[e] = -1;
    pg_next[e] = *h;
    if (*h >= 0) pg_prev[*h] = (int16_t)e;
    *h = (int16_t)e;
}

/* s_r overlaps entry e (inclusive sides) */
static __attribute__((noinline)) int pg_overlap_f(int e)       /* a float side: s_overlap's key compare */
{
    const rk *r = er[e];
    struct rbr b;
    b.r[0] = r[0]; b.r[1] = r[1]; b.r[2] = r[2]; b.r[3] = r[3]; b.w = erw[e];
    return s_overlap(&b);
}

static inline int pg_overlap(int e)
{
    const rk *r = er[e];
    if (erw[e] & s_r.w)
        return !(s_r.r[0] > r[2] || r[0] > s_r.r[2] || s_r.r[1] > r[3] || r[1] > s_r.r[3]);
    return pg_overlap_f(e);
}

/* the search: s_r, s_cb, s_ctx set by pcol_search / pcol_search_i */
static void pgrid_search(void)
{
    int c[4], x, y, n = 0, k, j, e;
    if (s_r.w) pg_cells(s_r.r, 1, c);
    else {
        if (!s_kv) s_keys();
        pg_cells(s_k, 0, c);
    }
    if (c[0] > 0) c[0]--;
    if (c[1] > 0) c[1]--;
    for (y = c[1]; y <= c[3]; y++)
        for (x = c[0]; x <= c[2]; x++)
            for (e = pg_head[y * PGRID_W + x]; e >= 0; e = pg_next[e]) {
                PCST(pcol_st.visits++);
                if (pg_overlap(e)) pg_buf[n++] = (int16_t)e;
            }
    for (e = pg_big; e >= 0; e = pg_next[e]) {
        PCST(pcol_st.visits++);
        if (pg_overlap(e)) pg_buf[n++] = (int16_t)e;
    }
    for (k = 0; k < n; k++) pg_seq[k] = pw_seq[pg_buf[k]];
    for (k = 1; k < n; k++) {                      /* newest first */
        int16_t v = pg_buf[k], sv = pg_seq[k];
        for (j = k; j > 0 && pg_seq[j - 1] < sv; j--) { pg_buf[j] = pg_buf[j - 1]; pg_seq[j] = pg_seq[j - 1]; }
        pg_buf[j] = v;
        pg_seq[j] = sv;
    }
    for (k = 0; k < n; k++)
        if (s_cb && !s_cb(pg_buf[k], s_ctx)) return;
}

/* the generated level was renamed to play entries (gen_load): its tree entries go in the grid */
static void pgrid_load(int n)
{
    int e;
    pgrid_clear();
    for (e = 0; e < n; e++)
        if (ef[e] & EF_TREE) pgrid_put(e);
}

#else
#define PCOL_GRID_ON 0
static void pgrid_clear(void) {}
static void pgrid_out(int e) { (void)e; }
static void pgrid_put(int e) { (void)e; }
static void pgrid_search(void) {}
static void pgrid_load(int n) { (void)n; }
#endif

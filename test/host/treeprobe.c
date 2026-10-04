/* The C side of tools/treeprobe.py: replays a case (creations, destroys, moves of oBrickSmooth) through the play
 * world's collision tree (src/game/pcol.c) and prints the three dumps as case indices.
 *   build/host/treeprobe <case> > out   (built by tools/treeprobe.py --c) */
#include <stdio.h>
#include <stdlib.h>
#include "pint.h"
#include "pcol.h"

static int idx[4096], n;

static void dump(void)
{
    static int32_t ids[PIN_MAX];
    int m = pcol_probe(OBJ_oBrickSmooth, ids, PIN_MAX), k, j;
    printf("%d", m);
    for (k = 0; k < m; k++) {
        for (j = 0; j < n; j++)
            if (PX(idx[j]).id == ids[k]) break;
        printf(" %d", j);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    int nd, nm, k, x, y, v;
    static int dest[4096], move[4096];
    if (!f || fscanf(f, "%d %d %d", &n, &nd, &nm) != 3) return 2;
    pw_reset();
    PW.next_id = 100000;
    for (k = 0; k < n; k++) {
        if (fscanf(f, "%d %d", &x, &y) != 2) return 2;
        idx[k] = pin_create(PI(x), PI(y), OBJ_oBrickSmooth);
    }
    for (k = 0; k < nd; k++) { if (fscanf(f, "%d", &v) != 1) return 2; dest[k] = v; }
    for (k = 0; k < nm; k++) { if (fscanf(f, "%d", &v) != 1) return 2; move[k] = v; }
    dump();
    for (k = 0; k < nd; k++) pin_kill(idx[dest[k]]);
    pcol_remove_marked();
    dump();
    for (k = 0; k < nm; k++) {
        PX(idx[move[k]]).x += PI(16);
        pcol_mark(idx[move[k]]);
    }
    dump();
    return 0;
}

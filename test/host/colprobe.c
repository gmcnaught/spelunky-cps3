/* tools/colprobe.py's C side: each case's two instances (oBrickSmooth with the sprite, image_index, image_angle,
 * scales, x, y the probe sets), then the pworld.c tests the runner's answers are compared with:
 * "k pair point1 rect1 line1 point0 rect0 line0" (1 hit, 0 miss).   build/host/colprobe <case file> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pint.h"

static int spr_index(const char *n)
{
    int s;
    for (s = 0; s < GSPR_COUNT; s++)
        if (!strcmp(gsprname[s], n)) return s;
    fprintf(stderr, "unknown sprite %s\n", n);
    exit(2);
}

static int make(const char *spr, double img, double x, double y, double ang, double xs, double ys, int32_t id)
{
    int i = pin_add(OBJ_oBrickSmooth, (pos)x, (pos)y, id);
    struct pin *p = &PX(i);
    pin_setspr(p, spr_index(spr));
    PE(p)->alpha = 1;
    pin_setimg(p, (img_t)img);
    pin_setangle(p, (float)ang);
    pin_setxscale(p, (float)xs);
    pin_setyscale(p, (float)ys);
    return i;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    char sa[64], sb[64];
    double a[6], b[6], p[2], r[4], l[4];
    int k = 0;
    if (!f) return 2;
    while (fscanf(f, "%63s %lf %lf %lf %lf %lf %lf %63s %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                  sa, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], sb, &b[0], &b[1], &b[2], &b[3], &b[4], &b[5],
                  &p[0], &p[1], &r[0], &r[1], &r[2], &r[3], &l[0], &l[1], &l[2], &l[3]) == 24) {
        int ia, ib;
        pw_reset();
        ia = make(sa, a[0], a[1], a[2], a[3], a[4], a[5], 100000);
        ib = make(sb, b[0], b[1], b[2], b[3], b[4], b[5], 100001);
        printf("%d %d %d %d %d %d %d %d\n", k, pw_test_pair(ia, ib) ? 1 : 0, pw_test_point(ia, p[0], p[1], 1),
               pw_test_rect(ia, r[0], r[1], r[2], r[3], 1), pw_test_line(ia, l[0], l[1], l[2], l[3], 1),
               pw_test_point(ia, p[0], p[1], 0), pw_test_rect(ia, r[0], r[1], r[2], r[3], 0),
               pw_test_line(ia, l[0], l[1], l[2], l[3], 0));
        k++;
    }
    return 0;
}

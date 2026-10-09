#!/usr/bin/env python3
"""mktuples.py <tuples.txt> <out.h>: tests/softfp's OP_PIECEH table (build/piece_tuples.h, used when present) from
lines "y yVel yAcc" (hex bits) that a host build instrumented at rubblepiece_step's y move logged over the 184
hostident runs, deduplicated in first-seen order (consecutive pieces sharing yVel / yAcc stay adjacent: the memo's
hits); docs/PERF3.md (branch softadd)"""
import sys
rows = [l.split() for l in open(sys.argv[1]) if l.strip()]
with open(sys.argv[2], 'w') as f:
    f.write('/* %d unique (y, yVel, yAcc) tuples of rubblepiece_step in the 184 host runs  */\n' % len(rows))
    f.write('#define PT_N %d\n' % len(rows))
    f.write('static const uint32_t pt_y[PT_N] = {\n%s };\n' % ',\n'.join(', '.join('0x%su' % r[0] for r in rows[i:i + 8]) for i in range(0, len(rows), 8)))
    f.write('static const uint64_t pt_v[PT_N] = {\n%s };\n' % ',\n'.join(', '.join('0x%sull' % r[1] for r in rows[i:i + 4]) for i in range(0, len(rows), 4)))
    f.write('static const uint64_t pt_a[PT_N] = {\n%s };\n' % ',\n'.join(', '.join('0x%sull' % r[2] for r in rows[i:i + 4]) for i in range(0, len(rows), 4)))

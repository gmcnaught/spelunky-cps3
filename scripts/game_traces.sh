#!/bin/sh
cd "$(dirname "$0")/.."
TRACE_SHOT=30,150,300,450,520,600,700,800 TRACE_NOENEMY=1 XVFB_SCREEN=1280x960x24 scripts/hd_trace.sh p4_exit559 559 g_p4_exit559_s559 | tail -1
TRACE_SHOT=60,90,160,214,240 TRACE_LEVEL=1 TRACE_MONEY=0 XVFB_SCREEN=1280x960x24 scripts/hd_trace.sh p5_spider 121 g_p5_spider_s121 | tail -1
L=$(sed -n 's/^# *level \([0-9]*\).*/\1/p' tests/routes/p5_shop.txt | head -1); M=$(sed -n 's/^# *money \([0-9]*\).*/\1/p' tests/routes/p5_shop.txt | head -1)
TRACE_SHOT=50,70,162,242,300 TRACE_LEVEL=${L:-1} TRACE_MONEY=${M:-0} XVFB_SCREEN=1280x960x24 scripts/hd_trace.sh p5_shop 96 g_p5_shop_s96 | tail -1

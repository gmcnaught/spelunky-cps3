#!/bin/sh
# The front end's reference traces (scripts/front_host.sh, scripts/game_check.sh ATTRACT=1 / 5): HD's boot with no
# input (the intro, then the title: TRACE_BOOT), and the high-scores room entered from the title flow (TRACE_ROOM),
# with runner frames (TRACE_SHOT), runner frames with the GUI (TRACE_GUI), the HUD / front globals and the sounds.
cd "$(dirname "$0")/.."
R=1,100,200,300,400,500,600,700,800,820,880,950,1000,1040,1060,1080,1100,1150,1200,1290
TRACE_BOOT=1 TRACE_HUD=1 TRACE_SND=1 TRACE_SHOT=$R TRACE_GUI=$R TRACE_TAIL=0 XVFB_SCREEN=1280x960x24 scripts/hd_trace.sh p8_boot 7 g_p8_boot_s7 | tail -1
R=1,20,60,120,199
TRACE_ROOM=rHighscores TRACE_HUD=1 TRACE_SND=1 TRACE_SHOT=$R TRACE_GUI=$R TRACE_TAIL=0 XVFB_SCREEN=1280x960x24 scripts/hd_trace.sh p8_scores 7 g_p8_scores_s7 | tail -1

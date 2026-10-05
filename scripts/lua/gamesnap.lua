-- MAME: tests/game's snapshots (tests/game/main.c marker at 0x02000000). While the program holds a frame (wait = 1),
-- the frame on screen shows record `shown`: two frames later a snapshot is taken, the line
-- "S <k> <rec> <draws> <dclk> <dmax> <vclk> <vmax> <entries> <entries_max> <sprites> <cells> <unsup> <todo> <noart>
--  <dropped> <steps> <vx> <vy> <room> <prof x 5 (clocks; DRAW_PROFILE builds)>" goes to $GAME_OUT and ack = shown lets the program go on. After $GAME_NSNAPS
-- snapshots, or when the route has ended (state 1), a final "E ..." line with the same fields (dsum as a double),
-- then MAME exits.
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local out = io.open(os.getenv("GAME_OUT"), "w")
local want = tonumber(os.getenv("GAME_NSNAPS") or "0")
local A = 0x02000000
local seen, wait_frames, taken, last = -1, 0, 0, 0
local function r(o) return mem:read_u32(A + o) end
local function s32(v) if v >= 0x80000000 then return v - 0x100000000 end return v end
local function fields()
  return string.format("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d  %d %d %d %d %d", r(20), r(24), r(28),
    r(40), r(44), r(48), r(52), r(56), r(60), r(64), r(68), r(72), r(76), r(80), s32(r(84)), s32(r(88)), s32(r(92)),
    r(104), r(108), r(112), r(116), r(120))
end
emu.register_frame_done(function()
  if r(0) ~= 0x53474d31 then return end
  local t = manager.machine.time:as_double()
  if t - last >= 60 then
    last = t
    print(string.format("progress t=%.0f steps=%d records=%d shown=%d", t, r(80), r(96), s32(r(8))))
  end
  if r(16) == 1 then
    local shown = s32(r(8))
    if shown ~= seen then seen = shown; wait_frames = 0 end
    wait_frames = wait_frames + 1
    if wait_frames == 1 and os.getenv("GAME_MIDSNAP") then   -- smooth motion: the midpoint list, before the
      manager.machine.screens[":screen"]:snapshot(string.format("mid_%04d.png", taken))   -- frame's own one
    end
    if wait_frames == 3 then
      manager.machine.video:snapshot()
      out:write(string.format("S %d %d %s %.3f\n", taken, shown, fields(), t))
      out:flush()
      taken = taken + 1
      mem:write_u32(A + 12, shown)
    end
  end
  if (want > 0 and taken >= want and r(16) == 0) or r(4) == 1 then
    out:write(string.format("E %d %.0f %s\n", taken, r(32) + 4294967296.0 * r(36), fields()))
    out:close()
    manager.machine:exit()
  end
end)

-- MAME: a cabinet game on tests/game PLAY=1 (scripts/capture_check.sh) and its GAME CAPTURE pages.
--   coin, Start at frame CAP_START; then random controls (xorshift seeded by CAP_MONKEY) for CAP_PLAY frames; then
--   bombs dropped at the player's feet until life is 0; the game over panel dismissed; once the capture's header has
--   the game over bit (CAP_ADDR + 8, 0x10), Coin + B2 open the settings screen, down to GAME CAPTURE, B1: CAP_SHOTS
--   snapshots one page apart (B1); B2, the test switch (saves and restarts the program); the settings screen
--   again and CAP_SHOTS more snapshots (the capture kept over the restart); exit.
-- CAP_GOD=1 (a DEV=1 build): the settings screen's INVINCIBLE first (save and restart), then the game; after
--   CAP_PLAY frames its capture is taken mid-game (the settings screen's restart ends the game).
-- CAP_TOGGLE=1: the settings screen's RUN BUTTON TOGGLE first (with CAP_GOD in the same visit).
-- CAP_LOG: per step "T <steps> <game_probe>" (tests/game/main.c marker block +80 steps, +124 probe, after each
-- draw), and "SHOTS <set> <count>" lines.
local screen = manager.machine.screens[":screen"]
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local log = io.open(os.getenv("CAP_LOG"), "w")
local CAP = tonumber(os.getenv("CAP_ADDR"))
local START = tonumber(os.getenv("CAP_START") or "200")
local PLAY = tonumber(os.getenv("CAP_PLAY") or "6000")
local SHOTS = tonumber(os.getenv("CAP_SHOTS") or "24")
local GOD = os.getenv("CAP_GOD") == "1"             -- a DEV=1 build: INVINCIBLE set first; the capture mid-game
local CAPROW = GOD and 6 or 5                       -- GAME CAPTURE's row in the settings menu
local TOGGLE = os.getenv("CAP_TOGGLE") == "1"       -- RUN BUTTON TOGGLE set first
local rs = tonumber(os.getenv("CAP_MONKEY") or "1") * 2654435761 % 4294967296
if rs == 0 then rs = 1 end
local function rnd(n)                                -- xorshift32, 0 .. n - 1
  rs = rs ~ (rs << 13) & 0xffffffff; rs = rs ~ (rs >> 17); rs = rs ~ (rs << 5) & 0xffffffff
  return rs % n
end
local fields = {}
for _, p in pairs(manager.machine.ioport.ports) do for n, f in pairs(p.fields) do fields[n] = f end end
local NAMES = { up = "P1 Up", down = "P1 Down", left = "P1 Left", right = "P1 Right", b1 = "P1 Jab Punch",
  b2 = "P1 Strong Punch", b3 = "P1 Fierce Punch", b4 = "P1 Short Kick", b5 = "P1 Forward Kick",
  b6 = "P1 Roundhouse Kick", coin = "Coin 1", start = "1 Player Start", test = "Service Mode" }
local held = {}
local function set(want)                             -- exactly these held
  for k, n in pairs(NAMES) do
    local v = want[k] and 1 or 0
    if held[k] ~= v then fields[n]:set_value(v); held[k] = v end
  end
end
local A = 0x02000000
local function r(o) return mem:read_u32(A + o) end
local function s32(v) if v >= 0x80000000 then return v - 0x100000000 end return v end

local phase, t0, act, act_end, last_steps, bombs, set_n, shots = "boot", 0, {}, 0, 0, 0, 0, 0
local script = nil                                   -- { {frames, inputs}, ... } run in order
local function run_script(s, now) script = s; script.k = 1; script.until_f = now + s[1][1] end

emu.register_frame_done(function()
  local f = screen:frame_number()
  -- the step log
  if r(0) == 0x53474d31 then
    local st = r(80)
    if st ~= last_steps and st > 0 then
      last_steps = st
      log:write(string.format("T %d %d %d %d %08x %08x %08x\n", st, s32(r(124)), s32(r(128)), s32(r(132)), r(136),
        r(140), r(144)))
    end
  end
  if script then                                     -- a fixed input sequence
    if f >= script.until_f then
      script.k = script.k + 1
      if script.k > #script then script = nil; set({}) else script.until_f = f + script[script.k][1] end
    end
    if script then set(script[script.k][2]) end
    return
  end
  if phase == "boot" then
    if f == 60 and (GOD or TOGGLE) then              -- settings: RUN BUTTON TOGGLE / INVINCIBLE on, save, restart
      local s, row = { { 75, { coin = true, b2 = true } }, { 30, {} } }, 0
      local function to(k)
        for _ = row + 1, k do s[#s + 1] = { 2, { down = true } }; s[#s + 1] = { 6, {} } end
        row = k
        s[#s + 1] = { 2, { b1 = true } }; s[#s + 1] = { 10, {} }
      end
      if TOGGLE then to(2) end
      if GOD then to(5) end
      s[#s + 1] = { 20, {} }; s[#s + 1] = { 2, { test = true } }
      s[#s + 1] = { 300, {} }
      run_script(s, f)
      phase = "boot2"
    elseif f == 60 then
      run_script({ { 4, { coin = true } }, { START - 64, {} }, { 4, { start = true } } }, f)
      phase = "play"; t0 = f + START - 56
    end
  elseif phase == "boot2" then
    run_script({ { 4, { coin = true } }, { START - 64, {} }, { 4, { start = true } } }, f)
    phase = "play"; t0 = f + START - 56
  elseif phase == "play" then
    if f < t0 then return end
    if f >= act_end then                             -- a new random action
      act = {}
      local d = rnd(10)
      if d < 4 then act.right = true elseif d < 7 then act.left = true end
      if rnd(6) == 0 then act.up = true elseif rnd(8) == 0 then act.down = true end
      if rnd(3) == 0 then act.b1 = true end
      if rnd(5) == 0 then act.b2 = true end
      if rnd(3) == 0 then act.b4 = true end
      if rnd(40) == 0 then act.b6 = true end
      if rnd(60) == 0 then act.b3 = true end
      if rnd(50) == 0 then act.start = true end      -- pay (in a shop)
      act_end = f + 2 + rnd(30)
    end
    set(act)
    if f >= t0 + PLAY then
      if GOD then phase = "menu"; t0 = f; set({}) else phase = "die" end
    end
  elseif phase == "die" then                         -- drop bombs until life is 0 (at most 8)
    if s32(r(124)) <= 0 or bombs >= 8 then phase = "over"; t0 = f; return end
    bombs = bombs + 1
    run_script({ { 20, {} }, { 4, { down = true } }, { 4, { down = true, b5 = true } }, { 220, {} } }, f)
  elseif phase == "over" then                        -- the game over panel: a press now and then
    if (mem:read_u32(CAP + 8) & 0x10) ~= 0 then phase = "menu"; t0 = f; set({}); return end
    if f - t0 > 6000 then phase = "menu"; t0 = f; return end   -- give up waiting: the capture so far
    set({ b2 = (f % 60) < 4 })
  elseif phase == "menu" then
    if f < t0 + 120 then return end
    set_n = set_n + 1
    local s = { { 75, { coin = true, b2 = true } }, { 30, {} } }
    for _ = 1, CAPROW do s[#s + 1] = { 2, { down = true } }; s[#s + 1] = { 6, {} } end
    s[#s + 1] = { 2, { b1 = true } }
    s[#s + 1] = { 40, {} }
    run_script(s, f)
    phase = "shots"; shots = 0; t0 = 0
  elseif phase == "shots" then
    if t0 == 0 then t0 = f end
    if (f - t0) % 20 == 10 then manager.machine.video:snapshot(); shots = shots + 1
    elseif (f - t0) % 20 == 14 then run_script({ { 2, { b1 = true } } }, f) end
    if shots == SHOTS and (f - t0) % 20 == 19 then
      log:write(string.format("SHOTS %d %d\n", set_n, shots))
      if set_n == 1 then                             -- back to the menu, save and restart, then again
        run_script({ { 2, { b2 = true } }, { 20, {} }, { 2, { test = true } }, { 300, {} } }, f)
        phase = "menu"; t0 = f
      else
        log:close(); manager.machine:exit()
      end
    end
  end
end)

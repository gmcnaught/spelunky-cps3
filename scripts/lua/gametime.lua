-- MAME: tests/gametime's results (marker 'GTM1' at main RAM 0x02000000, tests/gametime/gametime.h). When the route
-- has ended (state 1) and the results screen is up, a snapshot; the marker's words to $GT_OUT as "M <name> <value>"
-- lines and each step pair's clocks as "P <k> <clocks>"; then MAME exits.
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local A = 0x02000000
local names = {"magic", "state", "frames", "steps", "pairs", "vbl_sum", "vbl_max", "snd_sum", "snd_max", "step_sum",
  "step_max", "draw_sum", "draw_max", "shl_sum", "shl_max", "pair_sum_lo", "pair_sum_hi", "pair_max", "pair_max_at",
  "start_clk", "over"}
local wait, last = -1, 0
emu.register_frame_done(function()
  local t = manager.machine.time:as_double()
  if t - last >= 30 then last = t; print(string.format("progress t=%.0f steps=%d", t, mem:read_u32(A + 12))) end
  if mem:read_u32(A) ~= 0x47544d31 then return end
  if wait < 0 then
    if mem:read_u32(A + 4) == 1 then wait = 10 end
    return
  end
  wait = wait - 1
  if wait > 0 then return end
  manager.machine.video:snapshot()
  local f = io.open(os.getenv("GT_OUT"), "w")
  for k, n in ipairs(names) do f:write(string.format("M %s %d\n", n, mem:read_u32(A + 4 * (k - 1)))) end
  local np = math.min(mem:read_u32(A + 16), 1024)
  for k = 0, np - 1 do f:write(string.format("P %d %d\n", k, mem:read_u32(A + 84 + 4 * k))) end
  f:close()
  manager.machine:exit()
end)

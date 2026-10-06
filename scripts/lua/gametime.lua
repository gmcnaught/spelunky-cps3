-- MAME: tests/gametime's results (marker 'GTM2' at sprite RAM 0x0402e000: tests/gametime/marker.ld, gametime.h). When the last
-- section has ended (state 1) and the results screen is up, a snapshot; each section's words to $GT_OUT as
-- "S <sec> <name> <value>" lines and its step pairs as "S <sec> pair <clocks> ..."; then MAME exits.
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local A = 0x0402e000
local names = {"frames", "steps", "pairs", "vbl_sum", "vbl_max", "snd_sum", "snd_max", "step_sum", "step_max",
  "draw_sum", "draw_max", "shl_sum", "shl_max", "pair_sum_lo", "pair_sum_hi", "pair_max", "pair_max_at", "start_clk",
  "over"}
local PAIRS, SECW = 512, 19 + 512
local wait, last = -1, 0
emu.register_frame_done(function()
  local t = manager.machine.time:as_double()
  if t - last >= 30 then last = t; print(string.format("progress t=%.0f sec=%d", t, mem:read_u32(A + 8))) end
  if mem:read_u32(A) ~= 0x47544d32 then return end
  if wait < 0 then
    if mem:read_u32(A + 4) == 1 then wait = 10 end
    return
  end
  wait = wait - 1
  if wait > 0 then return end
  manager.machine.video:snapshot()
  local f = io.open(os.getenv("GT_OUT"), "w")
  for s = 0, 2 do
    local b = A + 12 + 4 * SECW * s
    for k, n in ipairs(names) do f:write(string.format("S %d %s %d\n", s, n, mem:read_u32(b + 4 * (k - 1)))) end
    local np = math.min(mem:read_u32(b + 8), PAIRS)
    local line = "S " .. s .. " pair"
    for k = 0, np - 1 do line = line .. " " .. mem:read_u32(b + 4 * (19 + k)) end
    f:write(line .. "\n")
  end
  f:close()
  manager.machine:exit()
end)

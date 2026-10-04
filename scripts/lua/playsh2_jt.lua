-- MAME: tests/playsh2's jtcps3 variant (JT builds): when its state word (main RAM 0x02000004) is 1 the screen is
-- drawn; a few frames later a snapshot is taken and the per-job words (main.c JTR: total clocks, steps, step
-- clocks, largest step, hash, level start) go to $PSH2_OUT as "J job w0 .. w5" lines, the sprite RAM test's count as
-- "S n"; then MAME exits.
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local out = os.getenv("PSH2_OUT")
local njobs = tonumber(os.getenv("PSH2_NJOBS"))
local wait = -1
emu.register_frame_done(function()
  if wait < 0 then
    if mem:read_u32(0x02000004) == 1 then wait = 10 end
    return
  end
  wait = wait - 1
  if wait > 0 then return end
  manager.machine.video:snapshot()
  local f = io.open(out, "w")
  f:write(string.format("S %d\n", mem:read_u32(0x02000000 + 4 * 15)))
  for j = 0, njobs - 1 do
    local s = "J " .. j
    for k = 0, 5 do s = s .. string.format(" %d", mem:read_u32(0x02000000 + 4 * (16 + 8 * j + k))) end
    f:write(s .. "\n")
  end
  f:close()
  manager.machine:exit()
end)

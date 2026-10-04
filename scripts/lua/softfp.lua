-- MAME: tests/softfp's results. When the state word (sprite RAM 0x04000004) is 1, one line per operation to
-- $SOFTFP_OUT: "O op cases differ hash fpbit_clocks_256 softfp_clocks_256 loop_clocks_256 [a b xor]...", then exit.
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local out = os.getenv("SOFTFP_OUT")
local done, last = false, 0
emu.register_periodic(function()
  local t = manager.machine.time:as_double()
  if t - last >= 100 then last = t; print(string.format("progress t=%.0f ops=%d", t, mem:read_u32(0x04000010))) end
  if done or mem:read_u32(0x04000000) ~= 0x53465054 or mem:read_u32(0x04000004) ~= 1 then return end
  done = true
  local f = io.open(out, "w")
  for op = 0, mem:read_u32(0x04000008) - 1 do
    local a = 0x04000100 + 64 * op
    local w = {}
    for k = 0, 15 do w[k] = mem:read_u32(a + 4 * k) end
    local s = string.format("O %d %d %d %08x %d %d %d", op, w[0], w[1], w[2], w[3], w[4], w[5])
    for n = 0, math.min(w[1], 2) - 1 do
      local b = 6 + 5 * n
      s = s .. string.format(" %08x%08x %08x%08x %08x", w[b], w[b + 1], w[b + 3], w[b + 2], w[b + 4])
    end
    f:write(s .. "\n")
  end
  f:close()
  manager.machine:exit()
end)

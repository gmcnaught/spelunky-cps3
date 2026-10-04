-- MAME: tests/playsh2's results. When the program sets its state word (sprite RAM 0x04000004) to 1, the records
-- (0x04000100, 5 words each) go to $PSH2_OUT as "R job kind idx sum n extra clocks" lines and the PROF samples
-- (0x04010000, 16-byte buckets of PC and PR) to $PSH2_OUT.prof as "S pc pr" lines; then MAME exits.
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local out = os.getenv("PSH2_OUT")
local done = false
local last = 0
emu.register_periodic(function()
  local t = manager.machine.time:as_double()
  if t - last >= 200 then   -- progress in mame.log
    last = t
    print(string.format("progress t=%.0f records=%d", t, mem:read_u32(0x04000008)))
  end
  if done or mem:read_u32(0x04000000) ~= 0x50534832 or mem:read_u32(0x04000004) ~= 1 then return end
  done = true
  local n = mem:read_u32(0x04000008)
  local f = io.open(out, "w")
  f:write(string.format("T %.6f %d %d %d %d %d\n", manager.machine.time:as_double(), n, mem:read_u32(0x0400000c),
                        mem:read_u32(0x0400001c), mem:read_u32(0x04000020), mem:read_u32(0x04000030)))
  for k = 0, n - 1 do
    local a = 0x04000100 + 20 * k
    local w = mem:read_u32(a)
    f:write(string.format("R %d %d %d %08x %d %d %d\n", w >> 16, (w >> 12) & 15, w & 0xfff, mem:read_u32(a + 4),
                          mem:read_u32(a + 8), mem:read_u32(a + 12), mem:read_u32(a + 16)))
  end
  f:close()
  local ns = mem:read_u32(0x04000018)
  if ns > 0 then
    local g = io.open(out .. ".prof", "w")
    for k = 0, ns - 1 do
      local w = mem:read_u32(0x04010000 + 4 * k)
      g:write(string.format("S %08x %08x\n", 0x06000000 + ((w >> 16) << 4), 0x06000000 + ((w & 0xffff) << 4)))
    end
    g:close()
  end
  manager.machine:exit()
end)

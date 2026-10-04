-- MAME: tests/playsh2's results (character RAM, tests/playsh2/main.c). When the program sets its state word
-- (0x04100004) to 1, the records (0x04100100, 5 words each) go to $PSH2_OUT as "R job kind idx sum n extra clocks"
-- lines; PROF samples (0x04140000, 16-byte buckets of PC and PR) to $PSH2_OUT.prof as "S pc pr" lines; with
-- $PSH2_ATTR (OBJ_COUNT) the ATTR tables (0x04180000, tests/playsh2/attr.c) to $PSH2_OUT.attr; then MAME exits.
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local out = os.getenv("PSH2_OUT")
local done = false
local last = 0
emu.register_periodic(function()
  local t = manager.machine.time:as_double()
  if t - last >= 200 then   -- progress in mame.log
    last = t
    print(string.format("progress t=%.0f records=%d", t, mem:read_u32(0x04100008)))
  end
  if done or mem:read_u32(0x04100000) ~= 0x50534832 or mem:read_u32(0x04100004) ~= 1 then return end
  done = true
  local n = mem:read_u32(0x04100008)
  local f = io.open(out, "w")
  f:write(string.format("T %.6f %d %d %d %d %d\n", manager.machine.time:as_double(), n, mem:read_u32(0x0410000c),
                        mem:read_u32(0x0410001c), mem:read_u32(0x04100020), mem:read_u32(0x04100030)))
  for k = 0, n - 1 do
    local a = 0x04100100 + 20 * k
    local w = mem:read_u32(a)
    f:write(string.format("R %d %d %d %08x %d %d %d\n", w >> 16, (w >> 12) & 15, w & 0xfff, mem:read_u32(a + 4),
                          mem:read_u32(a + 8), mem:read_u32(a + 12), mem:read_u32(a + 16)))
  end
  f:close()
  -- ATTR builds (tests/playsh2/attr.c): categories and event x object clocks / calls
  if os.getenv("PSH2_ATTR") then
    local g = io.open(out .. ".attr", "w")
    local function e64(a) return mem:read_u32(a) + 4294967296 * mem:read_u32(a + 4), mem:read_u32(a + 8) end
    for c = 0, 3 do
      local clk, calls = e64(0x04180000 + 12 * c)
      g:write(string.format("C %d %.0f %d\n", c, clk, calls))
    end
    local nobj = tonumber(os.getenv("PSH2_ATTR"))
    for t = 0, 7 do
      for o = 0, nobj - 1 do
        local clk, calls = e64(0x04180100 + 12 * (t * nobj + o))
        if calls > 0 then g:write(string.format("E %d %d %.0f %d\n", t, o, clk, calls)) end
      end
    end
    g:close()
  end
  local ns = mem:read_u32(0x04100018)
  if ns > 0 then
    local g = io.open(out .. ".prof", "w")
    for k = 0, ns - 1 do
      local w = mem:read_u32(0x04140000 + 4 * k)
      g:write(string.format("S %08x %08x\n", 0x06000000 + ((w >> 16) << 4), 0x06000000 + ((w & 0xffff) << 4)))
    end
    g:close()
  end
  manager.machine:exit()
end)

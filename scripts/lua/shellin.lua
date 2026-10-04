-- MAME: tests/shell's inputs. Each event holds a field for a number of screen frames; at END the step log of
-- tests/shell/main.c (struct shlog at SHLOG, from tests/shell/build/main.map) is printed as LOG lines, then exit.
local SHLOG = tonumber(os.getenv("SHLOG") or "0x02000000")
local END = 470
local ev = {   -- { first frame, port, field, frames held }
  { 100, ":INPUTS", "1 Player Start", 10 },          -- no credit: nothing
  { 130, ":INPUTS", "Coin 1", 3 },                   -- credit 1
  { 150, ":INPUTS", "Coin 2", 3 },                   -- credit 2
  { 170, ":INPUTS", "1 Player Start", 10 },          -- game on panel 1, credit 1 left
  { 200, ":INPUTS", "P1 Jab Punch", 1 },             -- a one-frame tap: jump
  { 210, ":INPUTS", "P1 Right", 20 },                -- right held 20 frames
  { 240, ":INPUTS", "1 Player Start", 4 },           -- pay
  { 260, ":INPUTS", "P2 Right", 10 },                -- the other panel: ignored
  { 280, ":EXTRA", "P1 Roundhouse Kick", 4 },        -- rope: the test game ends
  { 300, ":INPUTS", "2 Players Start", 10 },         -- game on panel 2, credit 0 left
  { 320, ":EXTRA", "P2 Short Kick", 6 },             -- run
  { 330, ":INPUTS", "P1 Right", 6 },                 -- the other panel: ignored
  { 340, ":INPUTS", "P2 Roundhouse Kick", 4 },       -- rope: game over
  { 360, ":INPUTS", "1 Player Start", 6 },           -- no credit: nothing
  { 380, ":INPUTS", "Service 1", 3 },                -- service: credit 1
  { 400, ":INPUTS", "Coin 1", 2 },                   -- credit 2
  { 404, ":INPUTS", "Coin 1", 2 },                   -- credit 3
}
local screen = manager.machine.screens[":screen"]
local ports = manager.machine.ioport.ports
local mem = manager.machine.devices[":maincpu"].spaces["program"]
emu.register_frame_done(function()
  local f = screen:frame_number()
  for _, e in ipairs(ev) do
    local fld = ports[e[2]].fields[e[3]]
    if f == e[1] then fld:set_value(1) elseif f == e[1] + e[4] then fld:set_value(0) end
  end
  if f == END then
    local n = mem:read_u32(SHLOG + 4)
    print(string.format("MAGIC %08x N %d", mem:read_u32(SHLOG), n))
    local h = SHLOG + 8 + 16 * 1024
    print(string.format("BOOT %d %d %d %d", mem:read_u32(h), mem:read_u32(h + 4), mem:read_u32(h + 8), mem:read_u32(h + 12)))
    for k = 0, n - 1 do
      local a = SHLOG + 8 + 16 * k
      print(string.format("LOG %d %d %d %d %d %d %d %d", mem:read_u32(a), mem:read_u8(a + 4), mem:read_u8(a + 5),
        mem:read_u8(a + 6), mem:read_u8(a + 7), mem:read_u16(a + 8), mem:read_u16(a + 10), mem:read_u16(a + 12)))
    end
    manager.machine:exit()
  end
end)

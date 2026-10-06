-- MAME autoboot script (scripts/cachebench.sh): tests/cachebench's results (cb_res at 0x02000000: magic 'CBEN',
-- passes, rows, values) written to $CB_OUT as "<row> <value>" after 2 passes, with a snapshot of the screen; then exit.
local machine = manager.machine
local sp = machine.devices[":maincpu"].spaces["program"]
emu.register_frame_done(function()
  if sp:read_u32(0x02000000) ~= 0x4342454e or sp:read_u32(0x02000004) < 2 then return end
  local f = assert(io.open(os.getenv("CB_OUT"), "w"))
  for i = 0, sp:read_u32(0x02000008) - 1 do
    local v = sp:read_u32(0x0200000c + 4 * i)
    if v >= 0x80000000 then v = v - 0x100000000 end
    f:write(string.format("%d %d\n", i, v))
  end
  f:close()
  machine.video:snapshot()
  machine:exit()
end)

-- MAME: every write to the CPS3 sound registers (0x040e0000-0x040e02ff and the cache-through mirror 0x240e0000),
-- logged to SNDLOG as "<machine time s> <word offset> <data hex>" (tools/sndcheck.py)
local sp = manager.machine.devices[":maincpu"].spaces["program"]
local log = io.open(os.getenv("SNDLOG"), "w")
local function tap(base)
  return sp:install_write_tap(base, base + 0x2ff, "snd" .. base, function(offset, data, mask)
    log:write(string.format("%.9f %d %08x\n", manager.machine.time:as_double(), (offset - base) // 4, data))
  end)
end
snd_tap1 = tap(0x040e0000)
snd_tap2 = tap(0x240e0000)
emu.add_machine_stop_notifier(function() log:close() end)

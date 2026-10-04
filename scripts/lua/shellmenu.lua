-- MAME: tests/shell settings screen (src/shell/shell.c settings_run): the test switch opens it, B1 turns free play
-- on, down + right sets 2 coins a credit, the test switch saves and restarts; then Start without a coin must begin a
-- game (free play). Prints BEGIN <frame> <credits> per game begun, snapshots the menu and the attract screen.
local SHLOG = tonumber(os.getenv("SHLOG") or "0x02000000")
local screen = manager.machine.screens[":screen"]
local P = manager.machine.ioport.ports
local ev = { {60, ":INPUTS", "Service Mode", 2}, {110, ":INPUTS", "P1 Jab Punch", 2}, {140, ":INPUTS", "P1 Down", 2},
             {150, ":INPUTS", "P1 Right", 2}, {160, ":INPUTS", "Service Mode", 2}, {300, ":INPUTS", "1 Player Start", 4} }
emu.register_frame_done(function()
  local f = screen:frame_number()
  for _, e in ipairs(ev) do
    local fld = P[e[2]].fields[e[3]]
    if f == e[1] then fld:set_value(1) elseif f == e[1] + e[4] then fld:set_value(0) end
  end
  if f == 100 or f == 155 or f == 250 or f == 330 then manager.machine.video:snapshot() end
  if f == 330 then
    local mem = manager.machine.devices[":maincpu"].spaces["program"]
    local n = mem:read_u32(SHLOG + 4)
    for k = 0, n - 1 do local a = SHLOG + 8 + 16 * k
      if mem:read_u8(a + 4) == 1 then print("BEGIN", mem:read_u32(a), mem:read_u8(a + 7)) end end
    manager.machine:exit() end
end)

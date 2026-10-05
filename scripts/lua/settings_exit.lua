-- MAME: the game program (tests/game PLAY=1) through the settings screen: Coin + B2 held 75 frames opens it, the test
-- switch at frame 200 saves and restarts; snapshots of the screen (150), and of the program after the restart (500, 900)
local screen = manager.machine.screens[":screen"]
local P = manager.machine.ioport.ports
local ev = { {30, ":INPUTS", "Coin 1", 75}, {30, ":INPUTS", "P1 Strong Punch", 75}, {200, ":INPUTS", "Service Mode", 2} }
emu.register_frame_done(function()
  local f = screen:frame_number()
  for _, e in ipairs(ev) do
    local fld = P[e[2]].fields[e[3]]
    if f == e[1] then fld:set_value(1) elseif f == e[1] + e[4] then fld:set_value(0) end
  end
  if f == 150 or f == 500 or f == 900 then manager.machine.video:snapshot() end
  if f == 900 then manager.machine:exit() end
end)

-- MAME: snapshots at the screen frames listed in SNAP_FRAMES (comma-separated, ascending), then exit
local want = {}
for n in string.gmatch(os.getenv("SNAP_FRAMES") or "", "%d+") do want[#want + 1] = tonumber(n) end
local screen = manager.machine.screens[":screen"]
local k = 1
emu.register_frame_done(function()
  local f = screen:frame_number()
  if k <= #want and f >= want[k] then
    print(string.format("snap %d at frame %d", k, f))
    manager.machine.video:snapshot()
    k = k + 1
    if k > #want then manager.machine:exit() end
  end
end)

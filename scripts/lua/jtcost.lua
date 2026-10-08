-- MAME (-nodrc -debug -debugger none): debugger traces of single play steps of a tests/playsh2 build for
-- tools/jtcost.py (scripts/jtcost.sh). A window opens when the program writes its record count (0x04100008,
-- tests/playsh2/main.c) equal to its start: "trace <file>,maincpu,noloop" logging r0-r15 and PR on every
-- instruction. On the next entry of play_step (JTC_STEP, from sh-elf-nm) the return address (PR) and the stack
-- pointer are read, and a breakpoint on that return address with that r15 turns the trace off. MAME exits after
-- the last window.
--   JTC_STEP  play_step's address (hex)
--   JTC_WIN   windows "start:file,start:file,...", starts ascending (the record count before the traced step)
--   JTC_TAP   the count's address (hex; default 0x04100008, tests/playsh2's record count). scripts/jtcost_draw.sh: the
--             game program's marker steps (0x02000050), JTC_STEP draw_frame: the draw of the step after the count
local cpu = manager.machine.devices[":maincpu"]
local space = cpu.spaces["program"]
local ops = cpu.spaces["decrypted_opcodes"]
local dbg = manager.machine.debugger
local STEP = tonumber(os.getenv("JTC_STEP"), 16)
local wins = {}
for s, f in os.getenv("JTC_WIN"):gmatch("(%d+):([^,]+)") do wins[#wins + 1] = { tonumber(s), f } end
local act = '{tracelog "%X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X|",' ..
            'r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13,r14,r15,pr}'
local w, state = 1, 0            -- state 0: waiting for the start count, 1: trace on, before play_step, 2: in it
local etap = nil

local function entry(offset, data, mask)
  local a = offset
  if mask == 0xffff then a = offset + 2 end
  if state ~= 1 or a ~= STEP then return end
  local pr, sp = cpu.state["PR"].value, cpu.state["R15"].value
  dbg:command(string.format("bp %X,r15==%X,{trace off,maincpu;bpclear;g}", pr, sp))
  print(string.format("jtcost: window %d: play_step entered, return %08X, r15 %08X", w, pr, sp))
  state = 2
end

local TAP = tonumber(os.getenv("JTC_TAP") or "04100008", 16)
local function open(win)
  dbg:command("trace " .. win[2] .. ",maincpu,noloop," .. act)
  etap = ops:install_read_tap(STEP & ~3, (STEP & ~3) + 3, "jtce", entry)
  state = 1
end
ctap = space:install_write_tap(TAP, TAP + 3, "jtc", function(offset, data, mask)
  if w > #wins then return end
  local win = wins[w]
  if state == 0 and data == win[1] then
    open(win)
  elseif state ~= 0 and data == win[1] + 1 then   -- the traced step's record: play_step has returned
    if state == 1 then print("jtcost: window " .. w .. ": play_step not entered"); dbg:command("trace off,maincpu") end
    if etap then etap:remove(); etap = nil end
    print("jtcost: window " .. w .. " done")
    state = 0
    w = w + 1
    -- consecutive records (the next window starts at this count): open it on this same write
    if w <= #wins and data == wins[w][1] then open(wins[w]) end
  end
end)
emu.register_periodic(function() if w > #wins then manager.machine:exit() end end)

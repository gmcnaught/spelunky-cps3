#!/usr/bin/env python3
# A virtual keyboard on the MiSTer (/dev/uinput, no evdev module needed) that Main_MiSTer passes to the core as
# PS/2 keys (JTFRAME: arrows, LCtrl/LAlt/Space/LShift/Z/X = B1-B6, 1 = Start 1, 5 = Coin 1, F2 = test).
# Runs on the MiSTer: commands on stdin, one a line:
#   down <key>... / up <key>... / tap <key>... [ms] / sleep <s> / shot (a MiSTer screenshot) / quit
# keys: up down left right b1..b6 start coin test esc enter, or a Linux KEY_ code number.
# From the host (stdin carries the commands, so the script is copied first):
#   scp scripts/mister_keys.py root@<mister>:/tmp/
#   printf 'tap coin\nsleep 1\ntap start\n' | ssh root@<mister> python3 /tmp/mister_keys.py
# Used for docs/ARCADE.md section 7's jtcps3 capture check (coin, start, down + B5 bomb, F2, GAME CAPTURE).
import fcntl, os, struct, sys, time

KEYS = dict(up=103, down=108, left=105, right=106, b1=29, b2=56, b3=57, b4=42, b5=44, b6=45,
            start=2, coin=6, test=60, esc=1, enter=28)
UI_SET_EVBIT, UI_SET_KEYBIT, UI_DEV_CREATE, UI_DEV_DESTROY = 0x40045564, 0x40045565, 0x5501, 0x5502
EV_SYN, EV_KEY = 0, 1

fd = os.open('/dev/uinput', os.O_WRONLY | os.O_NONBLOCK)
fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
fcntl.ioctl(fd, UI_SET_EVBIT, EV_SYN)
for k in range(1, 128):
    fcntl.ioctl(fd, UI_SET_KEYBIT, k)
# struct uinput_user_dev: name[80], input_id (4 x u16), ff_effects_max, absmax/min/fuzz/flat[64] each
os.write(fd, struct.pack('80sHHHHi', b'cps3 test keyboard', 3, 0x1234, 0x5678, 1, 0) + b'\0' * (4 * 64 * 4))
fcntl.ioctl(fd, UI_DEV_CREATE)
time.sleep(1.5)                       # Main_MiSTer opens new devices on inotify

def ev(t, c, v):
    s = time.time()
    os.write(fd, struct.pack('llHHi', int(s), int((s % 1) * 1e6), t, c, v))

def key(names, v):
    for n in names:
        ev(EV_KEY, KEYS[n] if n in KEYS else int(n), v)
    ev(EV_SYN, 0, 0)

for line in sys.stdin:
    a = line.split()
    if not a: continue
    if a[0] == 'quit': break
    if a[0] == 'sleep': time.sleep(float(a[1]))
    elif a[0] == 'down': key(a[1:], 1)
    elif a[0] == 'up': key(a[1:], 0)
    elif a[0] == 'tap':
        ms = 100
        if a[-1].isdigit() and a[-1] not in KEYS: ms = int(a[-1]); a = a[:-1]
        key(a[1:], 1); time.sleep(ms / 1000); key(a[1:], 0)
    elif a[0] == 'shot':
        os.system("timeout 5 sh -c 'echo screenshot > /dev/MiSTer_cmd'")
    sys.stdout.write('ok %s\n' % line.strip()); sys.stdout.flush()
time.sleep(0.2)
fcntl.ioctl(fd, UI_DEV_DESTROY)
os.close(fd)

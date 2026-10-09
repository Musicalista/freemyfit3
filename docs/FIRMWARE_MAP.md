# AZA3 firmware map (Galaxy Fit3, SM-R390 / R390XXU0AZA3)

Findings from **static** reverse engineering of the main image (`best1502x_b319_user.bin`, Bestechnic BES1502x SoC, Cortex-M4F, Thumb-2).
Execution (XIP) addresses: file offset `0` = `0x2c080000`. All of this is for interoperability and research; **no firmware is included in this repository**.
Names below are descriptive (the image is not stripped of log strings, not of symbols); addresses are of the AZA3 build only. Everything marked *(unverified)* comes from reading code, not from running it.

## FWD package (what the flasher sends)
`0x7e5a5a60` | source `R390XXU0AZA3` | updater | package `0x7e5a5a5a` with 73 ZIP components (each with a CRC32).
Main image: header at `0x1000` (`0x11223344 0xdeaddead`, CRC32 at `0x1008`, size at `0x100c`). No cryptographic signature was observed: the CRC and the flasher's validator are enough.
The flasher's validator limits the main image to `0x380000` (3.5 MB). Total flash: `0x400000` (4 MB).

## Free area used for injected code
`0x2c3a0522` .. `+0xf3fe` (file offset `0x320522`, ~62 KB of zeros). Injected code lives there. Flash is read-only: **no writable globals**.

## Useful functions (Thumb; call with bit 0 set)
| Address | Function |
|---|---|
| `0x2c2974f8` | log(level, tag, fmt, ...) |
| `0x2c0eb00c` / `0x2c0eb018` | malloc / free (pool 2) |
| `0x2c1423f8` | motor_once_ctrl(id, 0) (vibration; pattern table at `0x2c2e8b74`) |
| `0x2c112f58` | send_reply(seq_id, text, len) (quick reply -> phone) |
| `0x2c0a76d8` | tp_sample_get(sample): state at `+1` (1/2 touching), x at `+4`, y at `+6` (256x402) |
| `0x2c288ef4` | lv_tick_get |
| `0x2c28a3f0` / `0x2c28a448` | lv_timer_create(cb, period, user_data) / lv_timer_del (user_data at `timer+0xC`, callback at `+8`) |
| `0x201345d8` (RAM) | LVGL timer list (`lv_ll_t`: `[0]` node size, `[1]` head; node `next` pointer at `node + size + 4`) |
| `0x2c2905ac` / `0x2c29064c` | lv_img_create(parent) / lv_img_set_src(img, src) |
| `0x2c27908c` | lv_obj_align_to(obj, base, align, x, y) |
| `0x2c277b2c` | lv_obj_set_size |
| `0x2c27cbb8` / `0x2c27cc5c` | lv_obj_add_flag / clear_flag |
| `0x2c27d9cc` | lv_obj_add_event_cb(obj, cb, filter, user_data) |
| `0x2c27d980/84/88/90/94` | lv_event_get_target / current_target / code / param / user_data |
| `0x2c2782a8` | lv_obj_invalidate |
| `0x200f73b8` (RAM) | VFS table: `[0]` open(path,"r"/"w") `[1]` read `[2]` write `[3]` close |
| `0x2c212160` | current UI language id (the byte at `0x200fafe0`; 68 = pt-BR, 52 = pt-PT, 30 = en, 13 = en-US) |

LVGL v8, 16-bit RGB565 colours. `lv_img_dsc_t.header = 4 | (w<<10) | (h<<21)`.

## Hook points (each replaces one `bl`)
* `0x2c1d0802`: `bl send_reply` in the quick-reply click handler (reply keyboard).
* `0x2c1b76a0`: `bl log` in `app_settings_tutorials_goto_sub_page` ("Extra apps" launcher).
* `0x2c1b3eb6` / `0x2c1b4b26`: the vibration settings pages (used by the probes).

## Notification record
Filled by `0x2c0ed1f8` in the click handler (at `sp+0xc0`). `seq_id` at `+0x124`; inner struct at `+0x120` (0x448 bytes): `app_id` at `+8`, **title** at `+0x119`, **body** at `+0x35c`.

## Bluetooth host stack (BES) *(unverified on hardware)*
The firmware has a complete classic-Bluetooth host stack (L2CAP, RFCOMM, A2DP, HFP, GATT) and **no IP stack** (no lwIP/TCP/UDP/DHCP/DNS strings). `[PSM_BNEP]` appears only as a
name in an L2CAP debug table, so there is no PAN client; `net/` adds one.
| Address | What |
|---|---|
| `0x2011c1d0 + id*0x124` | app Bluetooth device structs (id 0..1): `bd_addr[6]` at `+0`, "in use" byte at `+6` (`app_bt_get_device` = `0x2c231ccc`) |
| `0x2c2339e9` | `app_bt_call_func_in_bt_thread(p0, p1, p2, p3, func)`: runs `func` in the Bluetooth thread (request id 15, `BT_Custom_Func_req`) |
| `0x2c25fd51` | `l2cap_open(bd_addr*, psm, mode, mtu, event_cb, data_cb)` -> channel* (`chan+0xc` = handle, `+0x96` = state, 9 = open) |
| `0x2c25d4c4` | `l2cap_register(psm, mode, cb, ctx)` (incoming PSM handlers, table at `0x2012d1dc`) |
| `0x2c2607f5` | `l2cap_send_data_auto_fragment(handle, data, len, tag)` -> 0 on success |
| `0x2c25ee45` | `l2cap_channel_close(?, channel*, reason)` |
| `0x2c250541` | data pointer of a packet buffer (`u16` length at `+8`) |
Event callback: `cb(mgr, event, handle, conn+8, 0)` with event 1 = outbound channel open, 2 = incoming connection indication, 3 = TX done, 4 = closed.
Data callback: `cb(mgr, handle, packet_buffer*)`.

## Serial Bluetooth protocol (the flasher's)
Service UUID `db764ac8-4b08-7f25-aafe-59d03c27bae3`.
* Write a file: `300` -> `33bin,<path>,<size>` -> blocks of 39600 B + CRC32 LE (each acknowledged by `310`) -> `32` -> `34`.
* Read a file: `061<path>\0` (answer: `0x40` + u32 BE size), then `062` per block (`0x40`, u32 BE n, n bytes, 4 checksum bytes); a last `062` ends it.
* Factory AT commands: `00AT^NAME=arg1=arg2` (no terminator). The dispatcher (`0x2c143f50`) splits on `=`, looks the name up in a table of 315 `{name, handler}`
  entries at `0x2c2ecdcc`, and calls `handler(arg1, arg2)`; handlers read decimal numbers with `atoi`. Replies are plain text such as `OK\r\n`.
  Examples: `SCREENBRIGHT=20|40|60|80|100`, `LANGUAGE=<0..68>`, `MOTOR_VIB=on|off`, `NOTICE=-a=<text>` or `NOTICE=-at=<type>,<text>`,
  `GET_DEVINFO`, `SWVER`, `BTMAC`, `GETBATPERCENT`, `GETVBAT`, `REBOOT`. The table also holds destructive commands (`FORMAT`, `SHIPMODE`, `POWEROFF`, `DELLBTADDR`, ...);
  `MEMORY_READ/WRITE` and `JUMP` are disabled stubs in production firmware.

## Display and touch
ICNA3311 LCD (QSPI), 256x402; LCDC at `0x40200000`. Touch through `tp_sample_get` (hyn/chsc/stx/ztw drivers underneath).

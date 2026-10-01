# Hardware Bring-Up & Integration

How the MSP430FR6989 was wired into this project end-to-end — toolchain, drivers, the
gotchas, and the exact commands. Written down so the next bring-up takes 10 minutes
instead of an afternoon.

## The setup

- **Board:** MSP-EXP430FR6989 LaunchPad (onboard eZ-FET debugger).
- **Host PC:** Windows 10, USB-C only, so the LaunchPad (USB-A cable) connects through a
  Dell Thunderbolt dock. The dock works — but see the driver note, it muddied the first attempt.
- **Extras:** onboard red LED on `P1.0` used as a heartbeat (3× boot blink, then
  ~1 Hz while streaming). The firmware is LCD-free — temperature goes out over
  serial, not to the segment display.
- **Toolchain:** Code Composer Studio (CCS) installed on `D:\ccs`, using the bundled TI
  MSP430 compiler `ti-cgt-msp430_21.6.3.LTS`. No `msp430-gcc`/`mspdebug` needed — CCS does
  compile + flash + drivers in one.

## Toolchain decision

The firmware is register-level FR6989 C (ADC12_B, eUSCI). The old `mspgcc` that
PlatformIO/Energia bundle doesn't have FR6989 device support, so those are out. The two real
options were TI `msp430-gcc` + `mspdebug` (CLI, matches `firmware/Makefile`) or CCS. CCS won
because it ships the eZ-FET drivers and FR6989 support together — less to install, less to break.

## CCS project

CCS wants its own project (`.project` / `.cproject`), which this repo didn't have. What worked:

1. **New CCS Project** → device `MSP430FR6989`, connection `TI MSP430 USB1`, template
   **Empty Project** (not "with main.c" — that stub `main()` collides with ours).
2. The new-project wizard wouldn't honor a custom location, so it landed in the default
   workspace. **Mind this** — it means CCS builds a copy *outside* the repo, so you must
   edit the copy in the workspace, not just `firmware/main.c`, or they silently drift.
3. Dropped the real `main.c` (from `firmware/` in the repo) into the project folder,
   overwriting the 123-byte stub `main.c`. The firmware is a single file — no LCD driver.
4. **F5** to refresh, **Ctrl+B** to build.

Build was clean (0 errors). The ULP power-advisor remarks and the FRAM `LOCKLPM5` warning
(#10420) are informational — `main.c` already clears `LOCKLPM5`, so that one's handled.

## The driver gotcha (this is the one that cost time)

Clicking **Debug (🐞)** failed with:

```
MSP430: Error initializing emulator: No USB FET was found
```

The board *was* plugged in and enumerating — it showed two COM ports — but Windows had bound
both interfaces to its **generic `usbser`** driver ("USB Serial Device") instead of TI's. CCS's
debugger can't speak to the FET over the generic driver, hence "no FET."

Confirmed with:

```powershell
Get-PnpDevice | ? { $_.InstanceId -match 'VID_2047' } |
  Select Status, Class, FriendlyName, InstanceId
# -> VID_2047 PID_0013, MI_00 and MI_02, both "USB Serial Device", driver = usbser (Microsoft)
```

TI's matching driver ships inside CCS at:

```
D:\ccs\ccs_base\emulation\drivers\msp430\USB_CDC\msp430tools.inf
```

That INF also settles which interface is which (and it's the opposite of what you'd guess):

| USB interface | Role                    | Enumerated as (example) |
|---------------|-------------------------|-------------------------|
| `MI_00`       | **MSP Debug Interface** | COM5 / COM7 / …         |
| `MI_02`       | **MSP Application UART1**| COM4 / COM6 / …         |

So **CCS debugs over the Debug Interface, and telemetry streams on Application UART1.**

> ⚠️ **The COM numbers are not fixed** — Windows assigns them per USB port, so
> they change when you replug into a different port or dock. Don't hard-code
> them; re-check each session with:
> ```powershell
> Get-PnpDevice -Class Ports -PresentOnly | Select Status, FriendlyName, InstanceId
> ```
> Match by the **friendly name** ("Application UART1" vs "Debug Interface"), not
> the number. On 2026-10-01, for instance, they came up as COM6 (UART) and COM7 (debug).

### Fix

Windows said "best driver already installed" and refused to swap on a plain browse, so you force it:

1. Trust TI's cert, in an **Administrator** `cmd`:
   ```bat
   cd /d "D:\ccs\ccs_base\emulation\drivers\msp430\USB_CDC"
   installCerts.bat        :: = certutil -addstore -f "TrustedPublisher" "USB_CDC_CERT.p7b"
   ```
2. **Device Manager → Ports (COM & LPT)** → right-click each port → **Update driver** →
   **Browse** → **Let me pick from a list** → **Have Disk…** → point at `msp430tools.inf` →
   pick **MSP Debug Interface** and **MSP Application UART1** (match by name, not number).

After that they show by their real names, and **Debug (🐞)** finds the FET.

> Note: the TI driver is only needed to **flash/debug**. Reading telemetry off the
> Application UART1 port works fine on Windows' generic `usbser` driver too.

## Flash & run

- **Debug (🐞 / F11)** → compiles, flashes the `.out` over USB, halts at `main`.
- **Resume (F8)** → firmware runs. You should see the red LED (P1.0) flash 3× then
  blink steadily at ~1 Hz, and frames going out on the Application UART1 port. Flash
  is non-volatile — it keeps running after you terminate the debug session.
  - If the LED flashes 3× then freezes (no steady blink) and nothing streams, the
    program stalled in init or trapped to `exit.c`. The usual cause is CCS flashing
    the wrong `main.c` — see the workspace note below.

Sanity-check the stream without the full host (the port must be free — one owner at a
time). Use *your* Application UART1 number from the `Get-PnpDevice` check above:

```python
import serial, time
with serial.Serial("COM6", 9600, timeout=0.5) as s:   # <-- your Application UART1 port
    end = time.time() + 4; buf = b""
    while time.time() < end: buf += s.read(256)
print(buf.decode(errors="replace"))
# expect lines like:  $0,TEMP,24.3*5C
```

## Bring up the dashboard

Host deps (fastapi, uvicorn, pyserial) are already on the global Python, so no venv:

```bash
cd host
python host.py --port COM6      # your Application UART1 port, NOT the Debug Interface
```

Then open the dashboard and click **Device → Start**. The host reads the UART, validates
the XOR checksum, stores to SQLite, and broadcasts over WebSocket; the chart fills live.

## The CCS workspace gotcha (cost the most time)

CCS's new-project wizard creates the project in its **own workspace** (e.g.
`C:\Users\<you>\workspace_ccstheia\telemetry-firmware`), *not* in this repo. It's
easy to edit the repo's `firmware/main.c` and keep flashing the workspace's stale
copy — which may still be the **123-byte empty template stub** whose `main()` just
returns into `exit.c`. Symptom: LED boot-blinks then freezes, zero bytes on the UART.
Fix: copy this repo's `firmware/main.c` over the workspace `main.c`, then **F5**
(refresh) → **Ctrl+B** (rebuild) → **Debug** → **Resume** in CCS.

## Quick troubleshooting

- **"No USB FET was found"** → driver gotcha above (generic `usbser` instead of TI).
- **Nothing on the UART, LED frozen after boot blink** → flashing the stub `main.c`;
  see the workspace gotcha above.
- **UART silent, LED blinking normally** → firmware paused at `main` (press **Resume
  (F8)**), or wrong port — telemetry is Application UART1, not the Debug Interface.
- **"Access denied" on the port** → something else owns it (another serial monitor, or a
  second host). One process per port.
- **Garbled bytes** → baud mismatch; firmware is **9600 8N1**.

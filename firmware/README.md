# Firmware — MSP430FR6989 telemetry node

Embedded C for the **MSP-EXP430FR6989 LaunchPad**. Every ~500 ms it samples the
internal temperature sensor and the supply-voltage monitor, shows the rounded
temperature on the on-board segment LCD, and streams a framed `TEMP` and `VOLT`
reading over the USB backchannel UART, blinking the on-board red LED as a heartbeat.

Deliberately minimal: a plain polled loop on the internal DCO — no crystal, no
timers, no low-power modes. The LCD is clocked from ACLK (= internal VLO), so
bring-up stays deterministic (there is no oscillator to fault on), and the hot
path is trap-free (no `sprintf`, which overflows the small default stack here).

## What it does

| Layer | Detail |
|---|---|
| Temp | Internal temp sensor (A30) via ADC12_B, 1.2 V reference, factory TLV calibration |
| Supply | Supply-voltage monitor (A31 = AVCC/2) via ADC12_B, 2.5 V reference — no extra hardware |
| Display | On-board FH-1138P segment LCD (`hal_LCD.c`), rounded whole degrees, clocked from ACLK = VLO |
| Link | eUSCI_A1 UART, **9600 8N1**, on P3.4 (TX) / P3.5 (RX) — the eZ-FET "Application UART1" |
| Heartbeat | On-board red LED (P1.0): 3× boot blink at reset, then ~1 Hz while the loop runs |
| Clock | Internal DCO ~1 MHz drives MCLK + SMCLK; VLO (~9.4 kHz) drives ACLK; no external crystal |
| Frame | `$<seq>,TEMP,<value>*<CS>` and `$<seq>,VOLT,<value>*<CS>`, CRLF-terminated — see [../PROTOCOL.md](../PROTOCOL.md) |

## Files

```
main.c        clocks, UART, ADC, framing, main loop (register-level, no driverlib)
hal_LCD.c/.h  segment-LCD driver (register-level)
Makefile      build/flash with msp430-gcc + mspdebug
```

## Build & flash

**Option A — Code Composer Studio (easiest on Windows)**
1. *File → New → CCS Project*, device `MSP430FR6989`, empty project.
2. Drop this folder's `main.c`, `hal_LCD.c`, `hal_LCD.h` into the project,
   overwriting the generated stub `main.c`. **Note:** the CCS wizard creates the
   project in its own workspace, *not* in this repo — so edit the copy CCS
   actually builds, or they'll drift apart. After adding files, press **F5** to
   refresh so the managed build discovers them.
3. Plug in the LaunchPad, click **Debug** (build + flash), then **Resume (F8)**.

**Option B — command line (msp430-gcc + mspdebug)**
```
set MSP430_GCC_INCLUDE_DIR=C:\ti\msp430-gcc\include   # your install path
make          # -> telemetry.out
make flash    # programs the board over USB
```

## Verify it's running

- The red LED (P1.0) flashes 3× at reset, then blinks steadily at ~1 Hz. A
  steady blink means execution reached the main loop; a frozen LED means it
  stalled in init or trapped out to `exit.c`.
- Open the backchannel COM port at 9600 baud (PuTTY / `screen`) and you should
  see alternating temperature and supply-voltage frames:
  ```
  $0,TEMP,22.4*26
  $1,VOLT,3.30*2E
  $2,TEMP,22.5*25
  $3,VOLT,3.30*2C
  ```
Then hand that COM port to the Python host (`../host`).

## If nothing appears

- **LED never settles into a steady blink:** the program stalled in init or
  trapped to `exit.c`. Pause in CCS and read the top of the call stack —
  `exit.c` means a crash/return (e.g. flashing the empty template stub, or a
  `sprintf` stack overflow); a spin in `uart_putc` means a UART clock/config issue.
- **Wrong COM / no lines:** the board shows *two* COM ports — use "Application
  UART1", not "Debug Interface". Confirm the backchannel UART is `eUSCI_A1`
  (P3.4/P3.5) on your board revision; some LaunchPads use `A0`.
- **Garbage characters:** baud mismatch — the host and terminal must both be 9600.
- **Temperature way off:** the TLV calibration addresses (`0x1A1A` / `0x1A1C`) are device-specific; verify against the FR6989 datasheet if readings look wrong.

# Firmware — MSP430FR6989 telemetry node

Embedded C for the **MSP-EXP430FR6989 LaunchPad**. Samples the internal
temperature sensor every ~500 ms and streams a framed reading over the USB
backchannel UART, blinking the on-board red LED as a heartbeat.

Deliberately minimal: a plain polled loop on the internal DCO — no LCD, no
crystal, no timers or low-power modes. That keeps bring-up deterministic (there
is no oscillator to fault on) and the hot path trap-free (no `sprintf`, which
overflows the small default stack on this part).

## What it does

| Layer | Detail |
|---|---|
| Sensor | Internal temp sensor via ADC12_B, 1.2 V reference, factory TLV calibration |
| Link | eUSCI_A1 UART, **9600 8N1**, on P3.4 (TX) / P3.5 (RX) — the eZ-FET "Application UART1" |
| Heartbeat | On-board red LED (P1.0): 3× boot blink at reset, then ~1 Hz while the loop runs |
| Clock | Internal DCO ~1 MHz drives MCLK + SMCLK; no external crystal |
| Frame | `$<seq>,TEMP,<value>*<CS>\r\n` — see [../PROTOCOL.md](../PROTOCOL.md) |

## Files

```
main.c        clocks, UART, ADC, framing, main loop (register-level, no driverlib)
Makefile      build/flash with msp430-gcc + mspdebug
```

## Build & flash

**Option A — Code Composer Studio (easiest on Windows)**
1. *File → New → CCS Project*, device `MSP430FR6989`, empty project.
2. Replace the generated stub `main.c` with this folder's `main.c`. **Note:** the
   CCS wizard creates the project in its own workspace, *not* in this repo —
   so edit the copy CCS actually builds, or they'll drift apart.
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
- Open the backchannel COM port at 9600 baud (PuTTY / `screen`) and you should see:
  ```
  $0,TEMP,22.4*1B
  $1,TEMP,22.5*1A
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

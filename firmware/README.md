# Firmware — MSP430FR6989 telemetry node

Embedded C for the **MSP-EXP430FR6989 LaunchPad**. Samples the internal
temperature sensor every 500 ms, shows the rounded value on the on-board
segment LCD, and streams a framed reading over the USB backchannel UART.
The CPU sleeps in **LPM3** between samples.

## What it does

| Layer | Detail |
|---|---|
| Sensor | Internal temp sensor via ADC12_B, 1.2 V reference, factory TLV calibration |
| Display | On-board FH-1138P segment LCD (`hal_LCD.c`), whole degrees |
| Link | eUSCI_A1 UART, **9600 8N1**, on P3.4 (TX) / P3.5 (RX) |
| Power | Timer_A wakes from LPM3 every 500 ms; ACLK from the 32 kHz crystal |
| Frame | `$<seq>,TEMP,<value>*<CS>\r\n` — see [../PROTOCOL.md](../PROTOCOL.md) |

## Files

```
main.c        clocks, UART, ADC, timer, framing, main loop
hal_LCD.c/.h  segment-LCD driver (register-level)
Makefile      build/flash with msp430-gcc + mspdebug
```

## Build & flash

**Option A — Code Composer Studio (easiest on Windows)**
1. *File → New → CCS Project*, device `MSP430FR6989`, empty project.
2. Add `main.c`, `hal_LCD.c`, `hal_LCD.h` to the project.
3. Plug in the LaunchPad, click **Debug** (build + flash), then **Run**.

**Option B — command line (msp430-gcc + mspdebug)**
```
set MSP430_GCC_INCLUDE_DIR=C:\ti\msp430-gcc\include   # your install path
make          # -> telemetry.out
make flash    # programs the board over USB
```

## Verify it's running

- The LCD shows the current temperature in °C.
- Open the backchannel COM port at 9600 baud (PuTTY / `screen`) and you should see:
  ```
  $0,TEMP,22.4*1B
  $1,TEMP,22.5*1A
  ```
Then hand that COM port to the Python host (`../host`).

## If nothing appears

- **Wrong COM / no lines:** confirm the backchannel UART is `eUSCI_A1` (P3.4/P3.5) on your board revision; some LaunchPads use `A0`. Adjust `init_uart()` and the `P3SEL0` pins if so.
- **Garbage characters:** baud mismatch — the host and terminal must both be 9600.
- **Temperature way off:** the TLV calibration addresses (`0x1A1A` / `0x1A1C`) are device-specific; verify against the FR6989 datasheet if readings look wrong.
- **Blank/scrambled LCD:** the 32 kHz crystal didn't start, or your board's segment map differs — see the note atop `hal_LCD.h`.

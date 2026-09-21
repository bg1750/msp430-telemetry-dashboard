# Wire Protocol

The serial protocol between the MSP430 firmware and the Python host. Deliberately simple, ASCII, and checksummed so corruption is detectable — the same style as NMEA.

## Physical layer

| Setting | Value |
|---|---|
| Transport | UART over the LaunchPad's USB backchannel |
| Baud | 9600 |
| Format | 8 data bits, no parity, 1 stop bit (8N1) |

## Frame format

One reading per line, ASCII, terminated with CRLF:

```
$<seq>,<TAG>,<value>*<CS>\r\n
```

| Field | Meaning | Example |
|---|---|---|
| `$` | Start-of-frame marker | `$` |
| `<seq>` | Unsigned frame counter, wraps at 65535 | `142` |
| `<TAG>` | Sensor tag (`TEMP`, `LIGHT`, `VOLT`) | `TEMP` |
| `<value>` | Reading, fixed-point decimal | `22.45` |
| `*` | End-of-payload marker | `*` |
| `<CS>` | Checksum: XOR of every byte **between** `$` and `*`, as two uppercase hex digits | `3F` |
| `\r\n` | Frame terminator | |

### Example

```
$142,TEMP,22.45*1B
```

The checksum is computed over `142,TEMP,22.45` (not including `$` or `*`).

## Error handling

The host validates every frame and **silently drops** anything that fails, incrementing a counter it logs periodically:

| Condition | Host action |
|---|---|
| Line doesn't start with `$` or has no `*` | Drop (framing error) |
| Checksum mismatch | Drop (corruption) |
| Unknown TAG or unparseable value | Drop (format error) |
| Partial line (device unplugged mid-frame) | Drop; reconnect loop continues |

This is the "handle the ugly cases" requirement: unplug the board mid-stream or inject a bad byte and the host keeps running, counting the bad frames instead of crashing.

## Downstream (host → dashboard)

The host re-emits each valid frame as JSON over a WebSocket:

```json
{ "seq": 142, "sensor": "temp", "unit": "°C", "value": 22.45, "t": 1758326400123 }
```

`t` is the host's receive timestamp in epoch milliseconds.

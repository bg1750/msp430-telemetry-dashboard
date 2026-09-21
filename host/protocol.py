"""
protocol.py — pure frame parsing for the MSP430 wire protocol.

Kept dependency-free (no serial/fastapi) so it can be unit-tested on its own.
See ../PROTOCOL.md for the format:  $<seq>,<TAG>,<value>*<CS>
"""

TAG_MAP = {"TEMP": ("temp", "°C"), "LIGHT": ("light", "lux"), "VOLT": ("voltage", "V")}


def checksum(body: str) -> int:
    """XOR of every byte in the payload (the part between '$' and '*')."""
    cs = 0
    for ch in body:
        cs ^= ord(ch)
    return cs


def parse_frame(line: str):
    """Parse one frame line -> dict {seq, sensor, unit, value}, or None if invalid."""
    line = line.strip()
    if not line.startswith("$") or "*" not in line:
        return None
    body, _, cs = line[1:].partition("*")

    try:
        if checksum(body) != int(cs, 16):
            return None  # corruption
    except ValueError:
        return None

    parts = body.split(",")
    if len(parts) != 3:
        return None
    seq_s, tag, val_s = parts
    if tag not in TAG_MAP:
        return None
    try:
        seq, value = int(seq_s), float(val_s)
    except ValueError:
        return None

    sensor, unit = TAG_MAP[tag]
    return {"seq": seq, "sensor": sensor, "unit": unit, "value": value}

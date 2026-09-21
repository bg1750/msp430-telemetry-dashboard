"""Unit tests for the frame parser. Run: pytest test_host.py"""
from protocol import parse_frame


def framed(body: str) -> str:
    """Build a valid '$<body>*<CS>' line for a given payload body."""
    cs = 0
    for ch in body:
        cs ^= ord(ch)
    return f"${body}*{cs:02X}"


def test_valid_temp_frame():
    r = parse_frame(framed("142,TEMP,22.45"))
    assert r == {"seq": 142, "sensor": "temp", "unit": "°C", "value": 22.45}


def test_valid_frame_with_crlf():
    assert parse_frame(framed("1,TEMP,20.0") + "\r\n") is not None


def test_corrupted_checksum_is_rejected():
    good = framed("5,TEMP,21.0")
    bad = good[:-2] + "00"  # clobber the checksum digits
    assert parse_frame(bad) is None


def test_flipped_bit_in_payload_is_rejected():
    # Same checksum, different value -> mismatch.
    line = "$5,TEMP,99.0*" + framed("5,TEMP,21.0").split("*")[1]
    assert parse_frame(line) is None


def test_unknown_tag_is_rejected():
    assert parse_frame(framed("1,PRESSURE,10.0")) is None


def test_missing_start_marker_is_rejected():
    assert parse_frame("142,TEMP,22.45*1B") is None


def test_partial_line_is_rejected():
    assert parse_frame("$142,TEM") is None


def test_non_numeric_value_is_rejected():
    assert parse_frame(framed("1,TEMP,warm")) is None


def test_light_and_voltage_tags_map():
    assert parse_frame(framed("2,LIGHT,410"))["sensor"] == "light"
    assert parse_frame(framed("3,VOLT,3.3"))["unit"] == "V"

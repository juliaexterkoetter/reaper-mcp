import json
import math

import pytest
from pydantic import ValidationError

from reaper_mcp.bridge.errors import BridgeError
from reaper_mcp.bridge.protocol import Discovery, decode_response, encode_request
from reaper_mcp.units import db_to_gain, gain_to_db


@pytest.mark.parametrize("db", [-150, -60, -3, 0, 6, 24])
def test_db_roundtrip(db):
    assert gain_to_db(db_to_gain(db)) == pytest.approx(db)


def test_silence():
    assert db_to_gain(None) == 0
    assert gain_to_db(0) is None


@pytest.mark.parametrize("db", [math.nan, math.inf, -151, 25])
def test_invalid_db(db):
    with pytest.raises(ValueError):
        db_to_gain(db)


def test_protocol():
    request = json.loads(encode_request("tracks.list", {}, "secret", "123"))
    assert request["jsonrpc"] == "2.0"
    assert decode_response(b'{"jsonrpc":"2.0","id":"123","result":[]}\n', "123") == []


@pytest.mark.parametrize(
    "data",
    [
        b"{}\n",
        b"[]\n",
        b"garbage\n",
        b'{"jsonrpc":"2.0","id":"wrong","result":0}\n',
        b'{"jsonrpc":"2.0","id":"123","result":NaN}\n',
    ],
)
def test_bad_response(data):
    with pytest.raises(BridgeError, match="INVALID_RESPONSE"):
        decode_response(data, "123")


def test_discovery_rejects_remote_host():
    with pytest.raises(ValidationError):
        Discovery(protocol_version=1, port=1234, pid=1, host="0.0.0.0")

import hashlib
import hmac
import math
import struct
import socket
import threading
import time
import os
import csv
import tempfile
from unittest.mock import MagicMock

import pytest

import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from server import (
    validate_sensor_data,
    recv_all,
    PACKET_MAGIC,
    PACKET_VERSION,
    DATA_FORMAT,
    DATA_SIZE,
    HMAC_SIZE,
    PACKET_SIZE,
    HMAC_KEY,
)


# --- validate_sensor_data ---

class TestValidateSensorData:
    def test_valid_readings(self):
        ok, msg = validate_sensor_data(25, 60, 1.5, 400.0, 1700000001)
        assert ok is True
        assert msg == ""

    def test_boundary_values(self):
        ok, _ = validate_sensor_data(-40, 0, 0.0, 0.0, 1700000000)
        assert ok is True
        ok, _ = validate_sensor_data(80, 100, 999.0, 999.0, 2000000000)
        assert ok is True

    def test_temp_too_low(self):
        ok, msg = validate_sensor_data(-41, 50, 1.0, 400.0, 1700000001)
        assert ok is False
        assert "Temperature" in msg

    def test_temp_too_high(self):
        ok, msg = validate_sensor_data(81, 50, 1.0, 400.0, 1700000001)
        assert ok is False
        assert "Temperature" in msg

    def test_humidity_too_low(self):
        ok, msg = validate_sensor_data(25, -1, 1.0, 400.0, 1700000001)
        assert ok is False
        assert "Humidity" in msg

    def test_humidity_too_high(self):
        ok, msg = validate_sensor_data(25, 101, 1.0, 400.0, 1700000001)
        assert ok is False
        assert "Humidity" in msg

    def test_nan_ch4(self):
        ok, msg = validate_sensor_data(25, 50, float('nan'), 400.0, 1700000001)
        assert ok is False
        assert "NaN" in msg

    def test_nan_co2(self):
        ok, msg = validate_sensor_data(25, 50, 1.0, float('nan'), 1700000001)
        assert ok is False
        assert "NaN" in msg

    def test_inf_ch4(self):
        ok, msg = validate_sensor_data(25, 50, float('inf'), 400.0, 1700000001)
        assert ok is False
        assert "Inf" in msg

    def test_inf_co2(self):
        ok, msg = validate_sensor_data(25, 50, 1.0, float('inf'), 1700000001)
        assert ok is False
        assert "Inf" in msg

    def test_negative_ch4(self):
        ok, msg = validate_sensor_data(25, 50, -0.1, 400.0, 1700000001)
        assert ok is False
        assert "Negative" in msg

    def test_negative_co2(self):
        ok, msg = validate_sensor_data(25, 50, 1.0, -1.0, 1700000001)
        assert ok is False
        assert "Negative" in msg

    def test_timestamp_too_low(self):
        ok, msg = validate_sensor_data(25, 50, 1.0, 400.0, 1699999999)
        assert ok is False
        assert "Timestamp" in msg

    def test_timestamp_too_high(self):
        ok, msg = validate_sensor_data(25, 50, 1.0, 400.0, 2000000001)
        assert ok is False
        assert "Timestamp" in msg


# --- recv_all ---

class TestRecvAll:
    def test_reassembles_chunks(self):
        chunks = [b'hel', b'lo', b' wor', b'ld']
        mock_sock = MagicMock()
        mock_sock.recv = MagicMock(side_effect=chunks)
        result = recv_all(mock_sock, 11)
        assert result == b'hello world'

    def test_single_recv(self):
        mock_sock = MagicMock()
        mock_sock.recv = MagicMock(return_value=b'abcdef')
        result = recv_all(mock_sock, 6)
        assert result == b'abcdef'

    def test_raises_on_closed_socket(self):
        mock_sock = MagicMock()
        mock_sock.recv = MagicMock(return_value=b'')
        with pytest.raises(ConnectionError):
            recv_all(mock_sock, 10)


# --- Packet construction helpers ---

def build_payload(seq=1, temp=25, humidity=60, ch4=1.5, co2=400.0,
                  timestamp=1700000001, magic=PACKET_MAGIC, version=PACKET_VERSION):
    return struct.pack(DATA_FORMAT, magic, version, 0, seq, temp, humidity, ch4, co2, timestamp)


def sign_payload(payload, key=HMAC_KEY):
    return hmac.new(key, payload, hashlib.sha256).digest()


class TestPacketConstruction:
    def test_payload_size(self):
        payload = build_payload()
        assert len(payload) == DATA_SIZE

    def test_packet_size(self):
        payload = build_payload()
        packet = payload + sign_payload(payload)
        assert len(packet) == PACKET_SIZE

    def test_valid_hmac_verification(self):
        payload = build_payload()
        mac = sign_payload(payload)
        expected = hmac.new(HMAC_KEY, payload, hashlib.sha256).digest()
        assert hmac.compare_digest(mac, expected)

    def test_tampered_payload_fails_hmac(self):
        payload = build_payload()
        mac = sign_payload(payload)
        tampered = bytearray(payload)
        tampered[0] ^= 0xFF
        expected = hmac.new(HMAC_KEY, bytes(tampered), hashlib.sha256).digest()
        assert not hmac.compare_digest(mac, expected)

    def test_wrong_key_fails_hmac(self):
        payload = build_payload()
        mac = sign_payload(payload)
        wrong_mac = hmac.new(b'wrong-key', payload, hashlib.sha256).digest()
        assert not hmac.compare_digest(mac, wrong_mac)

    def test_payload_unpacks_correctly(self):
        payload = build_payload(seq=42, temp=-5, humidity=80, ch4=3.14, co2=500.0, timestamp=1800000000)
        magic, version, reserved, seq, temp, humidity, ch4, co2, timestamp = struct.unpack(DATA_FORMAT, payload)
        assert magic == PACKET_MAGIC
        assert version == PACKET_VERSION
        assert seq == 42
        assert temp == -5
        assert humidity == 80
        assert abs(ch4 - 3.14) < 0.01
        assert abs(co2 - 500.0) < 0.01
        assert timestamp == 1800000000

    def test_wrong_magic_detected(self):
        payload = build_payload(magic=0x0000)
        magic, *_ = struct.unpack(DATA_FORMAT, payload)
        assert magic != PACKET_MAGIC

    def test_wrong_version_detected(self):
        payload = build_payload(version=99)
        _, version, *_ = struct.unpack(DATA_FORMAT, payload)
        assert version != PACKET_VERSION

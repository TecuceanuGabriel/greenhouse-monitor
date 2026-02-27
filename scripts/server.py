import socket
import struct
import datetime
import csv
import hashlib
import hmac
import logging
import math
import os
import threading

HOST = '0.0.0.0'
PORT = 1234
CSV_FILE = 'data_log.csv'

HMAC_KEY = os.environ.get('GH_HMAC_KEY', 'change-me-in-menuconfig').encode()

PACKET_MAGIC = 0x4748
PACKET_VERSION = 1
DATA_FORMAT = '<HBBIiiffq'  # little-endian, matches ESP32
DATA_SIZE = struct.calcsize(DATA_FORMAT)  # 32 bytes
HMAC_SIZE = 32
PACKET_SIZE = DATA_SIZE + HMAC_SIZE  # 64 bytes

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(threadName)s: %(message)s'
)
logger = logging.getLogger(__name__)

# track sequence numbers per client to detect gaps
last_seq = {}
seq_lock = threading.Lock()

csv_lock = threading.Lock()

# make sure the csv file exists with headers
if not os.path.isfile(CSV_FILE):
    with open(CSV_FILE, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Timestamp', 'Temperature (°C)', 'Humidity (%)', 'CH4 (ppm)', 'CO2 (ppm)'])


def recv_all(sock, length):
    """Read exactly length bytes from socket."""
    data = b''
    while len(data) < length:
        packet = sock.recv(length - len(data))
        if not packet:
            raise ConnectionError("Socket connection closed unexpectedly")
        data += packet
    return data


def validate_sensor_data(temp, humidity, ch4, co2, timestamp):
    """Sanity check sensor readings before logging."""
    if not (-40 <= temp <= 80):
        return False, f"Temperature out of range: {temp}"
    if not (0 <= humidity <= 100):
        return False, f"Humidity out of range: {humidity}"
    if math.isnan(ch4) or math.isnan(co2) or math.isinf(ch4) or math.isinf(co2):
        return False, f"NaN/Inf gas reading: CH4={ch4}, CO2={co2}"
    if ch4 < 0 or co2 < 0:
        return False, f"Negative gas reading: CH4={ch4}, CO2={co2}"
    if timestamp < 1700000000 or timestamp > 2000000000:
        return False, f"Timestamp out of range: {timestamp}"
    return True, ""


def handle_client(conn, addr):
    """Handle a single client connection in its own thread."""
    conn.settimeout(30)
    logger.info("Connected by %s", addr)

    with conn:
        while True:
            try:
                raw_data = recv_all(conn, PACKET_SIZE)
            except (ConnectionError, socket.timeout) as e:
                logger.info("Client %s disconnected: %s", addr[0], e)
                break

            payload = raw_data[:DATA_SIZE]
            received_hmac = raw_data[DATA_SIZE:]

            # verify HMAC (timing-safe comparison)
            expected_hmac = hmac.new(HMAC_KEY, payload, hashlib.sha256).digest()
            if not hmac.compare_digest(received_hmac, expected_hmac):
                logger.warning("HMAC verification failed from %s. Dropping packet.", addr[0])
                continue

            magic, version, reserved, seq, temp, humidity, ch4, co2, timestamp = \
                struct.unpack(DATA_FORMAT, payload)

            # validate protocol header
            if magic != PACKET_MAGIC:
                logger.warning("Invalid magic: 0x%04X from %s. Dropping packet.", magic, addr[0])
                continue

            if version != PACKET_VERSION:
                logger.warning("Unknown protocol version: %d from %s. Dropping packet.", version, addr[0])
                continue

            # validate sensor data
            valid, reason = validate_sensor_data(temp, humidity, ch4, co2, timestamp)
            if not valid:
                logger.warning("Invalid sensor data from %s: %s. Dropping packet.", addr[0], reason)
                continue

            # detect sequence gaps
            client_ip = addr[0]
            with seq_lock:
                if client_ip in last_seq:
                    expected = last_seq[client_ip] + 1
                    if seq != expected:
                        logger.warning("Sequence gap from %s: expected %d, got %d (%d packets lost)",
                                       client_ip, expected, seq, seq - expected)
                last_seq[client_ip] = seq

            readable = datetime.datetime.fromtimestamp(timestamp)
            time_str = readable.strftime('%Y-%m-%d %H:%M:%S')

            logger.info("[seq=%d] Temp: %d°C, Humidity: %d%%, CH4: %.2f ppm, CO2: %.2f ppm | %s",
                        seq, temp, humidity, ch4, co2, time_str)

            with csv_lock:
                with open(CSV_FILE, 'a', newline='') as f:
                    writer = csv.writer(f)
                    writer.writerow([time_str, temp, humidity, round(ch4, 2), round(co2, 2)])


with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, PORT))
    s.listen()
    logger.info("Listening on %s:%d", HOST, PORT)

    while True:
        conn, addr = s.accept()
        t = threading.Thread(target=handle_client, args=(conn, addr), daemon=True)
        t.start()

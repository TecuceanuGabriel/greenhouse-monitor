import socket
import struct
import datetime
import csv
import os

HOST = '0.0.0.0'
PORT = 1234
CSV_FILE = 'data_log.csv'

PACKET_MAGIC = 0x4748
PACKET_VERSION = 1
PACKET_FORMAT = '<HBBIiiffq'  # little-endian, matches ESP32
PACKET_SIZE = struct.calcsize(PACKET_FORMAT)  # 32 bytes

# track sequence numbers per client to detect gaps
last_seq = {}

# make sure the if the csv file does not exist to create it and add the headers
if not os.path.isfile(CSV_FILE):
    with open(CSV_FILE, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Timestamp', 'Temperature (°C)', 'Humidity (%)', 'CH4 (ppm)', 'CO2 (ppm)'])

# read exactly length bytes
def recv_all(sock, length):
    data = b''
    while len(data) < length:
        packet = sock.recv(length - len(data))
        if not packet:
            raise ConnectionError("Socket connection closed unexpectedly")
        data += packet
    return data

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, PORT))
    s.listen()
    print(f"Listening on {HOST}:{PORT}")

    while True:
        conn, addr = s.accept()

        conn.settimeout(30)

        with conn:
            print('Connected by', addr)

            while True:
                try:
                    raw_data = recv_all(conn, PACKET_SIZE)
                except (ConnectionError, struct.error) as e:
                    print("Connection lost:", e)
                    break

                magic, version, reserved, seq, temp, humidity, ch4, co2, timestamp = \
                    struct.unpack(PACKET_FORMAT, raw_data)

                # validate protocol header
                if magic != PACKET_MAGIC:
                    print(f"Invalid magic: 0x{magic:04X}, expected 0x{PACKET_MAGIC:04X}. Dropping packet.")
                    continue

                if version != PACKET_VERSION:
                    print(f"Unknown protocol version: {version}. Dropping packet.")
                    continue

                # detect sequence gaps
                client_ip = addr[0]
                if client_ip in last_seq:
                    expected = last_seq[client_ip] + 1
                    if seq != expected:
                        print(f"Sequence gap from {client_ip}: expected {expected}, got {seq} "
                              f"({seq - expected} packets lost)")
                last_seq[client_ip] = seq

                readable = datetime.datetime.fromtimestamp(timestamp)
                time_str = readable.strftime('%Y-%m-%d %H:%M:%S')

                print(f"[seq={seq}] Temp: {temp}°C, Humidity: {humidity}%, "
                      f"CH4: {ch4:.2f} ppm, CO2: {co2:.2f} ppm")
                print("Timestamp:", time_str)
                print()

                with open(CSV_FILE, 'a', newline='') as f:
                    writer = csv.writer(f)
                    writer.writerow([time_str, temp, humidity, round(ch4, 2), round(co2, 2)])

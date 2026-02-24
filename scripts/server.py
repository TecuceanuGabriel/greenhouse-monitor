import socket
import struct
import datetime
import csv
import os

HOST = '0.0.0.0'
PORT = 1234
CSV_FILE = 'data_log.csv'

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
                    raw_data = recv_all(conn, 24)
                except (ConnectionError, struct.error) as e:
                    print("Connection lost:", e)
                    break;

                # unpack values (2 int32, 1 float32, 1 int64)
                temp, humidity, ch4, co2, timestamp = struct.unpack('iiffq', raw_data)

                readable = datetime.datetime.fromtimestamp(timestamp)
                time_str = readable.strftime('%Y-%m-%d %H:%M:%S')

                print(f"Temp: {temp}°C, Humidity: {humidity}%, CH4: {ch4:.2f} ppm, CO2: {co2:.2f} ppm")
                print("Timestamp:", time_str)
                print()

                with open(CSV_FILE, 'a', newline='') as f:
                    writer = csv.writer(f)
                    writer.writerow([time_str, temp, humidity, round(ch4, 2), round(co2, 2)])

# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import time

import paho.mqtt.client as mqtt

# ==== Configuration ====
# BROKER = "broker.hivemq.com"
# BROKER = "test.mosquitto.org"
# BROKER = "iot.coreflux.cloud"

BROKER = '192.168.31.171'
PORT = 1883  # Default MQTT port
BIN_PATH = 'build/simple_mqtt_ota.bin'  # Path to your firmware binary
CHUNK_SIZE = 4000  # Chunk size in bytes, if you can't change the buffer size you need to set CHUNK_SIZE to 1000
TOPIC_UPDATE = 'ota/update'  # Topic where the CHUNK DATA are sended
TOPIC_FIRMWARE = 'ota/firmware'  # Topic where the start message is sended
TOPIC_DONE = 'ota/done'  # topic where the end message is sended

# ==== MQTT Setup ====
client = mqtt.Client()
client.connect(BROKER, PORT)
client.loop_start()

# ==== Step 1: Trigger OTA start ====
print('[1/3] Sending OTA start command...')
client.publish(TOPIC_UPDATE, payload='start')
time.sleep(1)

# ==== Step 2: Send .bin in chunks ====
print('[2/3] Sending firmware:', BIN_PATH)

# Sanity check: make sure the file starts with magic byte 0xE9
with open(BIN_PATH, 'rb') as f:
    magic = f.read(1)
    if magic != b'\xe9':
        print('Invalid firmware file: expected 0xE9 at start, got', magic.hex())
        client.loop_stop()
        exit(1)
time.sleep(5)
# Now send the whole file
with open(BIN_PATH, 'rb') as f:
    total_sent = 0
    while True:
        chunk = f.read(CHUNK_SIZE)
        if not chunk:
            break
        client.publish(TOPIC_FIRMWARE, payload=chunk)
        total_sent += len(chunk)
        print(f'Sent {total_sent} bytes', end='\r')
        time.sleep(0.5)  # Delay to allow ESP time to read from mqtt and write

print(f'\nFirmware upload complete: {total_sent} bytes sent.')

# ==== Step 3: Send OTA done ====
print('[3/3] Sending OTA done command...')
client.publish(TOPIC_DONE, payload='done')
time.sleep(2)

client.loop_stop()
client.disconnect()
print('OTA update pushed successfully via MQTT.')

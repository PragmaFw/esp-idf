| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- | -------- |

# Simple OTA Example

This example demonstrates how to perform firmware upgrades using MQTT and the `esp_ota_ops` APIs.

Firmware is transmitted in chunks over MQTT topics, initiated by an MQTT message and finalized upon receiving a DONE signal. It is designed to be simple and testable using a local or public MQTT broker.

## Configuration

# This project uses sdkconfig.defaults to predefine important OTA-related build parameters:

- Sets the flash size to 4MB.

- Enables the Two OTA Large partition table, allowing full image OTA updates.

- Increases the MQTT buffer size to 4096 bytes, enabling firmware to be sent in larger chunks (default is 1024).

These settings ensure compatibility and performance when performing OTA via MQTT.

- The example allows you to configure the MQTT broker URI and OTA topic names via:

```bash 
idf.py menuconfig -> MQTT OTA Example Configuration
```

## Topics Used

- **`ota/update`** – Initiates the OTA process.
- **`ota/firmware`** – Firmware binary data is sent in chunks.
- **`ota/done`** – Finalizes the OTA and triggers reboot if successful.

## Running OTA Update from a Python Script

You can use the included Python script to send the firmware update over MQTT.

### Prerequisites

Install the MQTT client library:

```bash
pip install paho-mqtt
```

## Notes
- This example uses plain MQTT (mqtt://). For production deployments, use mqtts:// (MQTT over TLS).

- Works with local brokers and also tested in some public brokers(hivemq and mosquitto.org) with unstable results because some time the esp32 will get a timeout from the mqtt or lost some package.

- In theory this example can be used to update multiple devices at the same time because we just send the firmware to a specific topic without expecting a response.

- The Python script checks for the ESP32 firmware magic byte (0xE9) before transmitting the binary.

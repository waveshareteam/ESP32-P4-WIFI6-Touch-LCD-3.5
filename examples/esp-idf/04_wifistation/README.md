# ESP32-C6 hosted Wi-Fi station

[简体中文](README_ZH.md)

The ESP32-P4 has no integrated radio. This example connects as a Wi-Fi station
through the onboard ESP32-C6 coprocessor using `esp_wifi_remote` and
`esp_hosted`. The host components and the firmware running on the ESP32-C6 must
use a compatible protocol generation.

Configure the SSID, password, retry count, authentication threshold, and WPA3
SAE options under `Example Configuration`:

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

The application initializes NVS and the default network interface, starts the
station, retries connection, and reports either an assigned IP address or final
failure without echoing the configured SSID or password. Repository defaults
and CI configuration contain placeholders only; still keep production
credentials out of committed configuration and shared logs.

A successful compile does not prove ESP32-C6 firmware compatibility or radio
operation. Record both host and coprocessor versions when testing on hardware.

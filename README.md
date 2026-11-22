# Skullcave Wi-Fi Extender (ESP32-S3 N16R8)

A high-throughput, dual-core 2.4 GHz Wi-Fi Range Extender (NAPT Router) built for the **ESP32-S3 N16R8**. Features a mobile-friendly Captive Portal web GUI for zero-code network configuration, persistent NVS credential storage, dynamic Eco-Idle power management, and real-time WS2812 RGB LED signal telemetry.

---

## Overview

When your primary router is too far from your workstation or mobile device, this firmware turns an ESP32-S3 N16R8 into a standalone **AP+STA NAT repeater**. It connects to your upstream 2.4 GHz router as a station (`STA`), spawns a dedicated local hotspot (`AP`) locked to the exact same Wi-Fi channel to avoid single-radio switching penalties, and routes IPv4 traffic using lwIP Network Address Port Translation (`NAPT`) with direct Cloudflare DNS (`1.1.1.1`) forwarding.

### Key Features
* **Mobile Captive Portal & Live Telemetry GUI:** Automatically pops up on Android/iOS upon first boot (`Skullcave_Setup`) with a built-in nearby Wi-Fi scanner, signal strength (`RSSI`) monitor, active client counter, and persistent NVS flash storage.
* **40 MHz (`HT40`) Turbo Throughput:** Forces 40 MHz channel width and `19.5 dBm` max TX power when clients are connected, delivering ~10–14 Mbps real-world throughput on the Arduino ESP32 core.
* **Dynamic Eco-Idle Power Management:** Automatically drops TX power to `8.5 dBm` and enables `WIFI_PS_MIN_MODEM` radio sleep when zero clients are connected, drastically reducing thermal load and power consumption during 24/7 operation.
* **Dual-Core Architecture:** Isolates UI telemetry, button watchdogs, and LED animations onto **Core 0**, leaving **Core 1** dedicated to uninterrupted lwIP packet forwarding.
* **Hardware Factory Reset:** Holding the onboard `BOOT` button (`GPIO 0`) for 3 seconds wipes stored credentials and reboots cleanly into Setup Mode.

---

## Hardware & Software Requirements

### Hardware
* **Microcontroller:** ESP32-S3 DevKitC-1 **N16R8** (16 MB Flash, 8 MB Octal PSRAM, onboard WS2812 RGB LED on `GPIO 48`).
* **Power Supply:** Reliable 5V / 1A+ USB-C power adapter (recommended for stable `19.5 dBm` TX bursts).
* **Optional:** External U.FL/IPEX 2.4 GHz antenna (if using a board variant with an external antenna connector) for extended upstream range.

### Software & Toolchain
* **ESP32 Arduino Core:** `v3.0.0` or newer (required for native `WiFi.AP.enableNAPT()` and `rgbLedWrite()` APIs).
* **IDE / Flasher:** Arduino IDE, PlatformIO, or **ArduinoDroid** (on Android via USB OTG).
* **Libraries (All Built-In):** `WiFi.h`, `WebServer.h`, `DNSServer.h`, `Preferences.h`, `esp_wifi.h`.

---

## Technical Architecture

| Parameter | Configuration | Purpose |
| :--- | :--- | :--- |
| **Operating Mode** | `WIFI_AP_STA` | Simultaneous upstream client & downstream hotspot |
| **Channel Sync** | Dynamic (`WiFi.channel()`) | Locks SoftAP channel to upstream router to prevent radio channel-hopping latency |
| **Channel Bandwidth** | `WIFI_BW_HT40` (40 MHz) | Maximizes 802.11n PHY link rate |
| **AP Subnet / Gateway** | `192.168.50.1/24` | Avoids subnet collisions with standard `192.168.0.x`, `192.168.1.x`, and `192.168.4.x` routers |
| **Client DNS** | `1.1.1.1` (Cloudflare) | Injected via DHCP lease options (`WiFi.AP.config`) for fast domain resolution |
| **Active Power Profile** | `19.5 dBm` / `WIFI_PS_NONE` | Zero modem sleep and maximum range when `clients > 0` |
| **Eco-Idle Profile** | `8.5 dBm` / `WIFI_PS_MIN_MODEM` | Cool-running standby mode when `clients == 0` |

---

## RGB LED Status Reference (`GPIO 48`)

| LED Color & Pattern | System State | Meaning / Action |
| :--- | :--- | :--- |
| **Solid Blue** | Booting | Initializing Wi-Fi stack and connecting to saved router |
| **Pulsing Purple** | Setup Portal Mode | Broadcasting open `Skullcave_Setup` network awaiting configuration |
| **Solid Green** | Active — Optimal (`> -62 dBm`) | Client connected; strong upstream link (peak throughput) |
| **Solid Orange** | Active — Moderate (`-62` to `-72 dBm`) | Client connected; acceptable link (move closer to router for higher speed) |
| **Solid Magenta** | Active — Weak (`< -72 dBm`) | Client connected; poor upstream signal (relocate extender halfway to router) |
| **Dim Cyan Pulse** | Eco-Idle Standby | Connected to router, 0 clients attached; low-power cool mode active |
| **Flashing Red** | Upstream Lost | Main router unreachable; auto-reconnecting every 15 seconds |

---

## Getting Started

### 1. Compile & Flash
1. Open the `.ino` sketch in **Arduino IDE** or **ArduinoDroid**.
2. Configure the board parameters:
   * **Board:** `ESP32S3 Dev Module`
   * **Flash Size:** `16MB (128Mb)`
   * **PSRAM:** `OPI PSRAM`
   * **USB CDC On Boot:** `Enabled` (if flashing via native USB port)
3. Connect your ESP32-S3 (use the **UART/COM** port on dual-USB boards for reliable flashing) and upload the firmware.

### 2. Initial Setup via Mobile
1. On first boot, the onboard RGB LED will **pulse Purple**.
2. Open Wi-Fi settings on your smartphone and connect to **`Skullcave_Setup`** (open network, no password).
3. Your phone will automatically trigger a **Sign in to Wi-Fi network** prompt (or navigate manually to `http://192.168.50.1`).
4. Tap **Scan Nearby Wi-Fi Networks**, select your **2.4 GHz** home router SSID, enter your router password, customize your extender SSID/password (default: `Skullcave` / `skullcave`, min 8 characters), and tap **Save & Reboot Extender**.

### 3. Placement & Daily Use
* **Optimal Placement:** Position the ESP32-S3 **halfway between your main router and your workstation**—ideally in a spot where the RGB LED stays **solid Green** when a device is connected. Keep the PCB antenna elevated and away from metal surfaces or thick concrete walls.
* **Live Dashboard:** While connected to your extender hotspot, visit `http://192.168.50.1` anytime from any browser to monitor live signal strength, connected clients, and power state, or to switch upstream networks.
* **Factory Reset:** Press and hold the onboard **`BOOT`** button (`GPIO 0`) for **3 seconds** until the LED flashes bright Red. All saved credentials will be cleared and the board will reboot into `Skullcave_Setup`.

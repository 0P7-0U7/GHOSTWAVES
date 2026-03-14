<p align="right">
  <a href="https://buymeacoffee.com/optoutbrussels">
    <img src="https://img.shields.io/badge/Buy%20Me%20a%20Coffee-ffdd00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black" alt="Buy Me a Coffee">
  </a>
</p>


## License (CNPL v4)
GHOSTWAVES is released under the **Cooperative Non-Violent Public License**.

This means you are free to use it for art, education, and civilian research. However:
- **NO Military Use:** Use by military organizations or for weapons development is strictly prohibited.
- **NO Surveillance:** Use for state-sponsored surveillance is prohibited.
- **Cooperative Only:** Commercial use is reserved for worker-owned cooperatives.

----------------------------------------




<picture>
  <source media="(prefers-color-scheme: dark)" srcset="images/LOGO_DARK.png">
  <source media="(prefers-color-scheme: light)" srcset="images/LOGO_LIGHT.png">
  <img alt="GHOSTWAVES by OPT-OUT" src="images/LOGO_LIGHT.png" width="100%">
</picture>

# GHOSTWAVES by OPT-OUT
**Bare-Metal 802.11 Frame Injection Library for ESP32**

Send and receive raw 802.11 Vendor-Specific Action Frames on ESP32 without any router, access point, or connection. Bypasses the standard Wi-Fi stack entirely via `esp_wifi_80211_tx()`.

### Features
* **Connectionless:** No router, no pairing, no handshake. Construct a frame and transmit.
* **Custom Frame Types:** The `oui_type` byte gives you 256 message types to build your own protocol.
* **Channel Hopping:** Scan for the quietest channel and migrate the network on the fly.
* **Full Frame Access:** You control the entire 802.11 frame — MAC header, OUI, type byte, payload.

### Limitations
* No encryption — payloads are plaintext. Bring your own crypto if needed.
* No delivery guarantees — no ACKs, no retries. You will lose packets.
* Not invisible — any monitor-mode device on the same channel can capture these frames.
* ESP32 only (all variants: S2, S3, C3, C6).

---

## Quick Start

**ESP32 Hardware Compatibility:**
- **All ESP32 Variants:** ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6.

### 1. Sender
```cpp
#include <GhostWaves.h>

const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void setup() {
  Serial.begin(115200);
  GhostWaves.begin(6); // Channel 6
}

void loop() {
  const char* msg = "hello";
  GhostWaves.send(broadcast, (const uint8_t*)msg, strlen(msg));
  delay(1000);
}
```

### 2. Receiver
```cpp
#include <GhostWaves.h>

void setup() {
  Serial.begin(115200);
  GhostWaves.begin(6); // Same channel
}

void loop() {
  if (GhostWaves.available()) {
    GhostRxPacket pkt;
    if (GhostWaves.read(&pkt)) {
      Serial.printf("[%02X:%02X] RSSI:%d len:%d\n",
        pkt.sender_mac[4], pkt.sender_mac[5],
        pkt.rssi, pkt.length);
    }
  }
}
```

### 3. Deep Sleep Sensor
```cpp
#include <GhostWaves.h>

struct __attribute__((packed)) SensorData {
  uint8_t node_id;
  float   temperature;
};

const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void setup() {
  GhostWaves.begin(6);

  SensorData data = { .node_id = 1, .temperature = 23.5 };
  GhostWaves.send(broadcast, (const uint8_t*)&data, sizeof(data), 0x02);
  delay(10);

  GhostWaves.end();
  esp_deep_sleep(30 * 1000000ULL);
}

void loop() {}
```

---

## API

| Method | Description |
| :--- | :--- |
| `begin(channel)` | Initialize radio on channel 1-13 |
| `send(dest, payload, len, type)` | Transmit a frame (type defaults to 0x01) |
| `available()` | Check if frames are in the receive queue |
| `read(&pkt)` | Dequeue oldest received frame |
| `setChannel(ch)` | Retune radio on the fly |
| `getChannel()` | Get current channel |
| `scanQuietestChannel(ms)` | Scan all channels, return least congested |
| `end()` | Stop radio, free queue |

---

## Examples

| Example | Description |
| :--- | :--- |
| **GhostSensor** | Wake, read sensor, send one frame, deep sleep. |
| **GhostSwarm** | Decentralized peer discovery with RSSI table. |
| **GhostChat** | Serial-to-air text messenger. |
| **GhostRelay** | Multi-hop relay with TTL and deduplication. |
| **GhostHop** | Coordinated channel migration (Leader/Follower). |

---

### Full Documentation
For architecture diagrams, frame anatomy, API reference, and all examples, open the included **`docs/index.html`** in your browser.
OR JUST GO TO <a href="https://0p7-0u7.github.io/GHOSTWAVES/" target="_blank">GHOSTWAVES PAGE</a>

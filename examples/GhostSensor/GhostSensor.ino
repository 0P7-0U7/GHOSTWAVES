/*
 * GhostSensor — Ultra-Low-Power Telemetry Node
 *
 * This sketch demonstrates the "wake, blast, sleep" pattern.
 * The ESP32 wakes from deep sleep, reads sensors, fires a single
 * GhostWaves frame into the void, and goes back to sleep.
 *
 * Total awake time: ~50ms. A coin-cell battery can last years.
 *
 * Wire a receiver running GhostPing (or any sketch that listens
 * for GHOST_TYPE_SENSOR) to see the data arrive.
 */

#include <GhostWaves.h>

#define GHOST_TYPE_SENSOR  0x02
#define SLEEP_SECONDS      30      // Deep sleep interval
#define GHOST_CHANNEL      6

// The payload structure — define whatever fits your sensors
struct __attribute__((packed)) SensorPayload {
  uint8_t  node_id;        // Identify this node
  float    temperature;
  float    humidity;
  float    battery_voltage;
};

const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void setup() {
  // No serial in production — saves power. Uncomment for debugging.
  // Serial.begin(115200);

  GhostWaves.begin(GHOST_CHANNEL);

  // Read sensors (replace with real readings)
  SensorPayload data;
  data.node_id     = 1;
  data.temperature  = readTemperature();
  data.humidity     = readHumidity();
  data.battery_voltage = readBattery();

  // Fire the frame — one shot, no handshake, no ACK needed
  GhostWaves.send(broadcast, (const uint8_t*)&data, sizeof(data), GHOST_TYPE_SENSOR);

  // Give the radio a moment to actually transmit
  delay(10);

  // Shut everything down
  GhostWaves.end();

  // Deep sleep — the ESP32 draws ~10uA in this state
  esp_deep_sleep(SLEEP_SECONDS * 1000000ULL);
}

void loop() {
  // Never reaches here — deep sleep resets into setup()
}

// --- Stub sensor functions (replace with your real hardware) ---

float readTemperature() {
  // Example: read from a DHT22, BME280, thermistor, etc.
  return 22.5 + (random(0, 100) / 100.0);
}

float readHumidity() {
  return 55.0 + (random(0, 200) / 100.0);
}

float readBattery() {
  // Example: read ADC on a voltage divider
  return 3.3 - (random(0, 50) / 100.0);
}

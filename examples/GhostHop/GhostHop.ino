/*
 * GhostHop — Channel Hopping & Auto-Migration
 *
 * Demonstrates three channel features:
 *   1. scanQuietestChannel() — find the least congested channel
 *   2. setChannel() — retune the radio on the fly
 *   3. Coordinated migration — a Leader node tells all Followers
 *      to hop to a new channel using a HOP command frame
 *
 * LEADER MODE: Scans, picks the best channel, broadcasts a HOP
 *              command, then switches itself.
 *
 * FOLLOWER MODE: Listens for HOP commands and follows the Leader.
 *
 * Set IS_LEADER to true on ONE board, false on the others.
 */

#include <GhostWaves.h>
#include <WiFi.h>

// --- CONFIG ---
#define IS_LEADER          true    // Set false on follower nodes
#define START_CHANNEL      6
#define HOP_INTERVAL       30000   // Leader scans every 30 seconds
#define GHOST_TYPE_HOP     0xF0    // Reserved: channel hop command
#define GHOST_TYPE_PING    0x01

const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// The HOP command payload
struct __attribute__((packed)) HopCommand {
  uint8_t new_channel;
  uint8_t reason;       // 0=auto, 1=manual
};

unsigned long lastHop = 0;
unsigned long lastPing = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  GhostWaves.begin(START_CHANNEL);

  Serial.printf("\n--- GHOSTHOP [%s] ---\n", IS_LEADER ? "LEADER" : "FOLLOWER");
  Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
  Serial.printf("Starting on Channel %d (%.3f GHz)\n\n",
    GhostWaves.getChannel(),
    2.407 + (GhostWaves.getChannel() * 0.005));
}

void loop() {
  unsigned long now = millis();

  // --- LEADER: Periodic channel scan and migration ---
  #if IS_LEADER
  if (now - lastHop > HOP_INTERVAL) {
    lastHop = now;

    Serial.println("[SCAN] Scanning all channels...");
    uint8_t best = GhostWaves.scanQuietestChannel(100); // 100ms per channel
    uint8_t current = GhostWaves.getChannel();

    Serial.printf("[SCAN] Best channel: %d (current: %d)\n", best, current);

    if (best != current) {
      Serial.printf("[HOP] Migrating network to channel %d...\n", best);

      // Broadcast the HOP command 3 times on the CURRENT channel
      // (redundancy — connectionless, so repeat for reliability)
      HopCommand cmd = { .new_channel = best, .reason = 0 };
      for (int i = 0; i < 3; i++) {
        GhostWaves.send(broadcast, (const uint8_t*)&cmd, sizeof(cmd), GHOST_TYPE_HOP);
        delay(5);
      }

      // Now switch ourselves
      delay(20); // Let the last frame leave the radio
      GhostWaves.setChannel(best);
      Serial.printf("[HOP] Now on channel %d (%.3f GHz)\n\n",
        best, 2.407 + (best * 0.005));
    } else {
      Serial.println("[SCAN] Already on the best channel.\n");
    }
  }
  #endif

  // --- FOLLOWER: Listen for HOP commands ---
  if (GhostWaves.available()) {
    GhostRxPacket pkt;
    if (GhostWaves.read(&pkt)) {

      if (pkt.oui_type == GHOST_TYPE_HOP && pkt.length >= sizeof(HopCommand)) {
        HopCommand* cmd = (HopCommand*)pkt.payload;
        uint8_t current = GhostWaves.getChannel();

        if (cmd->new_channel != current) {
          Serial.printf("[HOP] Leader says move to channel %d\n", cmd->new_channel);
          GhostWaves.setChannel(cmd->new_channel);
          Serial.printf("[HOP] Now on channel %d (%.3f GHz)\n\n",
            cmd->new_channel, 2.407 + (cmd->new_channel * 0.005));
        }
      }
      else if (pkt.oui_type == GHOST_TYPE_PING) {
        Serial.printf("[PING] from %02X:%02X ch=%d rssi=%d\n",
          pkt.sender_mac[4], pkt.sender_mac[5],
          GhostWaves.getChannel(), pkt.rssi);
      }
    }
  }

  // --- Both: Ping every 2 seconds to show we're alive ---
  if (now - lastPing > 2000) {
    lastPing = now;
    const char* msg = "hop-alive";
    GhostWaves.send(broadcast, (const uint8_t*)msg, strlen(msg), GHOST_TYPE_PING);
  }
}

/*
 * GhostRelay — Multi-Hop Frame Relay
 *
 * This demonstrates something ESP-NOW fundamentally cannot do:
 * a frame that hops from node to node, extending range beyond
 * what a single radio can reach.
 *
 * How it works:
 *   - A node creates a message with a TTL (time-to-live) and
 *     a unique message ID.
 *   - Any node that receives it decrements the TTL and rebroadcasts.
 *   - Duplicate messages (same ID) are silently dropped.
 *   - The message propagates outward like a ripple on water.
 *
 * Flash onto 3+ ESP32s spread across a distance. Node A sends a
 * message that Node C can only receive via Node B relaying it.
 *
 * This is the seed of a mesh network, built from nothing.
 */

#include <GhostWaves.h>
#include <WiFi.h>

#define GHOST_TYPE_RELAY   0x30
#define GHOST_CHANNEL      6
#define MAX_TTL            5       // Max hops before a frame dies
#define SEEN_CACHE_SIZE    32      // Remember recent message IDs

const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// The relay frame — self-describing, self-limiting
struct __attribute__((packed)) RelayFrame {
  uint16_t msg_id;           // Unique message identifier
  uint8_t  origin_mac[6];    // Who created this message originally
  uint8_t  ttl;              // Hops remaining
  uint8_t  hop_count;        // How many hops so far
  char     payload[180];     // The actual message content
};

// Ring buffer of recently seen message IDs (deduplication)
uint16_t seenCache[SEEN_CACHE_SIZE];
int seenIndex = 0;

uint8_t myMac[6];
uint16_t nextMsgId = 0;
unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  GhostWaves.begin(GHOST_CHANNEL);
  WiFi.macAddress(myMac);

  memset(seenCache, 0, sizeof(seenCache));

  // Seed message IDs from MAC so they're unique per node
  nextMsgId = (myMac[4] << 8) | myMac[5];

  Serial.println("\n--- GHOSTRELAY ---");
  Serial.println("Multi-hop frame relay over raw 802.11\n");
  Serial.printf("Node: %02X:%02X:%02X:%02X:%02X:%02X\n",
    myMac[0], myMac[1], myMac[2], myMac[3], myMac[4], myMac[5]);
  Serial.println("Type a message to broadcast across the relay mesh.\n");
}

void loop() {
  // --- ORIGINATE: Send typed messages into the relay ---
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    RelayFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.msg_id = nextMsgId++;
    memcpy(frame.origin_mac, myMac, 6);
    frame.ttl = MAX_TTL;
    frame.hop_count = 0;
    strncpy(frame.payload, line.c_str(), sizeof(frame.payload) - 1);

    markSeen(frame.msg_id);

    GhostWaves.send(broadcast_mac, (const uint8_t*)&frame, sizeof(frame), GHOST_TYPE_RELAY);

    Serial.printf("[SENT] id=%d ttl=%d \"%s\"\n", frame.msg_id, frame.ttl, frame.payload);
  }

  // --- RELAY: Receive, print, and forward ---
  if (GhostWaves.available()) {
    GhostRxPacket pkt;
    if (GhostWaves.read(&pkt) && pkt.oui_type == GHOST_TYPE_RELAY && pkt.length >= sizeof(RelayFrame)) {
      RelayFrame* frame = (RelayFrame*)pkt.payload;

      // Drop if we've already seen this message
      if (alreadySeen(frame->msg_id)) return;
      markSeen(frame->msg_id);

      // Print it
      Serial.printf("[RELAY] id=%d from=%02X:%02X:%02X:%02X:%02X:%02X hops=%d rssi=%d \"%s\"\n",
        frame->msg_id,
        frame->origin_mac[0], frame->origin_mac[1], frame->origin_mac[2],
        frame->origin_mac[3], frame->origin_mac[4], frame->origin_mac[5],
        frame->hop_count, pkt.rssi,
        frame->payload);

      // Forward if TTL allows
      if (frame->ttl > 1) {
        RelayFrame fwd = *frame;
        fwd.ttl--;
        fwd.hop_count++;

        // Small random delay to avoid collision with other relayers
        delay(random(10, 50));

        GhostWaves.send(broadcast_mac, (const uint8_t*)&fwd, sizeof(fwd), GHOST_TYPE_RELAY);
        Serial.printf("  -> Relayed (ttl now %d)\n", fwd.ttl);
      } else {
        Serial.println("  -> TTL expired, not relaying");
      }
    }
  }
}

bool alreadySeen(uint16_t id) {
  for (int i = 0; i < SEEN_CACHE_SIZE; i++) {
    if (seenCache[i] == id) return true;
  }
  return false;
}

void markSeen(uint16_t id) {
  seenCache[seenIndex] = id;
  seenIndex = (seenIndex + 1) % SEEN_CACHE_SIZE;
}

/*
 * GhostSwarm — Decentralized Swarm Coordination
 *
 * Every node broadcasts its own state (position, intent) and listens
 * to everyone else. No central controller. No router. No hierarchy.
 *
 * Each node maintains a live table of nearby peers, their positions,
 * and signal strength. This is the foundation for:
 *   - Robot swarm formation control
 *   - Interactive art installations
 *   - Distributed games with physical proximity
 *
 * Flash this same sketch onto multiple ESP32s and watch them
 * discover each other on the serial monitor.
 */

#include <GhostWaves.h>
#include <WiFi.h>

#define GHOST_TYPE_SWARM   0x10
#define GHOST_CHANNEL      6
#define BROADCAST_INTERVAL 500    // ms between broadcasts
#define PEER_TIMEOUT       5000   // Remove peer after 5s silence
#define MAX_PEERS          20

// What each node yells into the void
struct __attribute__((packed)) SwarmBeacon {
  uint8_t node_id;
  float   pos_x;
  float   pos_y;
  uint8_t state;    // 0=idle, 1=moving, 2=seeking, etc.
};

// What we remember about each peer
struct Peer {
  uint8_t  mac[6];
  uint8_t  node_id;
  float    pos_x;
  float    pos_y;
  uint8_t  state;
  int8_t   rssi;          // Signal strength = rough distance
  unsigned long last_seen;
  bool     active;
};

const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

Peer peers[MAX_PEERS];
uint8_t my_node_id;
unsigned long lastBroadcast = 0;
unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  GhostWaves.begin(GHOST_CHANNEL);

  // Derive a unique-ish node ID from the MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  my_node_id = mac[5]; // Last byte of MAC — good enough for demos

  memset(peers, 0, sizeof(peers));

  Serial.printf("\n--- GHOSTSWARM NODE 0x%02X ---\n", my_node_id);
  Serial.println("Broadcasting presence. Listening for peers...\n");
}

void loop() {
  unsigned long now = millis();

  // --- BROADCAST our presence ---
  if (now - lastBroadcast > BROADCAST_INTERVAL) {
    lastBroadcast = now;

    SwarmBeacon beacon;
    beacon.node_id = my_node_id;
    beacon.pos_x   = random(-100, 100) / 10.0; // Replace with real position
    beacon.pos_y   = random(-100, 100) / 10.0;
    beacon.state    = 0; // idle

    GhostWaves.send(broadcast_mac, (const uint8_t*)&beacon, sizeof(beacon), GHOST_TYPE_SWARM);
  }

  // --- LISTEN for peers ---
  if (GhostWaves.available()) {
    GhostRxPacket pkt;
    if (GhostWaves.read(&pkt) && pkt.oui_type == GHOST_TYPE_SWARM && pkt.length >= sizeof(SwarmBeacon)) {
      SwarmBeacon* b = (SwarmBeacon*)pkt.payload;

      if (b->node_id == my_node_id) return; // Ignore our own echo

      // Update or create peer entry
      int slot = findPeer(b->node_id);
      if (slot < 0) slot = findEmptySlot();
      if (slot >= 0) {
        memcpy(peers[slot].mac, pkt.sender_mac, 6);
        peers[slot].node_id   = b->node_id;
        peers[slot].pos_x     = b->pos_x;
        peers[slot].pos_y     = b->pos_y;
        peers[slot].state     = b->state;
        peers[slot].rssi      = pkt.rssi;
        peers[slot].last_seen = now;
        peers[slot].active    = true;
      }
    }
  }

  // --- EXPIRE stale peers ---
  for (int i = 0; i < MAX_PEERS; i++) {
    if (peers[i].active && (now - peers[i].last_seen > PEER_TIMEOUT)) {
      Serial.printf("  [LOST] Node 0x%02X disappeared\n", peers[i].node_id);
      peers[i].active = false;
    }
  }

  // --- PRINT peer table every 2 seconds ---
  if (now - lastPrint > 2000) {
    lastPrint = now;
    printPeerTable();
  }
}

int findPeer(uint8_t node_id) {
  for (int i = 0; i < MAX_PEERS; i++) {
    if (peers[i].active && peers[i].node_id == node_id) return i;
  }
  return -1;
}

int findEmptySlot() {
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peers[i].active) return i;
  }
  return -1; // Table full
}

void printPeerTable() {
  int count = 0;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (peers[i].active) count++;
  }

  Serial.printf("--- SWARM: %d peers visible ---\n", count);
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peers[i].active) continue;
    Serial.printf("  Node 0x%02X | pos=(%.1f, %.1f) | RSSI: %d dBm | %lums ago\n",
      peers[i].node_id,
      peers[i].pos_x, peers[i].pos_y,
      peers[i].rssi,
      millis() - peers[i].last_seen);
  }
  Serial.println();
}

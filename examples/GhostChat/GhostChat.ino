/*
 * GhostChat — Invisible Text Messenger
 *
 * Type a message in the Serial Monitor, press Enter, and it gets
 * broadcast as a raw 802.11 frame. Any other GhostChat node on the
 * same channel sees it instantly.
 *
 * No router. No internet. No account. No trace.
 * Just electromagnetic waves carrying your words through walls.
 *
 * Flash onto 2+ ESP32s, open their serial monitors, and talk.
 */

#include <GhostWaves.h>
#include <WiFi.h>

#define GHOST_TYPE_CHAT    0x20
#define GHOST_CHANNEL      6
#define MAX_MSG_LEN        200  // Leave room for the nickname header

const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Chat message structure
struct __attribute__((packed)) ChatMessage {
  char     nick[8];              // 7 chars + null
  char     text[MAX_MSG_LEN];
};

char myNick[8];
char inputBuffer[MAX_MSG_LEN];
int  inputPos = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n--- GHOSTCHAT ---");
  Serial.println("Invisible messenger over raw 802.11\n");

  GhostWaves.begin(GHOST_CHANNEL);

  // Generate a nickname from the MAC
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(myNick, sizeof(myNick), "G-%02X%02X", mac[4], mac[5]);

  Serial.printf("You are: %s\n", myNick);
  Serial.printf("Channel: %d\n", GHOST_CHANNEL);
  Serial.println("Type a message and press Enter.\n");
}

void loop() {
  // --- SEND: Read from Serial and broadcast ---
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (inputPos > 0) {
        inputBuffer[inputPos] = '\0';

        ChatMessage msg;
        memset(&msg, 0, sizeof(msg));
        strncpy(msg.nick, myNick, sizeof(msg.nick) - 1);
        strncpy(msg.text, inputBuffer, sizeof(msg.text) - 1);

        size_t sendLen = strlen(msg.nick) + 1 + strlen(msg.text) + 1;
        GhostWaves.send(broadcast_mac, (const uint8_t*)&msg, sendLen, GHOST_TYPE_CHAT);

        Serial.printf("[you] %s\n", inputBuffer);
        inputPos = 0;
      }
    } else if (inputPos < MAX_MSG_LEN - 1) {
      inputBuffer[inputPos++] = c;
    }
  }

  // --- RECEIVE: Print incoming messages ---
  if (GhostWaves.available()) {
    GhostRxPacket pkt;
    if (GhostWaves.read(&pkt) && pkt.oui_type == GHOST_TYPE_CHAT) {
      ChatMessage* msg = (ChatMessage*)pkt.payload;

      // Don't echo our own messages
      if (strncmp(msg->nick, myNick, sizeof(myNick)) == 0) return;

      Serial.printf("[%s] (RSSI:%d) %s\n", msg->nick, pkt.rssi, msg->text);
    }
  }
}

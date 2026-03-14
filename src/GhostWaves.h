#ifndef GHOSTWAVES_H
#define GHOSTWAVES_H

#include <Arduino.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// The maximum safe payload size for an Action Frame
#define GHOSTWAVES_MAX_PAYLOAD 230

// Channel limits
#define GHOSTWAVES_MIN_CHANNEL 1
#define GHOSTWAVES_MAX_CHANNEL 13

// Default OUI (Magic Bytes: 0xBE, 0xEF, 0x00)
// Declared extern to avoid multiple-definition linker errors
extern const uint8_t GHOST_OUI[3];

// Default frame type
#define GHOSTWAVES_DEFAULT_TYPE 0x01

// --- RAW 802.11 STRUCTURES ---

// The standard 802.11 MAC Header (24 bytes)
struct __attribute__((packed)) GhostMacHeader {
    uint16_t frame_control; // 0xD0 for Action Frame
    uint16_t duration;
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint8_t  bssid[6];
    uint16_t seq_ctrl;
};

// The Vendor-Specific Action Frame Body
struct __attribute__((packed)) GhostActionBody {
    uint8_t category;       // 0x7F for Vendor Specific
    uint8_t oui[3];         // 0xBE, 0xEF, 0x00
    uint8_t oui_type;       // Custom identifier (e.g., 0x01)
    uint8_t payload[GHOSTWAVES_MAX_PAYLOAD];
};

// The Complete Frame
struct __attribute__((packed)) GhostFrame {
    GhostMacHeader header;
    GhostActionBody body;
};

// Structure for passing received data through the FreeRTOS Queue
struct GhostRxPacket {
    uint8_t sender_mac[6];
    uint8_t oui_type;    // Frame type identifier
    uint8_t payload[GHOSTWAVES_MAX_PAYLOAD];
    size_t length;
    int8_t rssi; // Signal strength
};

// --- LIBRARY CLASS ---

class GhostWavesClass {
public:
    GhostWavesClass();
    
    // Initialize the radio on a specific channel (1-13)
    bool begin(uint8_t channel);
    
    // Transmit a payload to a specific MAC (or broadcast)
    // oui_type lets you tag frames by purpose (0x01=default, define your own)
    bool send(const uint8_t* dest_mac, const uint8_t* payload, size_t len, uint8_t oui_type = GHOSTWAVES_DEFAULT_TYPE);
    
    // Switch channel on the fly (1-13). Both TX and RX retune instantly.
    bool setChannel(uint8_t channel);

    // Get the current channel
    uint8_t getChannel();

    // Scan all channels and return the quietest one (least traffic)
    // Listens on each channel for `dwell_ms` milliseconds.
    uint8_t scanQuietestChannel(uint16_t dwell_ms = 50);

    // Check if we have received any ghost frames
    bool available();

    // Read the oldest frame from the queue
    bool read(GhostRxPacket* packet);

    // Stop promiscuous mode and clean up
    void end();

private:
    uint8_t _channel;
    uint8_t _my_mac[6];
    QueueHandle_t _rx_queue;

    // Packet counter for channel scanning (volatile — written from ISR)
    static volatile uint32_t _scan_pkt_count;

    // The internal promiscuous callback must be static
    static void _promiscuous_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type);

    // Lightweight scan callback — just counts all management frames
    static void _scan_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type);
};

// Make it a singleton like Serial or Wire
extern GhostWavesClass GhostWaves;

#endif
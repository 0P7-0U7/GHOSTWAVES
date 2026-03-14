#include "GhostWaves.h"

// Define the OUI constant (declared extern in header)
const uint8_t GHOST_OUI[3] = {0xBE, 0xEF, 0x00};

// Initialize the singleton instance
GhostWavesClass GhostWaves;

// Pointer to the singleton for use in the static callback
static GhostWavesClass* _instance = nullptr;

// Initialize static scan counter
volatile uint32_t GhostWavesClass::_scan_pkt_count = 0;

GhostWavesClass::GhostWavesClass() {
    _rx_queue = NULL;
    _instance = this;
}

bool GhostWavesClass::begin(uint8_t channel) {
    _channel = channel;
    
    // Create a queue to hold up to 10 received packets
    if (_rx_queue == NULL) {
        _rx_queue = xQueueCreate(10, sizeof(GhostRxPacket));
    }

    // Get our actual MAC address to use as the source
    esp_read_mac(_my_mac, ESP_MAC_WIFI_STA);

    // Wi-Fi must be in Station mode to use promiscuous features
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);

    // Set up the promiscuous callback
    esp_wifi_set_promiscuous_rx_cb(&GhostWavesClass::_promiscuous_rx_cb);
    esp_wifi_set_promiscuous(true);

    return true;
}

bool GhostWavesClass::send(const uint8_t* dest_mac, const uint8_t* payload, size_t len, uint8_t oui_type) {
    if (len > GHOSTWAVES_MAX_PAYLOAD) return false;

    GhostFrame frame;
    memset(&frame, 0, sizeof(GhostFrame));

    // 1. Build MAC Header
    // 0x00D0 = Action Frame (Management type, subtype 0x0D)
    frame.header.frame_control = 0x00D0;
    frame.header.duration = 0x0000;
    memcpy(frame.header.dest_mac, dest_mac, 6);
    memcpy(frame.header.src_mac, _my_mac, 6);
    memcpy(frame.header.bssid, dest_mac, 6); // Often same as dest in ad-hoc
    frame.header.seq_ctrl = 0; // The radio usually overwrites this anyway

    // 2. Build Action Body
    frame.body.category = 0x7F; // Vendor Specific
    memcpy(frame.body.oui, GHOST_OUI, 3);
    frame.body.oui_type = oui_type;

    // 3. Attach Payload
    memcpy(frame.body.payload, payload, len);

    // Calculate total frame size
    size_t frame_size = sizeof(GhostMacHeader) + 4 /* Category + OUI */ + 1 /* Type */ + len;

    // 4. FIRE THE RAW FRAME!
    esp_err_t result = esp_wifi_80211_tx(WIFI_IF_STA, &frame, frame_size, false);
    
    return (result == ESP_OK);
}

bool GhostWavesClass::setChannel(uint8_t channel) {
    if (channel < GHOSTWAVES_MIN_CHANNEL || channel > GHOSTWAVES_MAX_CHANNEL) return false;
    _channel = channel;
    esp_err_t result = esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);
    return (result == ESP_OK);
}

uint8_t GhostWavesClass::getChannel() {
    return _channel;
}

uint8_t GhostWavesClass::scanQuietestChannel(uint16_t dwell_ms) {
    // Save current state
    uint8_t original_channel = _channel;

    // Temporarily swap to a lightweight counter callback
    esp_wifi_set_promiscuous_rx_cb(&GhostWavesClass::_scan_rx_cb);

    uint32_t min_traffic = UINT32_MAX;
    uint8_t  best_channel = original_channel;

    for (uint8_t ch = GHOSTWAVES_MIN_CHANNEL; ch <= GHOSTWAVES_MAX_CHANNEL; ch++) {
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        _scan_pkt_count = 0;

        vTaskDelay(pdMS_TO_TICKS(dwell_ms));

        if (_scan_pkt_count < min_traffic) {
            min_traffic = _scan_pkt_count;
            best_channel = ch;
        }
    }

    // Restore: go back to original channel and the real Ghost callback
    esp_wifi_set_channel(original_channel, WIFI_SECOND_CHAN_NONE);
    _channel = original_channel;
    esp_wifi_set_promiscuous_rx_cb(&GhostWavesClass::_promiscuous_rx_cb);

    return best_channel;
}

// Lightweight scan callback — counts ALL packets on the channel
void IRAM_ATTR GhostWavesClass::_scan_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    _scan_pkt_count++;
}

bool GhostWavesClass::available() {
    if (_rx_queue == NULL) return false;
    return uxQueueMessagesWaiting(_rx_queue) > 0;
}

bool GhostWavesClass::read(GhostRxPacket* packet) {
    if (_rx_queue == NULL) return false;
    // Pull the packet from the queue (non-blocking)
    if (xQueueReceive(_rx_queue, packet, 0) == pdTRUE) {
        return true;
    }
    return false;
}

void GhostWavesClass::end() {
    esp_wifi_set_promiscuous(false);
    if (_rx_queue != NULL) {
        vQueueDelete(_rx_queue);
        _rx_queue = NULL;
    }
}

// --- THE SNIFFER CALLBACK (Runs in high-priority Wi-Fi task) ---
void IRAM_ATTR GhostWavesClass::_promiscuous_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return; // Only care about management frames

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    GhostFrame* frame = (GhostFrame*)pkt->payload;

    // Fast memory check: Is this an Action Frame?
    if (frame->header.frame_control != 0x00D0) return;

    // Fast memory check: Does it have our exact Magic OUI?
    if (frame->body.category != 0x7F || memcmp(frame->body.oui, GHOST_OUI, 3) != 0) return;

    // IT'S A GHOST FRAME! Package it up for the main loop.
    GhostRxPacket rx_data;
    memcpy(rx_data.sender_mac, frame->header.src_mac, 6);
    rx_data.oui_type = frame->body.oui_type;
    rx_data.rssi = pkt->rx_ctrl.rssi; // Grab signal strength

    // Calculate how long the user payload is (guard against underflow)
    size_t overhead = sizeof(GhostMacHeader) + 4 + 1 + 4; // Mac + Cat/OUI + Type + FCS
    if (pkt->rx_ctrl.sig_len <= overhead) return; // Corrupted / too short
    rx_data.length = pkt->rx_ctrl.sig_len - overhead;

    // Prevent buffer overflow on corrupted packets
    if (rx_data.length > GHOSTWAVES_MAX_PAYLOAD) rx_data.length = GHOSTWAVES_MAX_PAYLOAD;

    memcpy(rx_data.payload, frame->body.payload, rx_data.length);

    // Ship it to the FreeRTOS queue safely (don't block if the queue is full)
    if (_instance && _instance->_rx_queue) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(_instance->_rx_queue, &rx_data, &xHigherPriorityTaskWoken);
    }
}
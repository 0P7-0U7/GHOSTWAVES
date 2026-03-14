The Core Benefits of GHOSTWAVES
By stripping away the standard Wi-Fi stack and talking directly to the radio hardware, you gain massive advantages for specific types of IoT projects:

Zero-Overhead Latency: Standard Wi-Fi requires scanning for a network, authenticating, associating, getting an IP address via DHCP, and handling TCP handshakes. That can take seconds. GHOSTWAVES takes milliseconds. You dictate the frame, the radio fires it, and it's done.

Extreme Battery Life: Because there is no connection overhead, a battery-powered sensor can wake from deep sleep, turn on the radio, blast a GHOSTWAVES frame into the ether, and go back to sleep in a fraction of a second. This allows a coin-cell battery to last for years.

Invisible & Non-Disruptive: These frames do not show up on network scanners, they don't clog up your home router's routing tables, and they don't trigger alerts on standard devices. It is a completely isolated, shadow network sharing the 2.4GHz band.

Absolute Topology Freedom: You aren't forced into a "Router and Clients" (Star) topology. You can have 50 sensors yelling into the void (Many-to-One), one master node broadcasting to 100 receivers (One-to-Many), or a completely decentralized swarm where everyone hears everyone.

No "Black Box" Limitations: While Espressif has a protocol called ESP-NOW that does something similar, it is a closed-source black box. With GHOSTWAVES, you own the entire stack. You define the header, the OUI, and the payload.

The Project Prompt / Summary
Here is a clean, comprehensive prompt that perfectly describes what we are building, how it works, and why. You can copy and paste this into a README or use it as a foundational prompt for future development:

Project Title: GHOSTWAVES - A Bare-Metal 802.11 Frame Injection Library for ESP32

The Concept:
GHOSTWAVES is a custom C++ library for the Arduino-ESP32 ecosystem that bypasses the standard Wi-Fi stack to enable raw 802.11 frame injection and promiscuous sniffing. It transforms the ESP32 from a standard Wi-Fi client into a low-latency, connectionless 2.4GHz RF transceiver. It operates entirely invisibly to standard Wi-Fi routers, phones, and computers.

The Technical Foundation:
Standard Wi-Fi relies on heavy overhead (Beacons, Association, DHCP, TCP/IP). GHOSTWAVES discards this by using the ESP-IDF esp_wifi_80211_tx function to manually construct and transmit Vendor-Specific Action Frames (Management Frame 0xD0).

To receive data, the library places the ESP32 into promiscuous mode, capturing all raw RF traffic on a specific channel. It uses a highly optimized FreeRTOS callback to filter this traffic in real-time, looking only for a custom 3-byte OUI (Magic Bytes: 0xBE 0xEF 0x00). If a match is found, the payload is safely extracted to a FreeRTOS queue for the main application to process.

Key Capabilities:

Connectionless Transmission: No access points, no routers, no pairing.

High-Speed Execution: Payload delivery in milliseconds directly from the radio hardware.

Custom OUI Filtering: Survives noisy 2.4GHz environments by instantly dropping non-matching packets at the memory-offset level.

Thread-Safe Architecture: Uses FreeRTOS queues to pass data from high-priority Wi-Fi tasks to the main loop without triggering Watchdog Timer panics.

Target Use Cases:

Ultra-low-power, battery-operated telemetry sensors.

High-speed, low-latency remote control (e.g., drones, robotics).

Asynchronous, decentralized swarm communications.

Creating stealthy, out-of-band communication channels.
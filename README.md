# ⚡ Self-Organizing Grid-Edge Load Shedding over IPv6 Mesh

An RTOS-coordinated, decentralized demand-response microgrid monitoring system built with **ESP32-C6 microcontrollers**, **OpenThread IPv6 mesh networking**, and an **HTML5/JS Web Serial Dashboard**.

This project demonstrates how smart grid-edge load controllers can self-organize over an IEEE 802.15.4 Thread mesh network, detect grid stress signals, coordinate wireless load-shedding events without a central point of failure, and stream live terminal telemetry directly to an HTML dashboard using the native browser Web Serial API.

---

## 💡 Concept & Key Features

Modern power grids face severe strain during peak demand. Rather than relying on crude rolling blackouts or centralized Wi-Fi hubs that fail to scale gracefully, this project delivers a resilient grid-edge architecture:

1. **Native IPv6 Mesh (IEEE 802.15.4 / Thread):**
   Powered by **ESP32-C6** hardware running OpenThread, nodes self-organize into a robust mesh network without manual pairing. Devices communicate peer-to-peer using IPv6 UDP multicast (`ff03::1`) and automatically route around failed nodes.

2. **RTOS Task-Based Multitasking Architecture:**
   Each node executes independent, concurrent tasks managed by **Zephyr RTOS / FreeRTOS**:
   - **Power/Load Monitor Task:** Samples localized grid conditions and sensor inputs.
   - **Network Listener Task:** Asynchronously receives IPv6 mesh broadcasts via non-blocking sockets.
   - **Relay/PWM Actuation Task:** Controls hardware load shedding via MOSFET PWM dimming and 5V isolation relays.
   - *Synchronization:* Inter-task safety is strictly enforced using **Mutexes** (protecting shared state) and **Queues** (passing network commands to actuation).

3. **In-Browser HTML Web Serial Dashboard:**
   - **Direct USB Telemetry:** Uses the native **Web Serial API** (`navigator.serial`) in Chrome/Edge/Opera to read hardware logs straight from the ESP32-C6 COM port without requiring heavy IDE software.
   - **Aesthetic Log Formatting:** Automatically colorizes log prefixes (`[net]`, `[relay]`, `[monitor]`), prepends local timestamps, and maintains auto-scrolling.
   - **WebSocket Bridge Server:** A Python server (`server.py`) relays incoming IPv6 UDP multicast state events to the HTML dashboard for real-time visual UI updates (SVG load gauge, event tables).

---

## 🏗️ System Architecture

```text
  +-----------------------+              IPv6 UDP Multicast            +-----------------------+
  |  ESP32-C6 Mesh Node A | <----------------------------------------> |  ESP32-C6 Mesh Node B |
  |  (Zephyr RTOS Tasks)  |            Port 1234 / ff03::1             |  (Zephyr RTOS Tasks)  |
  +-----------------------+                                            +-----------------------+
              |                                                                    |
              | (USB Serial / Web Serial API)                                      | (IPv6 UDP)
              v                                                                    v
  +---------------------------------------------------+               +-------------------------+
  |               HTML/JS Web Dashboard               | <===========> |   Python Bridge Server  |
  |  (Web Serial Console / Gauge / Live Log Table)    |  (WebSocket)  |   (server.py)           |
  +---------------------------------------------------+               +-------------------------+

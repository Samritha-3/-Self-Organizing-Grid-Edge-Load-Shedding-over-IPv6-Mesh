# 🔌 Hardware Circuit Connections & Pinout Guide

This document provides the exact pinout specifications and wiring schematics for both **ESP32-C6 SuperMini** nodes operating on the OpenThread IPv6 demand-response mesh network[cite: 1].

---

## 📌 System Overview & Power Requirements

- **Microcontrollers:** ESP32-C6 SuperMini (Native IEEE 802.15.4 / Thread radio)[cite: 1]
- **Operating Voltage:** 3.3V Logic Level
- **Power Source:** 5V / VBUS via USB-C[cite: 1]
- **Common Ground:** All GND pins across modules, microcontrollers, and external load power supplies **must be tied to a single common ground point**.

---

## 🗂️ Pinout Table (Identical for Both Nodes)

Both **Node 1** (Local Load Controller) and **Node 2** (Remote Mesh Responder) share the exact same hardware pinout setup for consistent peer-to-peer mesh operation[cite: 1].

| Component | Component Pin | ESP32-C6 Pin | Wire Color / Connection Notes | Function |
| :--- | :--- | :--- | :--- | :--- |
| **MOSFET Module** | Gate (SIG) | **GPIO 18** | Signal Wire | PWM signal for variable load dimming / throttling (LED / Fan)[cite: 1] |
| **MOSFET Module** | VCC / GND | External / GND | Power Terminal Block | Connected to external load power supply circuit |
| **5V Relay Module** | IN (Signal) | **GPIO 19** | Signal Wire | Main circuit breaker drive (High-Impedance Active-Low)[cite: 1] |
| **5V Relay Module** | VCC | **5V / VBUS** | Red Power Wire | Powered directly via 5V line from USB-C supply[cite: 1] |
| **5V Relay Module** | GND | **GND** | Black Ground Wire | Connected to common system ground |
| **Push Button** | Terminal 1 | **GPIO 2** | Signal Wire | Short Press = Mesh Relief; Long Press = Blackout Toggle[cite: 1] |
| **Push Button** | Terminal 2 | **GND** | Black Ground Wire | Configured with internal `PULLUP` resistor |
| **Status LED** | Anode (+) | **GPIO 20** | Signal Wire (via 220Ω) | Visual indicator for Thread mesh network attachment[cite: 1] |
| **Status LED** | Cathode (-) | **GND** | Black Ground Wire | Ground return for status indicator LED |

---

## 💡 Wiring & Testing Instructions

1. **USB Serial Monitoring:** Connect Node 1 via USB-C to your PC[cite: 1]. You can open the **HTML Web Serial Dashboard** (`terminal.html`) in Google Chrome to inspect live `Serial.print()` logs at **115200 baud**[cite: 1].
2. **Button Operations:**
   - **Short Press (< 1.5s):** Dispatches a load reduction command across the OpenThread IPv6 mesh over UDP multicast (`ff03::1`)[cite: 1].
   - **Long Press (> 1.5s):** Toggles the 5V relay state directly for total load isolation (blackout simulation)[cite: 1].

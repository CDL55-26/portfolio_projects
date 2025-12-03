# Secure Long-Range Mesh Network — Project README

This document provides a condensed, Markdown-formatted summary of the full ECE 655 final project report: the design and implementation of a secure, long‑range, infrastructure‑independent LoRa mesh network built from the driver level upward.

---

# 1. Introduction

This project explores embedded security, ad‑hoc networking, and long‑range wireless communication by building a **custom mesh network over raw LoRa PHY**, avoiding LoRaWAN to gain full control over framing, reliability, and medium access.

The final system:

* Supports **up to 256 nodes**
* Provides **AES‑256 encrypted messaging**
* Implements **custom MAC + routing behavior**
* Uses a **T9 keypad driver**, **OLED UI**, and **FreeRTOS** state machine

The design serves as a proof‑of‑concept for secure communication in environments without infrastructure.

---

# 2. System Architecture

The device is based on an **ESP32** + **SX1276 LoRa transceiver**. Architecture includes:

## 2.1 Hardware Components

* **ESP32**: dual‑core MCU with hardware crypto acceleration
* **SX1276 LoRa module**: SPI interface + DIO0 interrupt for RX
* **HMI**:

  * 4×4 matrix keypad (T9 input)
  * SSD1306 OLED (I2C)

## 2.2 Software Architecture

Firmware uses **FreeRTOS**, separating networking and HMI into distinct tasks:

| Task                  | Priority | Description                               |
| --------------------- | -------- | ----------------------------------------- |
| `receive_packet_task` | 4        | Handles LoRa interrupts + packet RX queue |
| `keypad_input_task`   | 4        | Processes keypad scans + T9 entry         |

Shared resources protected by mutexes (`LoRa_mutex`, `oled_mutex`). A QueueSet allows the global FSM to block on multiple asynchronous events.

## 2.3 State Machine (SMF)

Primary states:

* **IDLE**
* **SEND_PACKET** (includes CAD / LBT)
* **RETRY_PACKET**
* **PARSE_PACKET**

---

# 3. Mesh Protocol Design

## 3.1 LoRa Settings

* **915 MHz**, **125 kHz BW**, **SF9**, **CR 4/5**, **TX 17 dBm**

## 3.2 Packet Structure

| Byte Offset | Field           | Description                         |
| ----------- | --------------- | ----------------------------------- |
| 0           | Sender ID       | 8‑bit node ID                       |
| 1           | Receiver ID     | 8‑bit target ID                     |
| 2–5         | Sequence Number | 32‑bit monotonic counter            |
| 6           | Flags           | Bitmask (data vs ACK)               |
| 7+          | Payload         | AES‑256‑CBC ciphertext + 16‑byte IV |

Header size: **7 bytes**

---

# 4. Implementation Details

## 4.1 Collision Avoidance

Implemented **Listen‑Before‑Talk (LBT)** using LoRa **CAD**:

* Detects preambles before transmitting
* Aborts TX + applies random backoff (0–200 ms)
* Prevents collisions in multi‑node environments

## 4.2 T9 Text Input

A full multi‑tap T9 system:

* Keypress timing tracked via `KEYPAD_CYCLE_TIMEOUT_MS`
* Repeated key cycles through character map
* Timeout commits the character

## 4.3 Reliability (Stop‑and‑Wait ARQ)

* Packets stored in a circular retry buffer
* Hardware timer enforces 4‑second ACK timeout
* Up to **3 retries** per packet

## 4.4 Encryption

* **AES‑256‑CBC** using ESP32 hardware crypto
* Unique **16‑byte IV per packet** (via hardware RNG)
* Protects against replay, plaintext correlation, and frequency analysis

---

# 5. Results

100‑packet tests at 1 m and 50 m, with and without acknowledgements:

| Mode   | Distance | Sent | Acked | Success Rate | Avg RTT |
| ------ | -------- | ---- | ----- | ------------ | ------- |
| No Ack | 1 m      | 100  | 94    | 94%          | 619 ms  |
| No Ack | 50 m     | 100  | 91    | 91%          | 619 ms  |
| Ack    | 1 m      | 100  | 100   | **100%**     | 738 ms  |
| Ack    | 50 m     | 100  | 100   | **100%**     | 802 ms  |

**Key Observations:**

* ARQ yields *perfect reliability*, correcting ~9% raw packet loss.
* Latency increases because retries occur more often at longer distances.

---

# 6. Design Challenges

## 6.1 Architectural Technical Debt

Reliability logic was added late, forcing complex integration into an FSM originally designed for simplex TX. A cleaner design would:

* Split parsing, TX, and retries into separate tasks
* Use the FSM only for orchestration

## 6.2 Stack Overflow Bug

Unpredictable packet TX failures were caused by a **silent stack overflow** in `keypad_input_task`:

* Initial stack: **2048 bytes** → insufficient
* Expanding to **4096 bytes** fixed the corruption

Reveals the importance of profiling FreeRTOS task stack usage.

---

# 7. Future Work

## 7.1 Security

* Add **HMAC** for integrity
* Replace static symmetric key with **Diffie–Hellman session key exchange**

## 7.2 Routing

* Replace broadcast flooding with hop‑count‑based routing tables
* Improve scalability beyond small meshes

## 7.3 User Interface

* Add word‑wrapping
* Add scrollable message history
* Implement multi‑packet fragmentation for long messages

---

# Summary

This project demonstrates a fully custom, secure LoRa mesh networking device built from the hardware driver level to encryption and UI logic. The system highlights the challenges of designing protocols from scratch, the value of robust RTOS design, and the necessity of careful resource management in embedded systems.


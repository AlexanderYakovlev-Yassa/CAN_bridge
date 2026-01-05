# Hardware Setup Guide - ESP32 CAN Bridge

This guide explains how to connect your ESP32 to a CAN transceiver and vehicle CAN bus.

## Components Required

- **ESP32 Development Board** (with USB-C connector)
- **SN65HVD230 CAN Transceiver Module** (or compatible: TJA1050, MCP2551)
- **Jumper Wires** (female-to-female or female-to-male depending on your boards)
- **Optional**: Breadboard for prototyping

---

## SN65HVD230 CAN Transceiver Pinout

The SN65HVD230 is a 3.3V CAN transceiver, ideal for ESP32 (which operates at 3.3V logic).

```
SN65HVD230 Module
┌─────────────┐
│    3V3      │ ← 3.3V Power Supply
│    GND      │ ← Ground
│    CTX      │ ← CAN Controller TX (from ESP32)
│    CRX      │ ← CAN Controller RX (to ESP32)
│    CANH     │ ← CAN High (to CAN Bus)
│    CANL     │ ← CAN Low (to CAN Bus)
└─────────────┘
```

---

## Wiring Diagram

### ESP32 to SN65HVD230 Connection

```
┌──────────────────┐              ┌──────────────────┐
│                  │              │   SN65HVD230     │
│      ESP32       │              │   Transceiver    │
│                  │              │                  │
│                  │              │                  │
│   3.3V    ●──────┼──────────────┼────● 3V3         │
│                  │              │                  │
│   GND     ●──────┼──────────────┼────● GND         │
│                  │              │                  │
│   GPIO4   ●──────┼──────────────┼────● CTX         │
│   (TX)           │              │                  │
│                  │              │                  │
│   GPIO5   ●──────┼──────────────┼────● CRX         │
│   (RX)           │              │                  │
│                  │              │                  │
│                  │              │   CANH    ●──────┼──── To CAN Bus H
│                  │              │                  │
│                  │              │   CANL    ●──────┼──── To CAN Bus L
│                  │              │                  │
└──────────────────┘              └──────────────────┘
```

### Connection Table

| ESP32 Pin | SN65HVD230 Pin | Description                    |
|-----------|----------------|--------------------------------|
| 3.3V      | 3V3            | Power supply (3.3V)            |
| GND       | GND            | Ground                         |
| GPIO4     | CTX            | CAN TX (ESP32 → Transceiver)   |
| GPIO5     | CRX            | CAN RX (Transceiver → ESP32)   |
| -         | CANH           | Connect to Vehicle CAN High    |
| -         | CANL           | Connect to Vehicle CAN Low     |

> **Note**: GPIO pins 4 and 5 are the default configuration. You can change these in `idf.py menuconfig` under "CAN Bridge Configuration".

---

## Connecting to Vehicle CAN Bus

### OBD-II Port Connection (Most Common)

Most vehicles have CAN bus signals available on the OBD-II diagnostic port:

```
OBD-II Connector (Looking at the connector face)
        ┌─────────────────┐
        │  1  2  3  4  5  6│
        │ 7  8  9 10 11 12 │
        └─────────────────┘
```

**Standard CAN Bus Pins:**
- **Pin 6**: CAN High (CANH) - Connect to SN65HVD230 CANH
- **Pin 14**: CAN Low (CANL) - Connect to SN65HVD230 CANL
- **Pin 4 or 5**: Ground (Optional, for reference)

**Wiring:**
```
SN65HVD230 CANH  ──→  OBD-II Pin 6 (CAN High)
SN65HVD230 CANL  ──→  OBD-II Pin 14 (CAN Low)
```

### Infotainment System Connection

If connecting directly to your car's infotainment CAN bus:

1. **Locate the CAN wires** behind the head unit (usually twisted pair)
2. **Identify CAN High and CAN Low** using a multimeter:
   - With ignition ON, CANH typically reads ~2.5-3.5V
   - CANL typically reads ~1.5-2.5V
   - When idle, both should be around 2.5V
3. **Connect the transceiver**:
   ```
   SN65HVD230 CANH  ──→  Vehicle CAN High (often yellow or white wire)
   SN65HVD230 CANL  ──→  Vehicle CAN Low (often green or blue wire)
   ```

> **⚠️ Warning**: Always verify voltage levels before connecting. CAN buses operate at 0-5V differential. Connecting to wrong wires can damage your transceiver or vehicle electronics.

---

## Complete System Diagram

```
                                       Vehicle CAN Bus
                                            │
                                  ┌─────────┴─────────┐
                                  │                   │
                              CANH│                   │CANL
                                  │                   │
                    ┌─────────────┴───────────────────┴─────────┐
                    │                                            │
                    │        SN65HVD230 Transceiver              │
                    │                                            │
                    │  3V3  GND  CTX  CRX  CANH  CANL           │
                    └───┬───┬────┬────┬───────────────────────┬─┘
                        │   │    │    │
                        │   │    │    │
         ┌──────────────┴───┴────┴────┴───────────┐
         │                                         │
         │           ESP32 Board                   │
         │                                         │
         │  3.3V  GND  GPIO4  GPIO5                │
         │                                         │
         │              USB-C                      │
         └────────────────┬────────────────────────┘
                          │
                          │ USB Cable
                          │
                    ┌─────┴──────┐
                    │            │
                    │     PC     │
                    │  SavvyCAN  │
                    │            │
                    └────────────┘
```

---

## Assembly Steps

### Step 1: Prepare Components
- Ensure ESP32 is unpowered
- Check SN65HVD230 module for any shorts or damage
- Gather appropriate jumper wires

### Step 2: Wire Power Connections
1. Connect ESP32 **3.3V** to SN65HVD230 **3V3**
2. Connect ESP32 **GND** to SN65HVD230 **GND**

> **Important**: Do NOT use 5V for SN65HVD230 - it requires 3.3V

### Step 3: Wire Data Connections
1. Connect ESP32 **GPIO4** to SN65HVD230 **CTX**
2. Connect ESP32 **GPIO5** to SN65HVD230 **CRX**

### Step 4: Connect to CAN Bus
1. Identify vehicle CAN H and CAN L wires
2. Connect SN65HVD230 **CANH** to vehicle **CAN High**
3. Connect SN65HVD230 **CANL** to vehicle **CAN Low**

### Step 5: Verify Connections
- Double-check all connections against the wiring diagram
- Ensure no shorts between power and ground
- Verify GPIO assignments match your configuration

### Step 6: Power On
1. Turn on vehicle ignition (for OBD-II connection)
2. Connect ESP32 to PC via USB
3. Open serial monitor to see auto-detection progress

---

## Breadboard Layout Example

```
                Breadboard
     ┌───────────────────────────────┐
     │                               │
     │  ┌──────┐        ┌─────────┐  │
     │  │ESP32 │        │SN65HVD230│ │
     │  │      │        │         │  │
     │  │3.3V──┼────────┼──3V3    │  │
     │  │GND───┼────────┼──GND    │  │
     │  │GPIO4─┼────────┼──CTX    │  │
     │  │GPIO5─┼────────┼──CRX    │  │
     │  │      │        │CANH─────┼──┼── Red wire to CAN H
     │  │      │        │CANL─────┼──┼── Blue wire to CAN L
     │  │      │        │         │  │
     │  └──────┘        └─────────┘  │
     │                               │
     └───────────────────────────────┘
```

---

## CAN Bus Termination

CAN buses require 120Ω termination resistors at each end of the bus:

- **If you're tapping into an existing CAN bus** (like OBD-II or infotainment), the bus is already terminated by the vehicle - **no additional resistor needed**
- **If you're creating a test setup** between two devices, add 120Ω resistor between CANH and CANL at each end

Most SN65HVD230 modules have a built-in 120Ω termination resistor that can be enabled/disabled via jumper.

**For vehicle connection: Keep termination jumper OFF (disabled)**

---

## Troubleshooting

### No CAN Traffic Detected

1. **Check Power**:
   - Measure 3.3V between 3V3 and GND on transceiver
   - ESP32 should power on (LED should be lit)

2. **Verify Wiring**:
   - Double-check GPIO4 → CTX and GPIO5 → CRX
   - Ensure CANH and CANL are not swapped

3. **Check Vehicle**:
   - Ignition must be ON for OBD-II CAN bus
   - Some vehicles require engine running for CAN traffic
   - Try different CAN buses (some vehicles have multiple)

4. **Measure CAN Bus Voltage**:
   - With ignition ON and active traffic:
     - CANH: ~2.5-3.5V idle, 3.5-4.5V active
     - CANL: ~1.5-2.5V idle, 0.5-1.5V active
     - Differential: ~2V during transmission

5. **Check Transceiver**:
   - Some modules have a standby pin (S/RS) - ensure it's LOW or unconnected
   - Verify the module is genuine SN65HVD230 (not counterfeit)

### ESP32 Won't Boot

- **Short circuit**: Check for accidental shorts
- **Wrong voltage**: Ensure using 3.3V, not 5V for transceiver
- **USB power insufficient**: Some transceivers draw significant current, use quality USB cable

### CAN Bus Errors

- **Bus-Off state**: Usually due to wrong bitrate - auto-detection will try multiple rates
- **High error count**: Check for loose connections or poor quality jumper wires
- **No ACK**: Ensure you're connected to an active CAN bus with other nodes

---

## Alternative Transceivers

If you can't find SN65HVD230, these transceivers also work:

| Transceiver | Voltage | Speed      | Notes                           |
|-------------|---------|------------|---------------------------------|
| **SN65HVD230** | 3.3V    | 1 Mbps     | Ideal for ESP32 (native 3.3V)   |
| **TJA1050**    | 5V      | 1 Mbps     | Requires level shifter for ESP32|
| **MCP2551**    | 5V      | 1 Mbps     | Requires level shifter for ESP32|
| **MCP2562**    | 5V      | 1 Mbps     | Fault-tolerant variant          |

> **⚠️ Important**: If using 5V transceivers (TJA1050, MCP2551), you MUST use a level shifter between ESP32 and the transceiver, as ESP32 GPIOs are not 5V tolerant!

---

## Safety Notes

- ⚠️ **Never connect/disconnect while vehicle is running**
- ⚠️ **Use proper insulation** on all exposed wires
- ⚠️ **Double-check polarity** before powering on
- ⚠️ **Don't use damaged cables** or transceivers
- ⚠️ **Test in a safe environment** first (not while driving)
- ⚠️ **Keep away from moving parts** in engine bay
- ℹ️ **Reverse polarity** on CAN bus (swapping H and L) usually won't cause damage, but won't work

---

## Testing Without a Vehicle

For testing the setup without a vehicle, you can use loopback mode:

1. Configure in `can_bridge_main.c` or add a test mode
2. Enable loopback and self-test flags
3. Frames sent by ESP32 will be received back

This helps verify:
- ESP32 is working correctly
- Transceiver is functional
- Code is operating properly

---

## Additional Resources

- **SN65HVD230 Datasheet**: [Texas Instruments](https://www.ti.com/product/SN65HVD230)
- **CAN Bus Standard**: ISO 11898
- **OBD-II Pinout**: [Wikipedia OBD-II PIDs](https://en.wikipedia.org/wiki/OBD-II_PIDs)
- **SavvyCAN Software**: [SavvyCAN GitHub](https://github.com/collin80/SavvyCAN)

---

## Support

If you encounter issues:

1. Check the `README.md` troubleshooting section
2. Monitor serial output: `idf.py -p /dev/ttyACM0 monitor`
3. Verify hardware connections with multimeter
4. Test with a known-good CAN bus

**Happy CAN sniffing! 🚗📡**

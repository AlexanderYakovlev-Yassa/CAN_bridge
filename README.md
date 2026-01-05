| Supported Targets: ESP32 

# CAN Bridge for SavvyCAN

This project implements a **CAN-to-USB bridge** for analyzing automotive CAN bus traffic using **SavvyCAN** on a PC. The ESP32 automatically detects the CAN bus bitrate and frame format, then forwards all traffic to the PC using the SLCAN protocol.

## Features

- **Automatic Bitrate Detection**: Automatically detects CAN bus speed (125k, 250k, 500k, 1M bps)
- **SLCAN Protocol**: Compatible with SavvyCAN and other SLCAN-based tools
- **USB CDC Interface**: Appears as a standard serial port on your PC
- **Zero Configuration**: Just connect and it works - no manual setup required
- **Full Traffic Forwarding**: All CAN frames are forwarded without filtering
- **Support for Standard (11-bit) and Extended (29-bit) CAN IDs**

## Hardware Requirements

- ESP32 development board (with USB-C connector recommended)
- CAN transceiver module:
  - **SN65HVD230** (recommended - 3.3V native, ideal for ESP32)
  - TJA1050 or MCP2551 (5V - requires level shifter)
- Jumper wires (female-to-female or female-to-male)
- Optional: Breadboard for prototyping

## Hardware Setup

### Quick Connection Guide

Connect the ESP32 to a CAN transceiver:

```
ESP32 Pin     SN65HVD230    CAN Bus / Vehicle
---------     -----------   ------------------
GPIO4 (TX) -->  CTX
GPIO5 (RX) <--  CRX
3.3V       -->  3V3
GND        -->  GND
              CANH       <-->  OBD-II Pin 3 (CAN High)
              CANL       <-->  OBD-II Pin 11 (CAN Low)
              GND        <-->  OBD-II Pin 5 (Signal GND)
```

**Important Notes**:
- GPIO pins can be configured in menuconfig under "CAN Bridge Configuration"
- Use **3.3V** for SN65HVD230 (NOT 5V!)
- For OBD-II connections: CAN High is pin 3, CAN Low is pin 11
- Disable termination resistor on transceiver module when tapping into existing vehicle CAN bus

### Detailed Setup

For complete wiring diagrams, breadboard layouts, OBD-II pinouts, alternative transceivers, troubleshooting, and safety notes, see:

📖 **[docs/HARDWARE_SETUP.md](docs/HARDWARE_SETUP.md)**

The hardware guide includes:
- Detailed wiring diagrams and connection tables
- OBD-II and infotainment system connection instructions
- CAN bus voltage testing and verification procedures
- Alternative transceiver options and compatibility
- Complete troubleshooting section
- Safety warnings and best practices

## Quick Start

### 1. Build and Flash

```bash
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### 2. Connect to Vehicle CAN Bus

Connect the transceiver to your car's infotainment CAN bus (usually CANH and CANL pins on OBD-II connector or head unit).

### 3. Wait for Auto-Detection

The ESP32 will automatically:
- Try different bitrates starting with 125kbps (most common for infotainment)
- Detect valid CAN traffic
- Initialize the bridge

You'll see output like:
```
I (1234) can_autodetect: Testing bitrate: 125000 bps
I (2345) can_autodetect: Valid frame detected at 125000 bps!
I (2346) can_bridge: ✓ CAN bitrate detected: 125000 bps
I (2347) can_bridge: Bridge is now running!
```

### 4. Connect with SavvyCAN

1. Open **SavvyCAN** on your PC
2. Go to **Connection → Open Connection Window**
3. Select **Serial Connection**
4. Choose the ESP32 serial port (e.g., `/dev/ttyACM0` on Linux, `COMx` on Windows)
5. Set protocol to **SLCAN**
6. Set baud rate to **115200** (standard for USB CDC)
7. Click **Connect**

You should now see CAN traffic in SavvyCAN!

## Configuration

Run `idf.py menuconfig` to configure:

### CAN Bridge Configuration
- **CAN TX GPIO**: GPIO pin for CAN TX (default: 4)
- **CAN RX GPIO**: GPIO pin for CAN RX (default: 5)

The auto-detection will try these bitrates in order:
1. 125 kbps (most common for infotainment systems)
2. 250 kbps
3. 500 kbps
4. 1000 kbps (1 Mbps)
5. 100 kbps
6. 50 kbps

## SLCAN Protocol

The bridge implements the SLCAN (Serial Line CAN) protocol, which is widely supported by CAN analysis tools. Supported commands:

| Command | Description |
|---------|-------------|
| `Sn` | Set bitrate (S0=10k, S4=125k, S5=250k, S6=500k, S8=1M) |
| `O` | Open CAN channel |
| `C` | Close CAN channel |
| `V` | Get hardware version |
| `v` | Get firmware version |
| `N` | Get serial number |
| `Zn` | Enable/disable timestamps (Z0=off, Z1=on) |
| `F` | Read status flags |

### Frame Format

- Standard frames: `tiiildd...`
- Extended frames: `Tiiiiiiiildd...`
- RTR frames: `riiil`, `Riiiiiiiil`

Where:
- `t`/`T` = standard/extended data frame
- `r`/`R` = standard/extended RTR frame
- `iii(iiiii)` = CAN ID in hex
- `l` = DLC (data length)
- `dd` = data bytes in hex

## Troubleshooting

### No Bitrate Detected

If auto-detection fails:
1. Check CAN transceiver connections
2. Verify the CAN bus has active traffic
3. Check GPIO configuration matches your wiring
4. Verify 3.3V and GND connections
5. Try connecting to a different CAN bus (some vehicles have multiple buses)

### SavvyCAN Connection Issues

1. Ensure the correct serial port is selected
2. Use 115200 baud rate
3. Select SLCAN protocol
4. Make sure no other application is using the serial port

### Monitor Output

To see debug information:
```bash
idf.py -p /dev/ttyACM0 monitor
```

## Technical Details

- **Protocol**: SLCAN (Serial Line CAN)
- **USB Interface**: USB CDC (Virtual COM Port)
- **Default Baud Rate**: 115200 bps
- **Auto-detection Timeout**: 2 seconds per bitrate
- **RX Queue Size**: 50 frames
- **TX Queue Size**: 10 frames

## Supported Targets

All ESP32 variants with TWAI (CAN) and USB CDC support.

## License

This project is licensed under CC0-1.0 (Public Domain).

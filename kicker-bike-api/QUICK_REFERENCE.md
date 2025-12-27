# Quick Reference Guide

## Overview

This is a condensed reference for developers implementing the KICKR BIKE protocol. For detailed information, see the full documentation.

## Essential UUIDs

```
Service:      0000FC82-0000-1000-8000-00805F9B34FB  (Zwift Ride)
Sync RX:      00000003-19CA-4651-86E5-FA29DCDD09D1  (Write)
Async TX:     00000002-19CA-4651-86E5-FA29DCDD09D1  (Notify)
Sync TX:      00000004-19CA-4651-86E5-FA29DCDD09D1  (Notify)
FTMS Sim:     00002AD5-0000-1000-8000-00805F9B34FB  (Indoor Bike Simulation)
```

## mDNS Service

```
Service Type: _wahoo-fitness-tnp._tcp
Port: 36867
TXT Records:
  - ble-service-uuids=FC82
  - mac-address=XX-XX-XX-XX-XX-XX
  - serial-number=XXXXXXXXX
```

## TCP Message Format

```
[Version] [MsgID] [Seq] [RespCode] [Length-HI] [Length-LO] [Body...]
   0x01    0x01-06  0-255    0x00      0-255       0-255      ...
```

## Message IDs

```
0x01 - DISCOVER_SERVICES
0x02 - DISCOVER_CHARACTERISTICS
0x03 - READ_CHARACTERISTIC
0x04 - WRITE_CHARACTERISTIC
0x05 - ENABLE_CHARACTERISTIC_NOTIFICATIONS
0x06 - CHARACTERISTIC_NOTIFICATION
```

## Handshake Sequence

```
1. Zwift → WRITE "RideOn" (0x52 0x69 0x64 0x65 0x4f 0x6e)
2. Trainer → ACK write
3. Trainer → NOTIFY "RideOn" + signature (0x52 0x69 0x64 0x65 0x4f 0x6e 0x01 0x03)
4. Trainer → Keep-alive every 5 seconds
```

## Button Opcodes

```
0x07 - CONTROLLER_NOTIFICATION (button presses)
0x19 - BATTERY_NOTIF
0x31 - LOG_DATA
0x52 - RIDE_ON (handshake)
```

## Button Masks

```dart
LEFT_BTN      = 0x00001  // Nav left
UP_BTN        = 0x00002  // Nav up
RIGHT_BTN     = 0x00004  // Nav right
DOWN_BTN      = 0x00008  // Nav down

A_BTN         = 0x00010  // Green button
B_BTN         = 0x00020  // Pink button
Y_BTN         = 0x00040  // Blue button
Z_BTN         = 0x00080  // Orange button

SHFT_UP_L     = 0x00100  // Left shifter up (easier)
SHFT_DN_L     = 0x00200  // Left shifter down (easier)
POWERUP_L     = 0x00400  // Left power-up
ONOFF_L       = 0x00800  // Left power button

SHFT_UP_R     = 0x01000  // Right shifter up (harder)
SHFT_DN_R     = 0x02000  // Right shifter down (harder)
POWERUP_R     = 0x04000  // Right power-up
ONOFF_R       = 0x08000  // Right power button
```

**Important**: Button pressed = bit CLEAR (0), Released = bit SET (1)

## Shifter Mapping

```
Right Shifters (up/down) → Shift UP → Harder → Increase gear
Left Shifters (up/down)  → Shift DOWN → Easier → Decrease gear
```

## Gear Example

```dart
// 24-speed gear system
static const gearRatios = [
  0.50,  // Gear 1 (easiest)
  0.55, 0.60, 0.65, 0.70, 0.75, 0.80, 0.85, 0.90, 0.95,
  1.00,  // Gear 11
  1.05,  // Gear 12 (default)
  1.10, 1.15, 1.20, 1.25, 1.30, 1.35, 1.40, 1.45,
  1.50, 1.55, 1.60, 1.65,  // Gear 24 (hardest)
];

// Apply to FTMS
final effectiveGrade = baseGrade * gearRatios[currentGear];
updateFTMSSimulation(grade: effectiveGrade.clamp(-20.0, 20.0));
```

## FTMS Simulation Parameters Encoding

```dart
final data = ByteData(7);
data.setInt16(0, (windSpeed * 1000).round(), Endian.little);  // m/s → mm/s
data.setInt16(2, (grade * 100).round(), Endian.little);       // % → 0.01%
data.setUint8(4, (crr * 10000).round());                      // → 0.0001
data.setUint8(5, (cw * 100).round());                         // → 0.01
```

## Protocol Flow Diagram

```
┌───────┐                                    ┌─────────┐
│ Zwift │                                    │ Trainer │
└───┬───┘                                    └────┬────┘
    │                                             │
    │  1. TCP Connect (port 36867)                │
    ├────────────────────────────────────────────►│
    │                                             │
    │  2. DISCOVER_SERVICES                       │
    ├────────────────────────────────────────────►│
    │                                             │
    │  3. FC82 Service UUID                       │
    │◄────────────────────────────────────────────┤
    │                                             │
    │  4. DISCOVER_CHARACTERISTICS                │
    ├────────────────────────────────────────────►│
    │                                             │
    │  5. 3 Characteristics + Properties          │
    │◄────────────────────────────────────────────┤
    │                                             │
    │  6. ENABLE_NOTIFICATIONS (Async TX)         │
    ├────────────────────────────────────────────►│
    │                                             │
    │  7. ENABLE_NOTIFICATIONS (Sync TX)          │
    ├────────────────────────────────────────────►│
    │                                             │
    │  8. WRITE "RideOn"                          │
    ├────────────────────────────────────────────►│
    │                                             │
    │  9. NOTIFY "RideOn" + signature             │
    │◄────────────────────────────────────────────┤
    │                                             │
    │  10. Keep-alive (every 5s)                  │
    │◄────────────────────────────────────────────┤
    │                                             │
    │  [Button pressed on controller]             │
    │                                             │
    │  11. NOTIFY CONTROLLER_NOTIFICATION         │
    │◄────────────────────────────────────────────┤
    │                                             │
    │  [Trainer updates FTMS with new grade]      │
    │                                             │
```

## Code Snippets

### Parse Button Press

```dart
void handleControllerNotification(Uint8List data) {
  final opcode = data[0];  // 0x07
  final status = RideKeyPadStatus.fromBuffer(data.sublist(1));
  
  // Check right shift up (harder)
  if (status.buttonMap & 0x01000 == 0) {
    gearSystem.shiftUp();
    applyGearChange();
  }
  
  // Check left shift up (easier)
  if (status.buttonMap & 0x00100 == 0) {
    gearSystem.shiftDown();
    applyGearChange();
  }
}
```

### Send Button Notification

```dart
void sendButtonPress(int buttonMask) {
  final status = RideKeyPadStatus()
    ..buttonMap = (~buttonMask) & 0xFFFFFFFF
    ..analogPaddles.clear();
  
  final notification = [
    0x07,  // CONTROLLER_NOTIFICATION
    ...status.writeToBuffer(),
  ];
  
  sendNotification('00000002-19CA-4651-86E5-FA29DCDD09D1', notification);
}
```

### Update FTMS

```dart
void applyGearChange() {
  final ratio = gearRatios[currentGear];
  final effectiveGrade = (baseGrade * ratio).clamp(-20.0, 20.0);
  
  final data = ByteData(7);
  data.setInt16(0, 0, Endian.little);  // No wind
  data.setInt16(2, (effectiveGrade * 100).round(), Endian.little);
  data.setUint8(4, 40);   // CRR = 0.004
  data.setUint8(5, 51);   // CW = 0.51
  
  writeFTMSCharacteristic('00002AD5-0000-1000-8000-00805F9B34FB', 
                          data.buffer.asUint8List());
}
```

## Testing Commands

```bash
# Test mDNS advertisement
dns-sd -B _wahoo-fitness-tnp._tcp

# Test TCP connection
nc localhost 36867

# Check firewall
sudo ufw allow 5353/udp
sudo ufw allow 36867/tcp

# Capture traffic
sudo tcpdump -i any port 36867 -w capture.pcap
```

## Common Issues

| Issue | Solution |
|-------|----------|
| Service not found | Check mDNS TXT records |
| Connection closes | Send RideOn response on Sync TX |
| Buttons not detected | Check inverted logic (pressed = 0) |
| No resistance change | Verify FTMS characteristic write |
| Zwift times out | Send keep-alive every 5 seconds |

## Performance Tips

1. Cache protobuf objects
2. Reuse byte buffers
3. Batch FTMS updates (100ms debounce)
4. Disable debug logging in production
5. Handle disconnections gracefully

## Minimum Implementation

To get a basic working implementation:

1. ✅ Advertise mDNS service
2. ✅ Accept TCP on port 36867
3. ✅ Handle DISCOVER_SERVICES → Return FC82
4. ✅ Handle DISCOVER_CHARACTERISTICS → Return 3 chars
5. ✅ Handle WRITE "RideOn" → Send "RideOn" + 0x01 0x03
6. ✅ Send keep-alive every 5 seconds
7. ✅ Parse CONTROLLER_NOTIFICATION (opcode 0x07)
8. ✅ Map buttons 0x00100, 0x01000 to shift up/down
9. ✅ Update FTMS simulation grade based on gear

## Full Documentation

See the complete guides:
- [README.md](README.md) - Overview
- [MDNS_SERVICE.md](MDNS_SERVICE.md) - Service advertisement
- [TCP_PROTOCOL.md](TCP_PROTOCOL.md) - Message protocol
- [BLE_GATT.md](BLE_GATT.md) - GATT simulation
- [ZWIFT_RIDE_PROTOCOL.md](ZWIFT_RIDE_PROTOCOL.md) - Button protocol
- [GEAR_AND_INCLINE.md](GEAR_AND_INCLINE.md) - Detailed gear examples
- [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md) - Step-by-step guide
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Debug help

## Reference Code

BikeControl implementation:
- `lib/bluetooth/devices/zwift/ftms_mdns_emulator.dart` - Complete working example
- `lib/bluetooth/devices/zwift/zwift_ride.dart` - Button parsing
- `lib/bluetooth/devices/zwift/protocol/` - Protobuf definitions

## License

This documentation is based on reverse engineering for educational and interoperability purposes. Respect Wahoo's and Zwift's intellectual property and terms of service.

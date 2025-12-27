# Protocol Buffers - Message Schemas

## Overview

Zwift devices use Protocol Buffers (protobuf) for serializing button states, device information, and other data. This document explains the protobuf messages used in the Zwift Hardware API.

## What is Protocol Buffers?

Protocol Buffers is a language-neutral, platform-neutral mechanism for serializing structured data. It's similar to JSON or XML but smaller, faster, and simpler.

### Key Concepts

- **Messages**: Structured data types (like classes or structs)
- **Fields**: Named, typed elements within messages
- **Enumerations**: Named integer constants
- **Varints**: Variable-length integer encoding (space-efficient)

## Protobuf Files Location

The BikeControl implementation contains generated Dart protobuf files:

```
lib/bluetooth/devices/zwift/protocol/
├── zwift.pb.dart         # Legacy device messages (Click, Play)
├── zwift.pbenum.dart     # Enumerations for legacy devices
├── zwift.pbjson.dart     # JSON representation
├── zp.pb.dart            # Zwift Protocol messages (Ride, Click v2)
├── zp.pbenum.dart        # ZP enumerations
├── zp.pbjson.dart        # ZP JSON representation
├── zp_vendor.pb.dart     # Vendor-specific messages
└── zp_vendor.pbenum.dart # Vendor enumerations
```

**Note**: The original `.proto` source files are not included in the repository. These are reverse-engineered specifications.

## Common Enumerations

### PlayButtonStatus

Used by all devices to represent button state:

```protobuf
enum PlayButtonStatus {
  ON = 0;   // Button pressed
  OFF = 1;  // Button released
}
```

**Usage**:
```dart
if (status.buttonA == PlayButtonStatus.ON) {
  print('A button pressed');
}
```

## Legacy Device Messages (Click, Play)

### ClickKeyPadStatus

**Used by**: Zwift Click

```protobuf
message ClickKeyPadStatus {
  PlayButtonStatus buttonPlus = 1;   // Plus (+) button
  PlayButtonStatus buttonMinus = 2;  // Minus (-) button
}
```

**Binary Format Example**:
```
// Plus button pressed
[0x08, 0x00]  // Field 1 (buttonPlus) = ON (0)

// Minus button pressed
[0x10, 0x00]  // Field 2 (buttonMinus) = ON (0)

// Both pressed
[0x08, 0x00, 0x10, 0x00]
```

**Parsing**:
```dart
final status = ClickKeyPadStatus.fromBuffer(bytes);
if (status.buttonPlus == PlayButtonStatus.ON) {
  handleButtonPress('plus');
}
```

---

### PlayKeyPadStatus

**Used by**: Zwift Play

```protobuf
message PlayKeyPadStatus {
  PlayButtonStatus rightPad = 1;      // Grip sensor (mode switch)
  PlayButtonStatus buttonYUp = 2;     // Y/Up button
  PlayButtonStatus buttonZLeft = 3;   // Z/Left button
  PlayButtonStatus buttonARight = 4;  // A/Right button
  PlayButtonStatus buttonBDown = 5;   // B/Down button
  PlayButtonStatus buttonOn = 6;      // Power button
  PlayButtonStatus buttonShift = 7;   // Side button
  sint32 analogLR = 8;                // Paddle: -100 to +100
}
```

**Field Encoding**:
- Fields 1-7: Enum values (varint encoded)
- Field 8: Signed 32-bit integer (zigzag + varint encoding)

**Binary Format Example**:
```
// A button pressed (action mode)
[0x08, 0x00,  // rightPad = ON
 0x20, 0x00]  // buttonARight = ON

// Paddle pulled to 100
[0x08, 0x00,  // rightPad = ON
 0x40, 0x64]  // analogLR = 100 (varint)
```

**Parsing**:
```dart
final status = PlayKeyPadStatus.fromBuffer(bytes);

if (status.rightPad == PlayButtonStatus.ON) {
  // Action mode
  if (status.buttonARight == PlayButtonStatus.ON) {
    handleButtonPress('A');
  }
}

if (status.analogLR.abs() == 100) {
  handlePaddlePull();
}
```

## Modern Protocol Messages (Ride, Click v2)

### Opcode Enumeration

All modern messages start with an opcode:

```protobuf
enum Opcode {
  GET = 0x08;                        // 8
  GET_RESPONSE = 0x3C;               // 60
  STATUS_RESPONSE = 0x12;            // 18
  CONTROLLER_NOTIFICATION = 0x07;    // 7
  BATTERY_NOTIF = 0x19;              // 25
  LOG_DATA = 0x31;                   // 49
  RESET = 0x22;                      // 34
  RIDE_ON = 0x52;                    // 82 ('R')
  // ... more opcodes
}
```

---

### RideKeyPadStatus

**Used by**: Zwift Ride, Click v2

```protobuf
message RideKeyPadStatus {
  uint32 buttonMap = 1;                         // Bit mask of buttons
  repeated RideAnalogKeyPress analogPaddles = 3; // Analog paddle data
}
```

**Button Map Encoding**:

The `buttonMap` is a 32-bit unsigned integer where each bit represents a button:

```
Bit  | Hex Value | Button
-----|-----------|------------------
 0   | 0x00001   | LEFT_BTN
 1   | 0x00002   | UP_BTN
 2   | 0x00004   | RIGHT_BTN
 3   | 0x00008   | DOWN_BTN
 4   | 0x00010   | A_BTN
 5   | 0x00020   | B_BTN
 6   | 0x00040   | Y_BTN
 7   | 0x00080   | Z_BTN
 8   | 0x00100   | SHFT_UP_L_BTN
 9   | 0x00200   | SHFT_DN_L_BTN
10   | 0x00400   | POWERUP_L_BTN
11   | 0x00800   | ONOFF_L_BTN
12   | 0x01000   | SHFT_UP_R_BTN
13   | 0x02000   | SHFT_DN_R_BTN
14   | 0x04000   | POWERUP_R_BTN
15   | 0x08000   | ONOFF_R_BTN
```

**Important**: Button pressed = bit is 0 (inverted logic!)

**Binary Format Example**:
```
// No buttons pressed (all released)
[0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F]
// buttonMap = 0xFFFFFFFF (all bits set)

// A button pressed (bit 4 = 0)
[0x08, 0xEF, 0xFF, 0xFF, 0xFF, 0x0F]
// buttonMap = 0xFFFFFFEF (bit 4 cleared)
```

**Parsing**:
```dart
final status = RideKeyPadStatus.fromBuffer(bytes);

// Check each button (0 = pressed)
if (status.buttonMap & 0x00001 == 0) {
  handleButtonPress('LEFT');
}

if (status.buttonMap & 0x00010 == 0) {
  handleButtonPress('A');
}
```

---

### RideAnalogKeyPress

**Used in**: RideKeyPadStatus (repeated field)

```protobuf
message RideAnalogKeyPress {
  RideAnalogKeyLocation location = 1;  // Which paddle
  sint32 analogValue = 2;              // Position (-100 to +100)
}

enum RideAnalogKeyLocation {
  L0 = 0;  // Left paddle
  L1 = 1;  // Right paddle
  L2 = 2;  // Reserved
  L3 = 3;  // Reserved
}
```

**Binary Format Example**:
```
// Left paddle at 100%
[0x1A, 0x04,      // Field 3 tag + length
 0x08, 0x00,      // location = L0
 0x10, 0x64]      // analogValue = 100

// Right paddle at -50%
[0x1A, 0x04,
 0x08, 0x01,      // location = L1
 0x10, 0x9C, 0x01] // analogValue = -50 (zigzag encoded)
```

**Parsing**:
```dart
for (final paddle in status.analogPaddles) {
  if (paddle.analogValue.abs() >= 25) {  // Threshold
    switch (paddle.location) {
      case RideAnalogKeyLocation.L0:
        handlePaddlePress('left', paddle.analogValue);
        break;
      case RideAnalogKeyLocation.L1:
        handlePaddlePress('right', paddle.analogValue);
        break;
    }
  }
}
```

---

### BatteryNotification

**Used by**: Zwift Ride, Click v2

```protobuf
message BatteryNotification {
  uint32 newPercLevel = 1;  // Battery percentage (0-100)
}
```

**Binary Format Example**:
```
// 100% battery
[0x08, 0x64]  // Field 1 = 100

// 50% battery
[0x08, 0x32]  // Field 1 = 50

// 10% battery
[0x08, 0x0A]  // Field 1 = 10
```

**Parsing**:
```dart
final notification = BatteryNotification.fromBuffer(bytes);
updateBatteryLevel(notification.newPercLevel);
```

---

### Get and GetResponse

**Request data objects**:

```protobuf
message Get {
  uint32 dataObjectId = 1;  // Which object to retrieve
}

message GetResponse {
  uint32 dataObjectId = 1;       // Which object this is
  bytes dataObjectData = 2;      // Serialized data object
}
```

**Data Object IDs**:

```protobuf
enum DO {
  PAGE_DEV_INFO = 0x0000;                      // Device information
  PAGE_DATE_TIME = 0x0030;                     // Date/time
  PAGE_CLIENT_SERVER_CONFIGURATION = 0x0010;   // Config
  PAGE_CONTROLLER_INPUT_CONFIG = 0x0880;       // Input config
  BATTERY_STATE = 0x0683;                      // Battery info
}
```

**Example - Request Device Info**:
```dart
final get = Get(dataObjectId: DO.PAGE_DEV_INFO.value);
final command = Uint8List.fromList([
  Opcode.GET.value,
  ...get.writeToBuffer(),
]);

await sendCommand(command);
// Sends: [0x08, 0x08, 0x00]
```

**Example - Parse Device Info Response**:
```dart
final response = GetResponse.fromBuffer(bytes);
if (response.dataObjectId == DO.PAGE_DEV_INFO.value) {
  final devInfo = DevInfoPage.fromBuffer(response.dataObjectData);
  print('Device: ${String.fromCharCodes(devInfo.deviceName)}');
  print('Serial: ${String.fromCharCodes(devInfo.serialNumber)}');
  print('Firmware: ${devInfo.systemFwVersion}');
}
```

---

### DevInfoPage

**Device information** (returned in GET_RESPONSE):

```protobuf
message DevInfoPage {
  repeated uint32 systemFwVersion = 1;   // e.g., [0, 0, 1, 2] = v1.2.0
  bytes deviceName = 2;                  // UTF-8 string
  bytes deviceUid = 3;                   // Unique ID
  bytes systemHwRevision = 4;            // e.g., "B.0"
  bytes serialNumber = 5;                // Serial number
  uint32 manufacturerId = 6;             // 0x094A for Zwift
  uint32 productId = 7;                  // Product identifier
  uint32 protocolVersion = 8;            // Protocol version
  repeated DeviceCapability deviceCapabilities = 9;
}
```

**Parsing**:
```dart
final devInfo = DevInfoPage.fromBuffer(bytes);

// Firmware version
final fw = devInfo.systemFwVersion.join('.');
print('Firmware: $fw');  // "1.2.0"

// Device name
final name = String.fromCharCodes(devInfo.deviceName);
print('Name: $name');  // "Zwift Ride"

// Serial number
final serial = String.fromCharCodes(devInfo.serialNumber);
print('Serial: $serial');
```

---

### BatteryStatus

**Complete battery information**:

```protobuf
message BatteryStatus {
  ChargingState chgState = 1;  // Charging state
  uint32 percLevel = 2;        // Percentage (0-100)
  uint32 timeToEmpty = 3;      // Minutes
  uint32 timeToFull = 4;       // Minutes
}

enum ChargingState {
  CHARGING_IDLE = 0;
  CHARGING_ACTIVE = 1;
  DISCHARGING = 2;
}
```

---

### StatusResponse

**Command acknowledgment**:

```protobuf
message StatusResponse {
  uint32 command = 1;  // Which command (opcode)
  Status status = 2;   // Result status
}

enum Status {
  OK = 0;
  ERROR_GENERAL = 1;
  ERROR_NOT_SUPPORTED = 2;
  // ... more error codes
}
```

**Parsing**:
```dart
final status = StatusResponse.fromBuffer(bytes);
if (status.status != Status.OK.value) {
  print('Command ${status.command} failed: ${status.status}');
}
```

## Varint Encoding

Protocol Buffers uses variable-length encoding for integers:

### Unsigned Integers (uint32)

```
Value | Encoded Bytes
------|---------------
0     | [0x00]
127   | [0x7F]
128   | [0x80, 0x01]
255   | [0xFF, 0x01]
300   | [0xAC, 0x02]
```

### Signed Integers (sint32)

Uses ZigZag encoding to efficiently encode negative numbers:

```
Value | ZigZag | Encoded
------|--------|----------
0     | 0      | [0x00]
-1    | 1      | [0x01]
1     | 2      | [0x02]
-2    | 3      | [0x03]
-100  | 199    | [0xC7, 0x01]
100   | 200    | [0xC8, 0x01]
```

## Working with Protobuf in Dart

### Parsing Messages

```dart
import 'package:bike_control/bluetooth/devices/zwift/protocol/zwift.pb.dart';

void parseButtonMessage(Uint8List bytes) {
  try {
    final status = PlayKeyPadStatus.fromBuffer(bytes);
    
    print('Right Pad: ${status.rightPad}');
    print('Button A: ${status.buttonARight}');
    print('Analog: ${status.analogLR}');
  } catch (e) {
    print('Failed to parse: $e');
  }
}
```

### Creating Messages

```dart
// Create a GET request
final get = Get()
  ..dataObjectId = DO.PAGE_DEV_INFO.value;

final bytes = get.writeToBuffer();
// Now send bytes via BLE
```

### Checking Field Presence

```dart
if (status.hasAnalogLR()) {
  print('Analog value: ${status.analogLR}');
}
```

### Repeated Fields

```dart
// Access repeated fields
for (final paddle in rideStatus.analogPaddles) {
  if (paddle.hasLocation() && paddle.hasAnalogValue()) {
    print('Paddle ${paddle.location}: ${paddle.analogValue}');
  }
}

// Add to repeated field
final paddle = RideAnalogKeyPress()
  ..location = RideAnalogKeyLocation.L0
  ..analogValue = 100;
rideStatus.analogPaddles.add(paddle);
```

## Debugging Protobuf Messages

### Hex Dump

```dart
String hexDump(Uint8List bytes) {
  return bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join(' ');
}

print('Message bytes: ${hexDump(messageBytes)}');
// Output: "08 00 10 64"
```

### Decode to JSON (Debug)

```dart
import 'package:bike_control/bluetooth/devices/zwift/protocol/zwift.pbjson.dart';

final status = PlayKeyPadStatus.fromBuffer(bytes);
print(status.toDebugString());
// Or convert to JSON for inspection
```

## Best Practices

1. **Always use try-catch** when parsing protobuf from untrusted sources
2. **Check field presence** with `has*()` methods before accessing
3. **Validate enum values** - unknown values may appear
4. **Handle unknown fields** gracefully - forward compatibility
5. **Log binary data** in hex for debugging
6. **Use generated code** - don't hand-write protobuf parsing
7. **Test with real devices** - specifications may be incomplete

## Common Issues

### Issue: Invalid Wire Type

**Cause**: Corrupted or wrong message type

**Solution**: Verify you're parsing the correct message type

### Issue: Missing Required Fields

**Cause**: Incomplete message or wrong protobuf definition

**Solution**: Check that all required fields are present

### Issue: Unexpected Enum Value

**Cause**: Device firmware returns unknown enum value

**Solution**: Handle unknown values gracefully

```dart
final buttonState = message.buttonA;
if (buttonState == PlayButtonStatus.ON) {
  // Handle ON
} else if (buttonState == PlayButtonStatus.OFF) {
  // Handle OFF
} else {
  // Unknown state - log and ignore
  print('Unknown button state: $buttonState');
}
```

## Next Steps

- Review [Message Types](MESSAGE_TYPES.md) for complete message catalog
- See [Examples](EXAMPLES.md) for practical parsing examples
- Check [Device Types](DEVICE_TYPES.md) for device-specific messages

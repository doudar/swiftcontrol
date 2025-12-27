# Message Types - Protocol Specifications

## Overview

Zwift devices communicate using various message types, each identified by a type byte or opcode. This document details all message formats used in the Zwift Hardware API.

## Message Type Systems

Zwift uses two different message type systems:

1. **Legacy Message Types** - Used by Zwift Click and Zwift Play
2. **Opcode System** - Used by Zwift Ride and Click v2

---

## Legacy Message Types

### Message Type Constants

```dart
// Message types received from device
const CONTROLLER_NOTIFICATION_MESSAGE_TYPE = 0x07;
const EMPTY_MESSAGE_TYPE = 0x15;  // 21 decimal
const BATTERY_LEVEL_TYPE = 0x19;  // 25 decimal
const UNKNOWN_CLICKV2_TYPE = 0x3C;

// Device-specific notification types
const CLICK_NOTIFICATION_MESSAGE_TYPE = 0x37;  // 55 decimal - Zwift Click
const PLAY_NOTIFICATION_MESSAGE_TYPE = 0x07;   // 7 decimal - Zwift Play
const RIDE_NOTIFICATION_MESSAGE_TYPE = 0x23;   // 35 decimal - (legacy)

// Disconnect message
const DISCONNECT_MESSAGE_TYPE = 0xFE;
```

### Empty/Keepalive Message (0x15)

**Purpose**: Keepalive heartbeat when no events

**Format**:
```
[0x15]  // Type byte only, no payload
```

**When Sent**:
- Periodically when no button presses
- Maintains connection

**Processing**:
```dart
case EMPTY_MESSAGE_TYPE:
  // No action needed
  break;
```

---

### Battery Level Message (0x19)

**Purpose**: Report battery status

**Format** (Legacy devices):
```
[0x19]           // Type byte
[0x??]           // Unknown/reserved
[0x00-0x64]      // Battery level (0-100%)
```

**Example**:
```
[0x19, 0x00, 0x64]  // 100% battery
[0x19, 0x00, 0x32]  // 50% battery
[0x19, 0x00, 0x0A]  // 10% battery (low)
```

**Processing**:
```dart
case BATTERY_LEVEL_TYPE:
  if (batteryLevel != message[1]) {
    batteryLevel = message[1];
    notifyBatteryChanged();
  }
  break;
```

---

### Click Button Notification (0x37)

**Purpose**: Report button presses from Zwift Click

**Format**:
```
[0x37]                        // Type byte
[...ClickKeyPadStatus bytes...] // Protobuf message
```

**ClickKeyPadStatus Protobuf**:
```protobuf
message ClickKeyPadStatus {
  PlayButtonStatus buttonPlus = 1;   // Plus button
  PlayButtonStatus buttonMinus = 2;  // Minus button
}

enum PlayButtonStatus {
  OFF = 0;
  ON = 1;
}
```

**Example Messages**:
```
// Plus button pressed
[0x37, 0x08, 0x01]  // buttonPlus = ON

// Minus button pressed
[0x37, 0x10, 0x01]  // buttonMinus = ON

// Both released
[0x37, 0x08, 0x00, 0x10, 0x00]

// Both pressed (uncommon)
[0x37, 0x08, 0x01, 0x10, 0x01]
```

**Processing**:
```dart
case CLICK_NOTIFICATION_MESSAGE_TYPE:
  final status = ClickKeyPadStatus.fromBuffer(message);
  final buttonsClicked = [
    if (status.buttonPlus == PlayButtonStatus.ON) 
      ZwiftButtons.shiftUpRight,
    if (status.buttonMinus == PlayButtonStatus.ON) 
      ZwiftButtons.shiftUpLeft,
  ];
  handleButtonsClicked(buttonsClicked);
  break;
```

---

### Play Button Notification (0x07)

**Purpose**: Report button presses from Zwift Play

**Format**:
```
[0x07]                        // Type byte
[...PlayKeyPadStatus bytes...]  // Protobuf message
```

**PlayKeyPadStatus Protobuf**:
```protobuf
message PlayKeyPadStatus {
  PlayButtonStatus rightPad = 1;      // Grip sensor (mode switch)
  PlayButtonStatus buttonYUp = 2;     // Y/Up button
  PlayButtonStatus buttonZLeft = 3;   // Z/Left button
  PlayButtonStatus buttonARight = 4;  // A/Right button
  PlayButtonStatus buttonBDown = 5;   // B/Down button
  PlayButtonStatus buttonOn = 6;      // Power button
  PlayButtonStatus buttonShift = 7;   // Side button
  sint32 analogLR = 8;                // Paddle position (-100 to +100)
}
```

**Field Interpretation**:

- `rightPad`: Mode selector
  - `OFF` (0): Navigation mode (D-pad active)
  - `ON` (1): Action mode (A/B/Y/Z active)
- `analogLR`: Analog paddle
  - `-100`: Fully pulled left
  - `0`: Released
  - `+100`: Fully pulled right

**Example Messages**:
```
// A button pressed (action mode)
[0x07, 0x08, 0x01, 0x20, 0x01]
// rightPad=ON, buttonARight=ON

// Navigation up (navigation mode)
[0x07, 0x08, 0x00, 0x10, 0x01]
// rightPad=OFF, buttonYUp=ON

// Paddle pulled (protobuf varint encoding)
[0x07, 0x08, 0x01, 0x40, 0x64]
// rightPad=ON, analogLR=100
```

**Processing**:
```dart
case PLAY_NOTIFICATION_MESSAGE_TYPE:
  final status = PlayKeyPadStatus.fromBuffer(message);
  
  final buttonsClicked = <ControllerButton>[];
  
  if (status.rightPad == PlayButtonStatus.ON) {
    // Action mode
    if (status.buttonYUp == PlayButtonStatus.ON) 
      buttonsClicked.add(ZwiftButtons.y);
    if (status.buttonARight == PlayButtonStatus.ON) 
      buttonsClicked.add(ZwiftButtons.a);
    // ... more action buttons
  } else {
    // Navigation mode
    if (status.buttonYUp == PlayButtonStatus.ON) 
      buttonsClicked.add(ZwiftButtons.navigationUp);
    if (status.buttonZLeft == PlayButtonStatus.ON) 
      buttonsClicked.add(ZwiftButtons.navigationLeft);
    // ... more navigation buttons
  }
  
  // Handle analog paddle
  if (status.analogLR.abs() == 100) {
    buttonsClicked.add(
      status.rightPad == PlayButtonStatus.ON 
        ? ZwiftButtons.paddleRight 
        : ZwiftButtons.paddleLeft
    );
  }
  
  handleButtonsClicked(buttonsClicked);
  break;
```

---

## Modern Opcode System (Zwift Ride/Click v2)

### Opcode Enumeration

```dart
enum Opcode {
  RIDE_ON = 0x52,                    // 'R' - Handshake
  GET = 0x08,                        // Request data object
  GET_RESPONSE = 0x3C,               // Data object response
  SET = 0x10,                        // Set data object
  STATUS_RESPONSE = 0x12,            // Command status
  CONTROLLER_NOTIFICATION = 0x07,    // Button events
  BATTERY_NOTIF = 0x19,              // Battery updates
  LOG_DATA = 0x31,                   // Debug logs
  VENDOR_MESSAGE = 0x32,             // Vendor-specific
  RESET = 0x22,                      // Reset device
  LOG_LEVEL_SET = 0x41,              // Set log level
}
```

### Message Structure

All modern messages follow this format:

```
[Opcode Byte] [Protobuf Payload...]
```

---

### RIDE_ON (0x52) - Handshake

**Purpose**: Establish connection (both directions)

**From App to Device**:
```
[0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e]
// ASCII: "RideOn"
```

**From Device to App**:
```
[0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e]  // "RideOn" echo
// Empty response in unencrypted mode
```

**Processing**:
```dart
case Opcode.RIDE_ON:
  // Connection established successfully
  print("Handshake complete - unencrypted mode");
  break;
```

---

### CONTROLLER_NOTIFICATION (0x07) - Button Events

**Purpose**: Report button presses and paddle positions

**Format**:
```
[0x07]                          // Opcode
[...RideKeyPadStatus bytes...]  // Protobuf message
```

**RideKeyPadStatus Protobuf**:
```protobuf
message RideKeyPadStatus {
  uint32 buttonMap = 1;                         // Bit mask of buttons
  repeated RideAnalogKeyPress analogPaddles = 3; // Analog paddle data
}

message RideAnalogKeyPress {
  RideAnalogKeyLocation location = 1;  // Paddle identifier
  sint32 analogValue = 2;              // -100 to +100
}

enum RideAnalogKeyLocation {
  L0 = 0;  // Left paddle
  L1 = 1;  // Right paddle
  L2 = 2;  // Reserved
  L3 = 3;  // Reserved
}
```

**Button Map Bits**:
```dart
const LEFT_BTN      = 0x00001;  // Bit 0
const UP_BTN        = 0x00002;  // Bit 1
const RIGHT_BTN     = 0x00004;  // Bit 2
const DOWN_BTN      = 0x00008;  // Bit 3

const A_BTN         = 0x00010;  // Bit 4
const B_BTN         = 0x00020;  // Bit 5
const Y_BTN         = 0x00040;  // Bit 6
const Z_BTN         = 0x00080;  // Bit 7

const SHFT_UP_L_BTN = 0x00100;  // Bit 8
const SHFT_DN_L_BTN = 0x00200;  // Bit 9
const POWERUP_L_BTN = 0x00400;  // Bit 10
const ONOFF_L_BTN   = 0x00800;  // Bit 11

const SHFT_UP_R_BTN = 0x01000;  // Bit 12
const SHFT_DN_R_BTN = 0x02000;  // Bit 13
const POWERUP_R_BTN = 0x04000;  // Bit 14
const ONOFF_R_BTN   = 0x08000;  // Bit 15
```

**Example Messages**:
```
// No buttons pressed
[0x07, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F]
// buttonMap = 0xFFFFFFFF (all bits set = all released)

// A button pressed
[0x07, 0x08, 0xEF, 0xFF, 0xFF, 0xFF, 0x0F]
// buttonMap = 0xFFFFFFEF (bit 4 cleared)

// Left paddle pulled
[0x07, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x1A, 0x04, 0x08, 0x00, 0x10, 0x64]
// buttonMap all released, paddle L0 at position 100
```

**Processing**:
```dart
case Opcode.CONTROLLER_NOTIFICATION:
  final status = RideKeyPadStatus.fromBuffer(message);
  
  final buttonsClicked = <ControllerButton>[];
  
  // Check each button bit (inverted logic: 0 = pressed)
  if (status.buttonMap & 0x00001 == 0) 
    buttonsClicked.add(ZwiftButtons.navigationLeft);
  if (status.buttonMap & 0x00010 == 0) 
    buttonsClicked.add(ZwiftButtons.a);
  // ... more buttons
  
  // Process analog paddles
  for (final paddle in status.analogPaddles) {
    if (paddle.analogValue.abs() >= 25) {  // Threshold
      if (paddle.location == RideAnalogKeyLocation.L0) {
        buttonsClicked.add(ZwiftButtons.paddleLeft);
      } else if (paddle.location == RideAnalogKeyLocation.L1) {
        buttonsClicked.add(ZwiftButtons.paddleRight);
      }
    }
  }
  
  handleButtonsClicked(buttonsClicked);
  break;
```

---

### BATTERY_NOTIF (0x19) - Battery Status

**Purpose**: Report battery level changes

**Format**:
```
[0x19]                            // Opcode
[...BatteryNotification bytes...]  // Protobuf message
```

**BatteryNotification Protobuf**:
```protobuf
message BatteryNotification {
  uint32 newPercLevel = 1;  // 0-100
}
```

**Example**:
```
// 100% battery
[0x19, 0x08, 0x64]

// 50% battery
[0x19, 0x08, 0x32]

// 10% battery (low)
[0x19, 0x08, 0x0A]
```

**Processing**:
```dart
case Opcode.BATTERY_NOTIF:
  final notification = BatteryNotification.fromBuffer(message);
  if (batteryLevel != notification.newPercLevel) {
    batteryLevel = notification.newPercLevel;
    notifyBatteryChanged();
  }
  break;
```

---

### GET (0x08) - Request Data Object

**Purpose**: Request device information

**Format (sent by app)**:
```
[0x08]      // Opcode
[...Get...] // Protobuf message
```

**Get Protobuf**:
```protobuf
message Get {
  uint32 dataObjectId = 1;  // Data object identifier
}
```

**Data Object IDs**:
```dart
enum DO {
  PAGE_DEV_INFO = 0x0000,                      // Device information
  PAGE_DATE_TIME = 0x0010,                     // Date/time
  PAGE_CLIENT_SERVER_CONFIGURATION = 0x0010,   // Config
  PAGE_CONTROLLER_INPUT_CONFIG = 0x0880,       // Input config
  BATTERY_STATE = 0x0683,                      // Battery info
}
```

**Example Request**:
```
// Request device info
[0x08, 0x08, 0x00]
// GET opcode, dataObjectId = 0x0000

// Request battery state
[0x08, 0x08, 0x83, 0x0D]
// GET opcode, dataObjectId = 0x0683 (varint encoded)
```

**Sending**:
```dart
final getDeviceInfo = Get(dataObjectId: DO.PAGE_DEV_INFO.value);
final command = Uint8List.fromList([
  Opcode.GET.value,
  ...getDeviceInfo.writeToBuffer(),
]);
await sendCommand(command);
```

---

### GET_RESPONSE (0x3C) - Data Object Response

**Purpose**: Return requested data object

**Format (from device)**:
```
[0x3C]                    // Opcode
[...GetResponse bytes...] // Protobuf message
```

**GetResponse Protobuf**:
```protobuf
message GetResponse {
  uint32 dataObjectId = 1;       // Which object is this
  bytes dataObjectData = 2;      // Serialized data object
}
```

**Example - Device Info Response**:
```
[0x3C, 0x08, 0x00, 0x12, 0x46, 0x0A, 0x44, ...]
// Opcode 0x3C
// dataObjectId = 0x00
// dataObjectData = DevInfoPage bytes
```

**Processing**:
```dart
case Opcode.GET_RESPONSE:
  final response = GetResponse.fromBuffer(message);
  final dataObjectType = DO.valueOf(response.dataObjectId);
  
  switch (dataObjectType) {
    case DO.PAGE_DEV_INFO:
      final devInfo = DevInfoPage.fromBuffer(response.dataObjectData);
      print('Device: ${String.fromCharCodes(devInfo.deviceName)}');
      print('Serial: ${String.fromCharCodes(devInfo.serialNumber)}');
      print('Firmware: ${devInfo.systemFwVersion}');
      break;
      
    case DO.BATTERY_STATE:
      final battery = BatteryStatus.fromBuffer(response.dataObjectData);
      batteryLevel = battery.percLevel;
      break;
  }
  break;
```

---

### STATUS_RESPONSE (0x12) - Command Status

**Purpose**: Acknowledge command execution

**Format**:
```
[0x12]                        // Opcode
[...StatusResponse bytes...]  // Protobuf message
```

**StatusResponse Protobuf**:
```protobuf
message StatusResponse {
  uint32 command = 1;  // Which command
  uint32 status = 2;   // Status code
}

enum Status {
  OK = 0;
  ERROR = 1;
  // ... more status codes
}
```

**Example**:
```
// Success response
[0x12, 0x08, 0x08, 0x10, 0x00]
// command = 0x08 (GET), status = 0 (OK)
```

**Processing**:
```dart
case Opcode.STATUS_RESPONSE:
  final status = StatusResponse.fromBuffer(message);
  if (status.status != Status.OK.value) {
    print('Command ${status.command} failed: ${status.status}');
  }
  break;
```

---

### LOG_DATA (0x31) - Debug Logs

**Purpose**: Device debug output

**Format**:
```
[0x31]                           // Opcode
[...LogDataNotification bytes...] // Protobuf message
```

**LogDataNotification Protobuf**:
```protobuf
message LogDataNotification {
  LogLevel logLevel = 1;
  string message = 2;
}

enum LogLevel {
  LOGLEVEL_TRACE = 0;
  LOGLEVEL_DEBUG = 1;
  LOGLEVEL_INFO = 2;
  LOGLEVEL_WARNING = 3;
  LOGLEVEL_ERROR = 4;
}
```

**Processing**:
```dart
case Opcode.LOG_DATA:
  final logMessage = LogDataNotification.fromBuffer(message);
  print('[${logMessage.logLevel}] ${logMessage.message}');
  break;
```

---

### RESET (0x22) - Reset Device

**Purpose**: Reset device state

**Format (sent by app)**:
```
[0x22]  // Opcode only, no payload
```

**Use Case**:
- Zwift Click v2 workaround
- Clear device state
- Recover from errors

**Sending**:
```dart
final resetCommand = Uint8List.fromList([Opcode.RESET.value]);
await sendCommand(resetCommand);
```

---

## Message Deduplication

Zwift devices may send duplicate messages. Implement deduplication:

```dart
List<ControllerButton>? _lastButtonsClicked;

void handleButtonsClicked(List<ControllerButton> buttonsClicked) {
  // Compare with last message
  if (_lastButtonsClicked == null || 
      !listEquals(_lastButtonsClicked, buttonsClicked)) {
    // Process only if different
    processButtonPress(buttonsClicked);
    _lastButtonsClicked = buttonsClicked;
  }
}
```

## Message Logging

For debugging, log all messages:

```dart
void logMessage(String direction, Uint8List bytes) {
  final timestamp = DateTime.now().toString().split(' ').last;
  final hex = bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join(' ');
  final opcode = Opcode.valueOf(bytes[0]);
  
  print('$timestamp $direction [$opcode]: $hex');
}

// Usage
logMessage('RX', receivedBytes);
logMessage('TX', sentBytes);
```

## Next Steps

- Review [Handshake Process](HANDSHAKE.md) for connection sequence
- See [Protocol Buffers](PROTOBUF.md) for message schemas
- Check [Examples](EXAMPLES.md) for complete implementations

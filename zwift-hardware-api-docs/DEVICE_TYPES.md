# Device Types - Zwift Hardware Specifications

## Overview

Zwift manufactures several Bluetooth controller devices, each with unique characteristics and capabilities. This guide provides detailed specifications for each device type.

## Device Identification

### Manufacturer Data

All Zwift devices use:
- **Manufacturer ID**: `0x094A` (2378 decimal)
- **Device Type Byte**: First byte of manufacturer data identifies the model

### Device Type Codes

```dart
const BC1 = 0x09;                    // Zwift Click (original)
const RC1_LEFT_SIDE = 0x03;          // Zwift Play Left Controller
const RC1_RIGHT_SIDE = 0x02;         // Zwift Play Right Controller
const RIDE_RIGHT_SIDE = 0x07;        // Zwift Ride Right Side
const RIDE_LEFT_SIDE = 0x08;         // Zwift Ride Left Side
const CLICK_V2_RIGHT_SIDE = 0x0A;    // Zwift Click v2 Right (unconfirmed)
const CLICK_V2_LEFT_SIDE = 0x0B;     // Zwift Click v2 Left (unconfirmed)
```

---

## Zwift Click (BC1)

### Device Information

- **Model Code**: BC1
- **Type Byte**: `0x09`
- **Released**: Original Zwift controller
- **Advertising Name**: "Zwift Click"
- **Latest Firmware**: 1.1.0

### Physical Characteristics

- Single controller unit (no left/right distinction)
- Two buttons: Plus (+) and Minus (-)
- Battery powered
- No vibration support

### BLE Service

- **Service UUID**: `00000001-19CA-4651-86E5-FA29DCDD09D1`
- Uses legacy custom service

### Available Buttons

```dart
// Zwift Click buttons
final buttons = [
  shiftUpRight,   // Plus button (+)
  shiftUpLeft,    // Minus button (-)
];
```

### Button Mapping

| Physical Button | Internal Name | Default Action |
|----------------|---------------|----------------|
| Plus (+) | `shiftUpRight` | Shift up / increase |
| Minus (-) | `shiftUpLeft` | Shift down / decrease |

### Message Format

**Handshake Response**:
```
[0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e]  // "RideOn"
[0x01, 0x03]                          // Response signature
[...32 bytes public key...]            // Device public key
```

**Button Notifications** (Type 0x37):
```
Message Type: 0x37 (55 decimal)
Payload: ClickKeyPadStatus protobuf

message ClickKeyPadStatus {
  PlayButtonStatus buttonPlus = 1;   // 0=OFF, 1=ON
  PlayButtonStatus buttonMinus = 2;  // 0=OFF, 1=ON
}
```

### Implementation Example

```dart
class ZwiftClick extends ZwiftDevice {
  @override
  List<ControllerButton> processClickNotification(Uint8List message) {
    final status = ClickKeyPadStatus.fromBuffer(message);
    
    return [
      if (status.buttonPlus == PlayButtonStatus.ON) 
        ZwiftButtons.shiftUpRight,
      if (status.buttonMinus == PlayButtonStatus.ON) 
        ZwiftButtons.shiftUpLeft,
    ];
  }
}
```

---

## Zwift Play (RC1)

### Device Information

- **Model Code**: RC1
- **Type Bytes**: `0x02` (right), `0x03` (left)
- **Released**: 2023
- **Advertising Name**: "Zwift Play"
- **Latest Firmware**: 1.3.1
- **Sides**: Two separate controllers (left and right)

### Physical Characteristics

- Dual controller system
- Left controller: D-pad navigation, paddle, side button
- Right controller: A/B/Y/Z action buttons, paddle, side button
- Supports vibration feedback
- Rechargeable batteries

### BLE Service

- **Service UUID**: `00000001-19CA-4651-86E5-FA29DCDD09D1`
- Uses legacy custom service

### Available Buttons

**Left Controller**:
```dart
navigationUp      // D-pad up
navigationDown    // D-pad down
navigationLeft    // D-pad left
navigationRight   // D-pad right
onOffLeft         // Power button
sideButtonLeft    // Side trigger button
paddleLeft        // Analog paddle (full pull = 100)
```

**Right Controller**:
```dart
a              // Green A button
b              // Pink B button
y              // Blue Y button
z              // Orange Z button
onOffRight     // Power button
sideButtonRight // Side trigger button
paddleRight    // Analog paddle (full pull = 100)
```

### Button Modes

Zwift Play has a mode switch controlled by the right paddle grip:

- **Navigation Mode** (`rightPad == OFF`): D-pad and navigation buttons active
- **Action Mode** (`rightPad == ON`): A/B/Y/Z action buttons active

### Message Format

**Handshake**:
```
Start Command: "RideOn" + [0x01, 0x04]
```

**Button Notifications** (Type 0x07):
```
Message Type: 0x07 (PLAY_NOTIFICATION_MESSAGE_TYPE)
Payload: PlayKeyPadStatus protobuf

message PlayKeyPadStatus {
  PlayButtonStatus rightPad = 1;      // Grip sensor (mode switch)
  PlayButtonStatus buttonYUp = 2;
  PlayButtonStatus buttonZLeft = 3;
  PlayButtonStatus buttonARight = 4;
  PlayButtonStatus buttonBDown = 5;
  PlayButtonStatus buttonOn = 6;      // Power button
  PlayButtonStatus buttonShift = 7;   // Side button
  sint32 analogLR = 8;                // Paddle: -100 to +100
}
```

### Implementation Example

```dart
class ZwiftPlay extends ZwiftDevice {
  @override
  List<ControllerButton> processClickNotification(Uint8List message) {
    final status = PlayKeyPadStatus.fromBuffer(message);
    
    return [
      if (status.rightPad == PlayButtonStatus.ON) ...[
        // Action mode buttons
        if (status.buttonYUp == PlayButtonStatus.ON) ZwiftButtons.y,
        if (status.buttonZLeft == PlayButtonStatus.ON) ZwiftButtons.z,
        if (status.buttonARight == PlayButtonStatus.ON) ZwiftButtons.a,
        if (status.buttonBDown == PlayButtonStatus.ON) ZwiftButtons.b,
        if (status.buttonOn == PlayButtonStatus.ON) ZwiftButtons.onOffRight,
        if (status.buttonShift == PlayButtonStatus.ON) ZwiftButtons.sideButtonRight,
        if (status.analogLR.abs() == 100) ZwiftButtons.paddleRight,
      ],
      if (status.rightPad == PlayButtonStatus.OFF) ...[
        // Navigation mode buttons
        if (status.buttonYUp == PlayButtonStatus.ON) ZwiftButtons.navigationUp,
        if (status.buttonZLeft == PlayButtonStatus.ON) ZwiftButtons.navigationLeft,
        if (status.buttonARight == PlayButtonStatus.ON) ZwiftButtons.navigationRight,
        if (status.buttonBDown == PlayButtonStatus.ON) ZwiftButtons.navigationDown,
        if (status.buttonOn == PlayButtonStatus.ON) ZwiftButtons.onOffLeft,
        if (status.buttonShift == PlayButtonStatus.ON) ZwiftButtons.sideButtonLeft,
        if (status.analogLR.abs() == 100) ZwiftButtons.paddleLeft,
      ],
    ];
  }
}
```

### Vibration Support

Zwift Play supports haptic feedback:

```dart
final vibrateCommand = Uint8List.fromList([
  0x12, 0x12, 0x08, 0x0A, 0x06, 0x08, 
  0x02, 0x10, 0x00, 0x18, 0x20
]);

await sendCommand(vibrateCommand);
```

---

## Zwift Ride

### Device Information

- **Model Code**: Zwift Ride
- **Type Bytes**: `0x07` (right), `0x08` (left)
- **Released**: 2024
- **Advertising Name**: "Zwift Ride" (left controller advertises)
- **Latest Firmware**: 1.2.0
- **Sides**: Integrated handlebar system with two sides

### Physical Characteristics

- Integrated smart bike handlebar
- Full feature set: navigation, action buttons, and shifters
- Left: D-pad, shift down, power-up button
- Right: A/B/Y/Z buttons, shift up, power-up button
- Analog paddles on both sides
- Supports vibration feedback
- Advanced protobuf protocol

### BLE Service

- **Service UUID**: `0000FC82-0000-1000-8000-00805F9B34FB`
- **Short UUID**: `FC82`
- Uses modern Ride service (different from legacy devices)

### Available Buttons

**Digital Buttons**:
```dart
// Navigation
navigationLeft, navigationRight, navigationUp, navigationDown

// Action buttons
a, b, y, z

// Shifters
shiftUpLeft, shiftDownLeft     // Left shifter (+ and -)
shiftUpRight, shiftDownRight   // Right shifter (+ and -)

// Power-up buttons
powerUpLeft, powerUpRight

// Power buttons
onOffLeft, onOffRight

// Analog paddles
paddleLeft, paddleRight
```

### Button Mapping (Button Mask)

The Zwift Ride uses a button mask in the protobuf message:

```dart
enum RideButtonMask {
  LEFT_BTN(0x00001),
  UP_BTN(0x00002),
  RIGHT_BTN(0x00004),
  DOWN_BTN(0x00008),
  
  A_BTN(0x00010),
  B_BTN(0x00020),
  Y_BTN(0x00040),
  Z_BTN(0x00080),
  
  SHFT_UP_L_BTN(0x00100),
  SHFT_DN_L_BTN(0x00200),
  POWERUP_L_BTN(0x00400),
  ONOFF_L_BTN(0x00800),
  
  SHFT_UP_R_BTN(0x01000),
  SHFT_DN_R_BTN(0x02000),
  POWERUP_R_BTN(0x04000),
  ONOFF_R_BTN(0x08000);
}
```

### Protocol - Opcode System

Zwift Ride uses an advanced opcode-based protocol:

```dart
enum Opcode {
  RIDE_ON = 0x52,                    // 'R' - Handshake
  GET = 0x08,                        // Request data object
  GET_RESPONSE = 0x3C,               // Data object response
  STATUS_RESPONSE = 0x12,            // Command status
  CONTROLLER_NOTIFICATION = 0x07,    // Button events
  BATTERY_NOTIF = 0x19,              // Battery updates
  LOG_DATA = 0x31,                   // Debug logs
  VENDOR_MESSAGE = 0x32,             // Vendor-specific
  RESET = 0x22,                      // Reset device
  LOG_LEVEL_SET = 0x41,              // Set log level
}
```

### Message Format

**Button Notifications** (Opcode 0x07):
```
[0x07]                        // CONTROLLER_NOTIFICATION opcode
[...RideKeyPadStatus bytes...]  // Protobuf message

message RideKeyPadStatus {
  uint32 buttonMap = 1;              // Bit mask of pressed buttons
  repeated RideAnalogKeyPress analogPaddles = 3;
}

message RideAnalogKeyPress {
  RideAnalogKeyLocation location = 1;  // 0=L0 (left), 1=L1 (right)
  sint32 analogValue = 2;              // -100 to +100
}
```

**Battery Notifications** (Opcode 0x19):
```
[0x19]  // BATTERY_NOTIF opcode
[...BatteryNotification bytes...]

message BatteryNotification {
  uint32 newPercLevel = 1;  // 0-100
}
```

### Data Objects

Zwift Ride supports querying various data objects:

```dart
enum DO {
  PAGE_DEV_INFO = 0x0000,                      // Device information
  PAGE_DATE_TIME = 0x0010,                     // Date/time settings
  PAGE_CLIENT_SERVER_CONFIGURATION = 0x0010,   // Client/server config
  PAGE_CONTROLLER_INPUT_CONFIG = 0x0880,       // Input configuration
  BATTERY_STATE = 0x0683,                      // Battery status
}
```

### Analog Paddle Threshold

The paddles use analog values from -100 to +100. A threshold prevents accidental triggers:

```dart
static const int analogPaddleThreshold = 25;

// Only register paddle press if |analogValue| >= 25
if (paddle.analogValue.abs() >= analogPaddleThreshold) {
  // Process paddle press
}
```

### Implementation Example

```dart
class ZwiftRide extends ZwiftDevice {
  @override
  Future<void> processData(Uint8List bytes) async {
    Opcode? opcode = Opcode.valueOf(bytes[0]);
    Uint8List message = bytes.sublist(1);
    
    switch (opcode) {
      case Opcode.CONTROLLER_NOTIFICATION:
        final buttonsClicked = processClickNotification(message);
        handleButtonsClicked(buttonsClicked);
        break;
        
      case Opcode.BATTERY_NOTIF:
        final notification = BatteryNotification.fromBuffer(message);
        batteryLevel = notification.newPercLevel;
        break;
    }
  }
  
  @override
  List<ControllerButton> processClickNotification(Uint8List message) {
    final status = RideKeyPadStatus.fromBuffer(message);
    
    // Process digital buttons via button map
    final buttonsClicked = [
      if (status.buttonMap & RideButtonMask.LEFT_BTN.mask == 1)
        ZwiftButtons.navigationLeft,
      if (status.buttonMap & RideButtonMask.A_BTN.mask == 1)
        ZwiftButtons.a,
      // ... more buttons
    ];
    
    // Process analog paddles
    for (final paddle in status.analogPaddles) {
      if (paddle.analogValue.abs() >= analogPaddleThreshold) {
        final button = switch (paddle.location.value) {
          0 => ZwiftButtons.paddleLeft,
          1 => ZwiftButtons.paddleRight,
          _ => null,
        };
        if (button != null) buttonsClicked.add(button);
      }
    }
    
    return buttonsClicked;
  }
}
```

---

## Zwift Click v2

### Device Information

- **Type Bytes**: `0x0A` (right), `0x0B` (left)
- **Released**: 2024
- **Advertising Name**: "Zwift Click"
- **Latest Firmware**: 1.1.0
- **Status**: Beta support

### Physical Characteristics

- Enhanced version of original Click
- Similar to Zwift Ride in protocol
- Uses modern protobuf protocol
- No vibration support
- Requires periodic reset workaround

### BLE Service

- **Service UUID**: `0000FC82-0000-1000-8000-00805F9B34FB`
- Uses Ride service UUID (same as Zwift Ride)

### Available Buttons

Limited button set compared to Ride:

```dart
navigationLeft, navigationRight, navigationUp, navigationDown
a, b, y, z
shiftUpLeft, shiftUpRight
```

### Special Behavior

Click v2 has a known issue where it stops sending events after initial connection. A workaround is required:

**Issue Detection**:
```
Device sends: [0xFF, 0x05, 0x00, 0xEA, 0x05]  // Variant 1
           or [0xFF, 0x05, 0x00, 0xFA, 0x05]  // Variant 2
```

**Workaround**:
```dart
// Send reset command after detecting stop message
await sendCommand(Opcode.RESET, null);

// Or send special initialization sequence during handshake
await sendCommandBuffer(Uint8List.fromList([0xFF, 0x04, 0x00]));
```

### Implementation

Click v2 extends ZwiftRide class but with limited buttons:

```dart
class ZwiftClickV2 extends ZwiftRide {
  bool _noLongerSendsEvents = false;
  
  @override
  Future<void> setupHandshake() async {
    await super.setupHandshake();
    // Special initialization for Click v2
    await sendCommandBuffer(Uint8List.fromList([0xFF, 0x04, 0x00]));
  }
  
  @override
  Future<void> processData(Uint8List bytes) {
    // Detect stop message
    if (bytes.startsWith([0xFF, 0x05, 0x00, 0xEA, 0x05]) ||
        bytes.startsWith([0xFF, 0x05, 0x00, 0xFA, 0x05])) {
      _noLongerSendsEvents = true;
      // Notify user to reset device
    }
    return super.processData(bytes);
  }
}
```

---

## Device Comparison Matrix

| Feature | Click | Click v2 | Play | Ride |
|---------|-------|----------|------|------|
| **Protocol** | Legacy | Modern | Legacy | Modern |
| **Service UUID** | `...09D1` | `FC82` | `...09D1` | `FC82` |
| **Button Count** | 2 | 10 | 14 | 20+ |
| **Vibration** | ❌ | ❌ | ✅ | ✅ |
| **Analog Paddles** | ❌ | ❌ | ✅ | ✅ |
| **Firmware Version** | 1.1.0 | 1.1.0 | 1.3.1 | 1.2.0 |
| **Protobuf Usage** | Minimal | Full | Minimal | Full |
| **Data Objects** | ❌ | ✅ | ❌ | ✅ |
| **Left/Right Split** | ❌ | ✅ | ✅ | ✅ |
| **Status** | Stable | Beta | Stable | Stable |

---

## Pairing Considerations

### Zwift Play & Click

- Single controller advertises (doesn't matter which side)
- Both left and right recognized after connection
- No special pairing sequence needed

### Zwift Ride

- **Only connect to LEFT controller** (type 0x08)
- Right controller paired automatically
- Connecting to right controller causes issues
- Check device pairing status via vendor data objects

### Click v2

- **Only connect to LEFT controller** (type 0x0B)
- Similar to Ride pairing behavior
- Requires special initialization sequence

## Next Steps

- Review [Message Types](MESSAGE_TYPES.md) for detailed message specifications
- See [Protocol Buffers](PROTOBUF.md) for protobuf schema details
- Check [Button Mapping](BUTTON_MAPPING.md) for action configuration
- Refer to [Examples](EXAMPLES.md) for device-specific implementations

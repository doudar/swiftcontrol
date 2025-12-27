# Zwift Ride Protocol

## Overview

The Zwift Ride protocol uses Protocol Buffers (protobuf) to encode controller button events and device status. This document describes the message structure and how to parse button presses.

## Protobuf Messages

### Prerequisites

You'll need a protobuf library for your platform. The message definitions are available in the BikeControl repository:

- `lib/bluetooth/devices/zwift/protocol/zp.proto` (not in repo, but generated files are)
- `lib/bluetooth/devices/zwift/protocol/zp.pb.dart` (generated Dart code)

## Message Structure

All Zwift Ride messages have an opcode prefix followed by a protobuf message:

```
[Opcode (1 byte)] [Protobuf message (N bytes)]
```

## Opcodes

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

## Handshake

### Initial Handshake

Zwift sends "RideOn" (no opcode, just raw bytes):

```
Received: 52 69 64 65 4f 6e
ASCII: "RideOn"
```

### Handshake Response

Your trainer should respond with "RideOn" + signature on the Sync TX characteristic:

```
Send: 52 69 64 65 4f 6e 01 03
      "RideOn"         Signature
```

The signature `01 03` identifies this as a Zwift Ride device (similar to other Zwift controllers).

After the handshake, follow with a keep-alive message after 5 seconds (see Keep-Alive section below).

## Button Notifications

### Controller Notification Message

When a button is pressed or released on the Zwift Ride controller:

```
Opcode: 0x07 (CONTROLLER_NOTIFICATION)
Protobuf: RideKeyPadStatus
```

### RideKeyPadStatus Protobuf

```protobuf
message RideKeyPadStatus {
  uint32 buttonMap = 1;                              // Button state mask
  repeated RideAnalogKeyPress analogPaddles = 3;     // Paddle positions
}

message RideAnalogKeyPress {
  RideAnalogKeyLocation location = 1;  // 0=L0 (left), 1=L1 (right)
  sint32 analogValue = 2;              // -100 to +100
}
```

### Button Map

The `buttonMap` is a 32-bit integer where each bit represents a button. When a button is **pressed**, its bit is **cleared (0)**. When **released**, its bit is **set (1)**.

**Important**: This is inverted logic! Pressed = 0, Released = 1.

### Button Bit Masks

```dart
enum RideButtonMask {
  LEFT_BTN(0x00001),       // Navigation left
  UP_BTN(0x00002),         // Navigation up
  RIGHT_BTN(0x00004),      // Navigation right
  DOWN_BTN(0x00008),       // Navigation down
  
  A_BTN(0x00010),          // A button (green)
  B_BTN(0x00020),          // B button (pink)
  Y_BTN(0x00040),          // Y button (blue)
  Z_BTN(0x00080),          // Z button (orange)
  
  SHFT_UP_L_BTN(0x00100),  // Left shifter up
  SHFT_DN_L_BTN(0x00200),  // Left shifter down
  POWERUP_L_BTN(0x00400),  // Left power-up button
  ONOFF_L_BTN(0x00800),    // Left power button
  
  SHFT_UP_R_BTN(0x01000),  // Right shifter up
  SHFT_DN_R_BTN(0x02000),  // Right shifter down
  POWERUP_R_BTN(0x04000),  // Right power-up button
  ONOFF_R_BTN(0x08000);    // Right power button
}
```

### Parsing Button Map

To detect if a button is pressed:

```dart
// Check if button bit is CLEAR (0) = pressed
bool isPressed = (buttonMap & buttonMask) == 0;
```

However, the actual implementation in BikeControl uses a different approach - checking if the bit equals the button status value:

```dart
// BikeControl approach
if (status.buttonMap & RideButtonMask.A_BTN.mask == PlayButtonStatus.ON.value) {
  // A button is pressed
}
```

Where `PlayButtonStatus.ON.value` is the indicator of a pressed state.

### Example - Detect Shift Up Right

```dart
final status = RideKeyPadStatus.fromBuffer(message);

// Method 1: Check if bit is clear
if ((status.buttonMap & RideButtonMask.SHFT_UP_R_BTN.mask) == 0) {
  print('Right shift up pressed!');
}

// Method 2: Using BikeControl pattern
if (status.buttonMap & RideButtonMask.SHFT_UP_R_BTN.mask == PlayButtonStatus.ON.value) {
  print('Right shift up pressed!');
}
```

### Example - Parse All Buttons

```dart
List<String> getPressed Buttons(RideKeyPadStatus status) {
  final pressed = <String>[];
  
  if (status.buttonMap & RideButtonMask.LEFT_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('navigationLeft');
  if (status.buttonMap & RideButtonMask.UP_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('navigationUp');
  if (status.buttonMap & RideButtonMask.RIGHT_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('navigationRight');
  if (status.buttonMap & RideButtonMask.DOWN_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('navigationDown');
    
  if (status.buttonMap & RideButtonMask.A_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('a');
  if (status.buttonMap & RideButtonMask.B_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('b');
  if (status.buttonMap & RideButtonMask.Y_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('y');
  if (status.buttonMap & RideButtonMask.Z_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('z');
    
  if (status.buttonMap & RideButtonMask.SHFT_UP_L_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('shiftUpLeft');
  if (status.buttonMap & RideButtonMask.SHFT_DN_L_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('shiftDownLeft');
  if (status.buttonMap & RideButtonMask.SHFT_UP_R_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('shiftUpRight');
  if (status.buttonMap & RideButtonMask.SHFT_DN_R_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('shiftDownRight');
    
  if (status.buttonMap & RideButtonMask.POWERUP_L_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('powerUpLeft');
  if (status.buttonMap & RideButtonMask.POWERUP_R_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('powerUpRight');
    
  if (status.buttonMap & RideButtonMask.ONOFF_L_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('onOffLeft');
  if (status.buttonMap & RideButtonMask.ONOFF_R_BTN.mask == PlayButtonStatus.ON.value) 
    pressed.add('onOffRight');
  
  return pressed;
}
```

## Analog Paddles

The Zwift Ride has two analog paddles (left and right) that can be partially pulled.

### Paddle Locations

```dart
enum RideAnalogKeyLocation {
  L0 = 0,  // Left paddle
  L1 = 1,  // Right paddle
  L2 = 2,  // Unused
  L3 = 3,  // Unused
}
```

### Paddle Values

- Range: -100 to +100
- Positive values: Paddle pulled
- Negative values: Also valid (implementation-specific)
- Zero: Paddle released

### Analog Threshold

To prevent accidental triggers from light touches:

```dart
static const int analogPaddleThreshold = 25;

// Only register paddle press if absolute value >= 25
if (paddle.analogValue.abs() >= analogPaddleThreshold) {
  // Process paddle press
}
```

### Parsing Paddles

```dart
for (final paddle in status.analogPaddles) {
  if (paddle.hasLocation() && paddle.hasAnalogValue()) {
    if (paddle.analogValue.abs() >= analogPaddleThreshold) {
      switch (paddle.location.value) {
        case 0:  // L0 = left paddle
          print('Left paddle: ${paddle.analogValue}');
          break;
        case 1:  // L1 = right paddle
          print('Right paddle: ${paddle.analogValue}');
          break;
      }
    }
  }
}
```

## Sending Button Events

When you want to emulate a button press (for example, when user shifts gears on your trainer):

### Create RideKeyPadStatus

```dart
// Example: Send shift up right button press
final status = RideKeyPadStatus()
  ..buttonMap = (~RideButtonMask.SHFT_UP_R_BTN.mask) & 0xFFFFFFFF
  ..analogPaddles.clear();

final bytes = status.writeToBuffer();
```

Note: `~` (bitwise NOT) inverts the mask because pressed = 0.

### Send Notification

```dart
// Send button press
final notification = [
  Opcode.CONTROLLER_NOTIFICATION.value,  // 0x07
  ...bytes,
];

sendNotification(
  socket,
  '00000002-19CA-4651-86E5-FA29DCDD09D1',  // Async TX characteristic
  notification,
);
```

### Send Button Release

```dart
// Release all buttons (all bits set to 1)
final releaseStatus = RideKeyPadStatus()
  ..buttonMap = 0xFFFFFFFF
  ..analogPaddles.clear();

final bytes = releaseStatus.writeToBuffer();

final notification = [
  Opcode.CONTROLLER_NOTIFICATION.value,
  ...bytes,
];

sendNotification(socket, '00000002-19CA-4651-86E5-FA29DCDD09D1', notification);
```

## Battery Notifications

Send periodic battery updates:

### BatteryNotification Protobuf

```protobuf
message BatteryNotification {
  uint32 newPercLevel = 1;  // 0-100
}
```

### Send Battery Update

```dart
final batteryNotification = BatteryNotification()
  ..newPercLevel = 85;  // 85%

final bytes = batteryNotification.writeToBuffer();

final notification = [
  Opcode.BATTERY_NOTIF.value,  // 0x19
  ...bytes,
];

sendNotification(
  socket,
  '00000002-19CA-4651-86E5-FA29DCDD09D1',  // Async TX
  notification,
);
```

## Keep-Alive Messages

After the handshake, send a keep-alive message every 5 seconds:

```dart
Future<void> sendKeepAlive() async {
  await Future.delayed(const Duration(seconds: 5));
  
  if (socket != null) {
    final keepAliveData = hexToBytes(
      'B70100002041201C00180004001B4F00B701000020798EC5BDEFCBE4563418269E4926FBE1'
    );
    
    sendNotification(
      socket,
      '00000004-19CA-4651-86E5-FA29DCDD09D1',  // Sync TX
      keepAliveData,
    );
    
    // Schedule next keep-alive
    sendKeepAlive();
  }
}
```

This keep-alive appears to be a vendor-specific message that maintains the connection.

## Processing Received Data

When you receive data on Sync RX characteristic:

```dart
Future<void> handleWriteRequest(String characteristicUuid, Uint8List data) {
  if (characteristicUuid == '00000003-19CA-4651-86E5-FA29DCDD09D1') {
    // Sync RX - commands from Zwift
    
    // Check for handshake
    if (data.contentEquals([0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e])) {
      // "RideOn" handshake
      print('Handshake received!');
      
      // Respond with "RideOn" + signature
      return Uint8List.fromList([
        0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e,  // "RideOn"
        0x01, 0x03,                           // Signature
      ]);
    }
    
    // Check for opcodes
    final opcode = Opcode.valueOf(data[0]);
    final message = data.sublist(1);
    
    switch (opcode) {
      case Opcode.GET:
        // Handle GET request
        break;
      case Opcode.RESET:
        // Handle RESET
        break;
      // ... other opcodes
    }
  }
  
  return null;
}
```

## Complete Button Press Example

```dart
// When user shifts up on your trainer:

1. Create button status
   final status = RideKeyPadStatus()
     ..buttonMap = (~RideButtonMask.SHFT_UP_R_BTN.mask) & 0xFFFFFFFF
     ..analogPaddles.clear();

2. Serialize to protobuf
   final bytes = status.writeToBuffer();

3. Send as notification
   sendNotification(
     socket,
     '00000002-19CA-4651-86E5-FA29DCDD09D1',
     [0x07, ...bytes],
   );

4. Wait 100-500ms (button hold duration)

5. Send button release
   final release = RideKeyPadStatus()
     ..buttonMap = 0xFFFFFFFF
     ..analogPaddles.clear();
   
   sendNotification(
     socket,
     '00000002-19CA-4651-86E5-FA29DCDD09D1',
     [0x07, ...release.writeToBuffer()],
   );
```

## Implementation Checklist

- [ ] Install protobuf library for your platform
- [ ] Copy or generate protobuf definitions from BikeControl
- [ ] Handle "RideOn" handshake
- [ ] Send "RideOn" + signature response
- [ ] Parse CONTROLLER_NOTIFICATION messages
- [ ] Decode RideKeyPadStatus button map
- [ ] Check button masks with inverted logic
- [ ] Parse analog paddle values with threshold
- [ ] Send button press notifications
- [ ] Send button release notifications
- [ ] Send battery notifications
- [ ] Implement keep-alive messages

## Reference Code

See BikeControl implementation:
- File: `lib/bluetooth/devices/zwift/zwift_ride.dart`
- File: `lib/bluetooth/devices/zwift/ftms_mdns_emulator.dart` (lines 336-386)
- Protobuf: `lib/bluetooth/devices/zwift/protocol/zwift.pb.dart`

## Next Steps

After understanding the Zwift Ride protocol, proceed to **[GEAR_AND_INCLINE.md](GEAR_AND_INCLINE.md)** to learn how to map button presses to gear changes and incline adjustments on your FTMS trainer.

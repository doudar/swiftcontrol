# Zwift Hardware API - Overview

## Architecture

The Zwift Hardware API is built on Bluetooth Low Energy (BLE) and uses a custom protocol for communication between Zwift controllers and client applications.

### High-Level Architecture

```
┌─────────────────┐         BLE          ┌──────────────────┐
│  Zwift Device   │ ←──────────────────→ │  Your App        │
│  (Controller)   │   GATT Protocol      │  (Central)       │
└─────────────────┘                      └──────────────────┘
        │                                         │
        │  Notifications                          │  Write Commands
        │  (Button presses,                       │  (Vibration,
        │   Battery level)                        │   Handshake)
        ▼                                         ▼
```

### Protocol Stack

```
┌───────────────────────────────────────┐
│     Application Layer                 │
│  (Button Events, Battery Status)      │
├───────────────────────────────────────┤
│     Message Layer                     │
│  (Protocol Buffers, Message Types)    │
├───────────────────────────────────────┤
│     Transport Layer                   │
│  (BLE Characteristics, Notifications) │
├───────────────────────────────────────┤
│     Physical Layer                    │
│  (Bluetooth Low Energy)               │
└───────────────────────────────────────┘
```

## Core Components

### 1. BLE Services

Zwift devices expose two main custom services:

**Legacy Service** (Zwift Click, Play):
- UUID: `00000001-19CA-4651-86E5-FA29DCDD09D1`

**Ride Service** (Zwift Ride):
- UUID: `0000FC82-0000-1000-8000-00805F9B34FB`
- Short UUID: `FC82`

### 2. Characteristics

Each service exposes three primary characteristics:

| Characteristic | UUID | Properties | Purpose |
|---------------|------|------------|---------|
| Async | `00000002-19CA-4651-86E5-FA29DCDD09D1` | Notify | Asynchronous notifications (button presses) |
| Sync RX | `00000003-19CA-4651-86E5-FA29DCDD09D1` | Write Without Response | Commands from app to device |
| Sync TX | `00000004-19CA-4651-86E5-FA29DCDD09D1` | Indicate | Synchronous responses from device |

### 3. Device Identification

Zwift devices are identified by:

**Manufacturer Data**:
- Manufacturer ID: `0x094A` (2378 decimal) - Zwift, Inc.
- Device Type Byte: Identifies specific device model

**Device Type Codes**:
```dart
const BC1 = 0x09;                    // Zwift Click
const RC1_LEFT_SIDE = 0x03;          // Zwift Play Left
const RC1_RIGHT_SIDE = 0x02;         // Zwift Play Right
const RIDE_RIGHT_SIDE = 0x07;        // Zwift Ride Right
const RIDE_LEFT_SIDE = 0x08;         // Zwift Ride Left
const CLICK_V2_RIGHT_SIDE = 0x0A;    // Zwift Click v2 Right
const CLICK_V2_LEFT_SIDE = 0x0B;     // Zwift Click v2 Left
```

## Communication Flow

### 1. Device Discovery

```dart
// Scan for BLE devices with Zwift services
scanForDevices(
  withServices: [
    "00000001-19CA-4651-86E5-FA29DCDD09D1",
    "0000FC82-0000-1000-8000-00805F9B34FB"
  ],
  withManufacturerData: {
    0x094A: null  // Zwift manufacturer ID
  }
)
```

### 2. Connection Sequence

```
1. CONNECT to device
2. DISCOVER services
3. DISCOVER characteristics
4. SUBSCRIBE to Async characteristic (notifications)
5. SUBSCRIBE to Sync TX characteristic (indications)
6. SEND handshake (RideOn message)
7. RECEIVE handshake response
8. BEGIN message processing
```

### 3. Message Exchange

**Outbound (App → Device)**:
- Handshake initialization
- Vibration commands
- Configuration requests

**Inbound (Device → App)**:
- Handshake response
- Battery level updates
- Button press notifications
- Status updates

## Message Format

### Basic Message Structure

All messages follow this pattern:

```
[Message Type Byte] [Payload Bytes...]
```

### Common Message Types

| Type | Value | Description | Used By |
|------|-------|-------------|---------|
| EMPTY | 0x15 (21) | Keep-alive message | All |
| BATTERY_LEVEL | 0x19 (25) | Battery status | All |
| PLAY_NOTIFICATION | 0x07 (7) | Button events | Zwift Play |
| CLICK_NOTIFICATION | 0x37 (55) | Button events | Zwift Click |
| RIDE_NOTIFICATION | 0x23 (35) | Button events | Zwift Ride (legacy) |
| CONTROLLER_NOTIFICATION | 0x07 (7) | Button events | Zwift Ride (protobuf) |

## Protocol Versions

### Legacy Protocol (Click, Play)

- Uses simple message types
- Button states encoded as varints
- Minimal protobuf usage

### Modern Protocol (Ride, Click v2)

- Extensive use of Protocol Buffers
- Opcode-based message routing
- Rich device information exchange
- Support for advanced features

### Opcode System (Zwift Ride)

```dart
enum Opcode {
  RIDE_ON = 0x52,              // Handshake
  GET = 0x08,                  // Request data object
  GET_RESPONSE = 0x3C,         // Data object response
  STATUS_RESPONSE = 0x12,      // Command status
  CONTROLLER_NOTIFICATION = 0x07,  // Button events
  BATTERY_NOTIF = 0x19,        // Battery updates
  LOG_DATA = 0x31,             // Debug logs
  VENDOR_MESSAGE = 0x32,       // Vendor-specific
  RESET = 0x22                 // Reset device
}
```

## Design Patterns

### 1. Abstract Base Classes

The implementation uses inheritance for code reuse:

```
BaseDevice (abstract)
  └── BluetoothDevice (abstract)
        └── ZwiftDevice (abstract)
              ├── ZwiftClick
              ├── ZwiftClickV2
              ├── ZwiftPlay
              └── ZwiftRide
```

### 2. Message Processing

```dart
abstract class ZwiftDevice {
  Future<void> processCharacteristic(String uuid, Uint8List bytes) {
    // Route to appropriate handler
    if (bytes.startsWith(HANDSHAKE)) {
      processHandshake(bytes);
    } else {
      processData(bytes);
    }
  }
  
  Future<void> processData(Uint8List bytes) {
    int messageType = bytes[0];
    Uint8List payload = bytes.sublist(1);
    
    switch (messageType) {
      case BATTERY_LEVEL_TYPE:
        updateBattery(payload);
        break;
      case BUTTON_NOTIFICATION_TYPE:
        processButtonPress(payload);
        break;
    }
  }
}
```

### 3. Event Streaming

Button presses are delivered through a stream:

```dart
Stream<List<ControllerButton>> get buttonEvents;

// Example usage
buttonEvents.listen((buttons) {
  for (var button in buttons) {
    handleAction(button.action);
  }
});
```

## Data Structures

### Button Representation

```dart
class ControllerButton {
  final String name;          // Internal identifier
  final InGameAction? action; // Associated action
  final IconData? icon;       // Visual representation
  final Color? color;         // Button color
}
```

### Protocol Buffer Messages

Key protobuf message types:

- `PlayKeyPadStatus` - Zwift Play button state
- `ClickKeyPadStatus` - Zwift Click button state
- `RideKeyPadStatus` - Zwift Ride button state
- `BatteryNotification` - Battery level update
- `GetResponse` - Data object response
- `DevInfoPage` - Device information

## Error Handling

### Connection Errors

```dart
try {
  await connect();
} catch (e) {
  if (e is ServiceNotFoundException) {
    // Suggest firmware update
  } else if (e is CharacteristicNotFoundException) {
    // Handle missing characteristic
  }
}
```

### Message Parsing Errors

```dart
try {
  final message = ProtobufMessage.fromBuffer(bytes);
} catch (e) {
  // Log malformed message
  // Continue processing other messages
}
```

## Performance Considerations

### 1. Message Deduplication

Zwift devices may send duplicate messages. Implement deduplication:

```dart
List<ControllerButton>? _lastButtonsClicked;

if (_lastButtonsClicked != buttonsClicked) {
  handleButtonPress(buttonsClicked);
  _lastButtonsClicked = buttonsClicked;
}
```

### 2. Characteristic Subscriptions

- Subscribe to notifications early in connection process
- Handle subscription failures gracefully
- Resubscribe on reconnection

### 3. Battery Optimization

- Process only changed battery levels
- Batch UI updates
- Use appropriate BLE connection intervals

## Platform Considerations

### Android
- Requires `BLUETOOTH` and `BLUETOOTH_ADMIN` permissions
- Location permission needed for BLE scanning (Android 6-11)
- `BLUETOOTH_SCAN` and `BLUETOOTH_CONNECT` (Android 12+)

### iOS
- Requires `NSBluetoothAlwaysUsageDescription` in Info.plist
- Background modes for continued connectivity
- Limited BLE filtering compared to Android

### Web
- Web Bluetooth API has limited support
- Requires user gesture to initiate scanning
- Manufacturer data may not be available

### Windows
- WinRT Bluetooth APIs
- May require pairing for some operations

### macOS
- Similar to iOS requirements
- Entitlements for Bluetooth usage

## Security Model

### Encryption (Advanced)

Some Zwift devices support encrypted communication:

1. Public key exchange during handshake
2. Symmetric key derivation
3. Encrypted message payloads

**Note**: BikeControl primarily uses unencrypted mode for simplicity.

### Privacy

- Devices advertise with consistent names
- No personal data transmitted
- Button presses are context-free

## Best Practices

1. **Always check firmware version** and notify users of updates
2. **Implement robust error handling** for BLE operations
3. **Deduplicate messages** to avoid double-triggers
4. **Test with real hardware** - emulators may not capture all behaviors
5. **Support reconnection** - BLE connections are unstable
6. **Provide user feedback** for connection status
7. **Log message exchanges** in debug mode for troubleshooting

## Next Steps

- Proceed to [Protocol Basics](PROTOCOL_BASICS.md) for detailed BLE implementation
- Review [Device Types](DEVICE_TYPES.md) for device-specific details
- See [Code Examples](EXAMPLES.md) for practical implementations

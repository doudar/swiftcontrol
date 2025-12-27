# Protocol Basics - BLE Communication

## Bluetooth Low Energy Fundamentals

### BLE Architecture

Zwift devices use the **Generic Attribute Profile (GATT)** for communication:

```
Device (Peripheral/Server)
  └── Services
        └── Characteristics
              └── Descriptors
```

### Roles

- **Peripheral (Server)**: The Zwift device (Click, Play, Ride)
- **Central (Client)**: Your application

## Zwift BLE Services

### Service UUIDs

Zwift devices expose two main custom service UUIDs:

#### Legacy Custom Service
```
UUID: 00000001-19CA-4651-86E5-FA29DCDD09D1
Used by: Zwift Click, Zwift Play
```

#### Ride Custom Service
```
UUID: 0000FC82-0000-1000-8000-00805F9B34FB
Short form: FC82
Used by: Zwift Ride, Zwift Click v2
```

### Characteristic Breakdown

Each Zwift service contains these characteristics:

#### 1. Async Characteristic (Notifications)

**UUID**: `00000002-19CA-4651-86E5-FA29DCDD09D1`

**Properties**: 
- Notify

**Purpose**: 
- Asynchronous events from device
- Button presses
- Battery updates (Zwift Ride)

**Data Flow**: Device → App (push)

**Example Data**:
```
// Battery notification (Zwift Ride)
[0x19, 0x08, 0x64]  // Type 0x19, battery 100%

// Button press (Zwift Play)
[0x07, 0x08, 0x01, 0x10, 0x01, ...]  // Protobuf data
```

#### 2. Sync RX Characteristic (Write)

**UUID**: `00000003-19CA-4651-86E5-FA29DCDD09D1`

**Properties**: 
- Write Without Response

**Purpose**: 
- Commands from app to device
- Handshake initialization
- Vibration commands
- Configuration requests

**Data Flow**: App → Device (push)

**Example Commands**:
```
// Handshake (all devices)
[0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e]  // "RideOn"

// Vibration command (Play/Ride)
[0x12, 0x12, 0x08, 0x0A, 0x06, 0x08, 0x02, 0x10, 0x00, 0x18, 0x20]

// Get device info (Ride)
[0x08, 0x08, 0x00]  // Opcode 0x08 (GET), data object 0x00
```

#### 3. Sync TX Characteristic (Indications)

**UUID**: `00000004-19CA-4651-86E5-FA29DCDD09D1`

**Properties**: 
- Read
- Indicate

**Purpose**: 
- Synchronous responses to commands
- Handshake acknowledgment
- Data object responses

**Data Flow**: Device → App (response)

**Example Responses**:
```
// Handshake response (Click)
[0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e,  // "RideOn"
 0x01, 0x03,                          // Response signature
 0x40, 0xEA, 0x05, ...]               // Public key (32 bytes)

// Get response (Ride)
[0x3C, 0x08, 0x00, 0x12, 0x46, ...]  // Opcode 0x3C, protobuf data
```

## Additional BLE Services

### Device Information Service

**UUID**: `0000180A-0000-1000-8000-00805F9B34FB`

Standard BLE service providing:

| Characteristic | UUID | Content |
|---------------|------|---------|
| Manufacturer Name | 0x2A29 | "Zwift" |
| Serial Number | 0x2A25 | Device serial |
| Hardware Revision | 0x2A27 | "A.0", "B.0", etc. |
| Firmware Revision | 0x2A26 | "1.1.0", "1.3.1", etc. |

### Battery Service

**UUID**: `0000180F-0000-1000-8000-00805F9B34FB`

Standard BLE battery service:

| Characteristic | UUID | Content |
|---------------|------|---------|
| Battery Level | 0x2A19 | 0-100 (percentage) |

**Note**: Some devices report battery via custom notifications instead.

## Device Discovery

### Scanning for Zwift Devices

#### Method 1: Service UUID Filter

```dart
// Scan for devices advertising Zwift services
await UniversalBle.startScan(
  scanFilter: ScanFilter(
    withServices: [
      "00000001-19CA-4651-86E5-FA29DCDD09D1",  // Legacy
      "0000FC82-0000-1000-8000-00805F9B34FB",  // Ride
    ],
  ),
);
```

#### Method 2: Manufacturer Data Filter

```dart
// Scan for Zwift manufacturer ID
final devices = await scanWithManufacturerId(0x094A);

// Parse manufacturer data
if (device.manufacturerData.containsKey(0x094A)) {
  final data = device.manufacturerData[0x094A];
  final deviceType = data[0];  // First byte = device type
  
  switch (deviceType) {
    case 0x09: // Zwift Click
    case 0x02: // Zwift Play Right
    case 0x03: // Zwift Play Left
    case 0x07: // Zwift Ride Right
    case 0x08: // Zwift Ride Left
    case 0x0A: // Zwift Click v2 Right
    case 0x0B: // Zwift Click v2 Left
  }
}
```

### Device Identification

#### By Name Pattern

```dart
final deviceName = device.name;

if (deviceName == "Zwift Play") {
  // Zwift Play controller
} else if (deviceName == "Zwift Ride") {
  // Zwift Ride handlebar
} else if (deviceName == "Zwift Click") {
  // Could be v1 or v2 - check manufacturer data
}
```

#### By Manufacturer Data

More reliable than name:

```dart
final manufacturerData = device.manufacturerData[0x094A];
if (manufacturerData != null && manufacturerData.isNotEmpty) {
  final deviceType = manufacturerData[0];
  
  final deviceModel = switch (deviceType) {
    0x09 => "Zwift Click v1",
    0x0A => "Zwift Click v2 Right",
    0x0B => "Zwift Click v2 Left",
    0x02 => "Zwift Play Right",
    0x03 => "Zwift Play Left",
    0x07 => "Zwift Ride Right",
    0x08 => "Zwift Ride Left",
    _ => "Unknown Zwift Device"
  };
}
```

## Connection Process

### 1. Connect to Device

```dart
await UniversalBle.connect(deviceId);
```

### 2. Discover Services

```dart
final services = await UniversalBle.discoverServices(deviceId);

// Find Zwift custom service
final zwiftService = services.firstWhere(
  (s) => s.uuid == "00000001-19CA-4651-86E5-FA29DCDD09D1" ||
         s.uuid == "0000FC82-0000-1000-8000-00805F9B34FB"
);
```

### 3. Get Characteristics

```dart
final asyncChar = zwiftService.characteristics.firstWhere(
  (c) => c.uuid == "00000002-19CA-4651-86E5-FA29DCDD09D1"
);

final syncRxChar = zwiftService.characteristics.firstWhere(
  (c) => c.uuid == "00000003-19CA-4651-86E5-FA29DCDD09D1"
);

final syncTxChar = zwiftService.characteristics.firstWhere(
  (c) => c.uuid == "00000004-19CA-4651-86E5-FA29DCDD09D1"
);
```

### 4. Subscribe to Notifications

```dart
// Subscribe to async notifications (button presses)
await UniversalBle.subscribeNotifications(
  deviceId,
  zwiftService.uuid,
  asyncChar.uuid
);

// Subscribe to sync indications (responses)
await UniversalBle.subscribeIndications(
  deviceId,
  zwiftService.uuid,
  syncTxChar.uuid
);
```

### 5. Perform Handshake

```dart
// Send "RideOn" handshake
final rideOn = Uint8List.fromList([
  0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e  // "RideOn"
]);

await UniversalBle.write(
  deviceId,
  zwiftService.uuid,
  syncRxChar.uuid,
  rideOn,
  withoutResponse: true
);

// Wait for handshake response on syncTxChar
// Response format: RideOn + signature + public key
```

## Message Reception

### Setting Up Listeners

```dart
// Listen for characteristic notifications
UniversalBle.onValueChanged = (deviceId, charUuid, value) {
  if (charUuid == asyncCharUuid) {
    processAsyncNotification(value);
  } else if (charUuid == syncTxCharUuid) {
    processSyncIndication(value);
  }
};
```

### Processing Notifications

```dart
void processAsyncNotification(Uint8List bytes) {
  if (bytes.isEmpty) return;
  
  final messageType = bytes[0];
  final payload = bytes.sublist(1);
  
  switch (messageType) {
    case 0x15: // Empty/keepalive
      break;
    case 0x19: // Battery (Ride)
      final batteryLevel = payload[1];
      updateBattery(batteryLevel);
      break;
    case 0x07: // Button notification (Play/Ride)
      processButtonNotification(payload);
      break;
    case 0x37: // Button notification (Click)
      processButtonNotification(payload);
      break;
  }
}
```

## Message Sending

### Writing Commands

```dart
Future<void> sendCommand(Uint8List command) async {
  await UniversalBle.write(
    deviceId,
    serviceUuid,
    syncRxCharUuid,
    command,
    withoutResponse: true  // Always use write without response
  );
}
```

### Common Commands

#### Vibration (Zwift Play/Ride)

```dart
final vibrateCommand = Uint8List.fromList([
  0x12, 0x12, 0x08, 0x0A, 0x06, 0x08, 
  0x02, 0x10, 0x00, 0x18, 0x20
]);

await sendCommand(vibrateCommand);
```

#### Get Device Info (Zwift Ride)

```dart
final getDeviceInfo = Uint8List.fromList([
  0x08,  // GET opcode
  0x08,  // Length
  0x00   // Data object: PAGE_DEV_INFO
]);

await sendCommand(getDeviceInfo);
```

## Connection Management

### Handling Disconnections

```dart
UniversalBle.onConnectionChanged = (deviceId, isConnected) {
  if (!isConnected) {
    // Clean up
    // Attempt reconnection if desired
    reconnect();
  }
};
```

### Reconnection Strategy

```dart
Future<void> reconnect() async {
  int attempts = 0;
  const maxAttempts = 3;
  
  while (attempts < maxAttempts) {
    try {
      await UniversalBle.connect(deviceId);
      await discoverServices();
      await setupHandshake();
      return; // Success
    } catch (e) {
      attempts++;
      await Future.delayed(Duration(seconds: 2));
    }
  }
}
```

## Data Types and Encoding

### Byte Order

All multi-byte values use **little-endian** encoding:

```dart
// Example: 2-byte value 0x1234
[0x34, 0x12]  // Little-endian
```

### Varints (Protocol Buffers)

Many fields use protobuf varint encoding:

```dart
// Varint encoding examples:
0x00 = [0x00]
0x7F = [0x7F]
0x80 = [0x80, 0x01]
0xFF = [0xFF, 0x01]
```

### Message Framing

No explicit length prefix - messages are self-contained:

```
[Type Byte] [Protobuf Data...]
```

For protobuf messages, the protobuf encoding includes length information.

## Error Handling

### Service Not Found

```dart
try {
  final service = services.firstWhere(
    (s) => s.uuid == zwiftServiceUuid
  );
} catch (e) {
  throw Exception(
    'Zwift service not found. '
    'Device may need firmware update via Zwift Companion app.'
  );
}
```

### Write Failures

```dart
try {
  await UniversalBle.write(...);
} catch (e) {
  // Retry or notify user
  if (e is BleWriteException) {
    // Handle specific BLE error
  }
}
```

### Subscription Failures

```dart
try {
  await UniversalBle.subscribeNotifications(...);
} catch (e) {
  throw Exception('Failed to subscribe to notifications');
}
```

## Platform-Specific Considerations

### Android

```dart
// Request necessary permissions
await Permission.bluetooth.request();
await Permission.bluetoothScan.request();
await Permission.bluetoothConnect.request();
await Permission.location.request();  // Required for BLE scan on Android < 12
```

### iOS/macOS

```plist
<!-- Info.plist -->
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Required to connect to Zwift controllers</string>
```

### Web

```javascript
// Web Bluetooth API
const device = await navigator.bluetooth.requestDevice({
  filters: [{
    services: ['00000001-19ca-4651-86e5-fa29dcdd09d1']
  }]
});
```

**Limitations**:
- Requires HTTPS or localhost
- User gesture required
- Limited manufacturer data access

## Best Practices

1. **Always subscribe before handshake** - Ensure you don't miss the handshake response
2. **Handle missing services gracefully** - Prompt for firmware update
3. **Implement connection timeouts** - BLE operations can hang
4. **Log all messages in debug mode** - Essential for troubleshooting
5. **Use write without response** - Faster and more reliable
6. **Deduplicate notifications** - Devices may send duplicate messages
7. **Monitor RSSI** - Signal strength affects reliability

## Debugging Tips

### Enable BLE Logging

```dart
// Log all BLE events
UniversalBle.onValueChanged = (deviceId, charUuid, value) {
  print('RX $charUuid: ${value.map((b) => b.toRadixString(16).padLeft(2, '0')).join(' ')}');
  processMessage(value);
};
```

### Hex Dump Utility

```dart
String hexDump(Uint8List bytes) {
  return bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join(' ');
}

// Usage
print('Received: ${hexDump(messageBytes)}');
// Output: "52 69 64 65 4f 6e 01 03 40 ea 05..."
```

### Message Tracing

```dart
void traceMessage(String direction, String type, Uint8List bytes) {
  final timestamp = DateTime.now().toString().split(' ').last;
  print('$timestamp $direction [$type]: ${hexDump(bytes)}');
}

// Usage
traceMessage('TX', 'HANDSHAKE', rideOnBytes);
traceMessage('RX', 'BUTTON', notificationBytes);
```

## Next Steps

- Proceed to [Handshake Process](HANDSHAKE.md) for connection establishment details
- Review [Message Types](MESSAGE_TYPES.md) for message format specifications
- See [Device Types](DEVICE_TYPES.md) for device-specific implementation details

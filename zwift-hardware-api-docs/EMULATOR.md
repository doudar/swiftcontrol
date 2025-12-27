# Building a Zwift Device Emulator

## Overview

A Zwift device emulator acts as a BLE peripheral that mimics a Zwift controller (Play, Click, or Ride). This allows Zwift or other apps to connect to your software instead of physical hardware.

This guide explains how to build an emulator based on the BikeControl implementation.

## Why Build an Emulator?

- **Testing**: Test your Zwift integration without physical hardware
- **Development**: Develop apps that work with Zwift controllers
- **Bridging**: Create virtual devices that relay inputs from other sources
- **Simulation**: Simulate button presses programmatically

## Architecture

```
┌──────────────────┐         BLE          ┌─────────────────┐
│  Your Emulator   │ ←──────────────────→ │  Zwift App      │
│  (Peripheral)    │   GATT Protocol      │  (Central)      │
└──────────────────┘                      └─────────────────┘
        │
        │  Simulated button presses
        │  Battery updates
        ▼
```

## Requirements

### Platform Support

**Android**: ✅ Full support with `bluetooth_low_energy` package

**iOS/macOS**: ✅ Supported via Core Bluetooth

**Linux**: ⚠️ Limited support

**Windows**: ⚠️ Limited support

**Web**: ❌ Not supported (Web Bluetooth doesn't support peripheral mode)

### Dependencies

```yaml
dependencies:
  bluetooth_low_energy: ^6.0.0
  permission_handler: ^11.0.0
  protobuf: ^3.0.0
```

## Implementation Steps

### Step 1: Set Up Peripheral Manager

```dart
import 'package:bluetooth_low_energy/bluetooth_low_energy.dart';

class ZwiftEmulator {
  late final PeripheralManager _peripheralManager;
  Central? _connectedCentral;
  
  ZwiftEmulator() {
    _peripheralManager = PeripheralManager();
    setupListeners();
  }
  
  void setupListeners() {
    // Listen for state changes
    _peripheralManager.stateChanged.forEach((state) {
      print('Peripheral state: ${state.state}');
    });
    
    // Listen for connections
    _peripheralManager.connectionStateChanged.forEach((state) {
      if (state.state == ConnectionState.connected) {
        _connectedCentral = state.central;
        print('Zwift connected!');
      } else if (state.state == ConnectionState.disconnected) {
        _connectedCentral = null;
        print('Zwift disconnected');
      }
    });
  }
}
```

### Step 2: Create GATT Service

```dart
Future<void> addZwiftService() async {
  // Create characteristics
  final asyncChar = GATTCharacteristic.mutable(
    uuid: UUID.fromString('00000002-19CA-4651-86E5-FA29DCDD09D1'),
    descriptors: [],
    properties: [GATTCharacteristicProperty.notify],
    permissions: [],
  );
  
  final syncRxChar = GATTCharacteristic.mutable(
    uuid: UUID.fromString('00000003-19CA-4651-86E5-FA29DCDD09D1'),
    descriptors: [],
    properties: [GATTCharacteristicProperty.writeWithoutResponse],
    permissions: [],
  );
  
  final syncTxChar = GATTCharacteristic.mutable(
    uuid: UUID.fromString('00000004-19CA-4651-86E5-FA29DCDD09D1'),
    descriptors: [],
    properties: [
      GATTCharacteristicProperty.read,
      GATTCharacteristicProperty.indicate,
    ],
    permissions: [GATTCharacteristicPermission.read],
  );
  
  // Create service
  final service = GATTService(
    uuid: UUID.fromString('00000001-19CA-4651-86E5-FA29DCDD09D1'),
    isPrimary: true,
    characteristics: [asyncChar, syncRxChar, syncTxChar],
    includedServices: [],
  );
  
  // Add service to peripheral
  await _peripheralManager.addService(service);
  
  print('Zwift service added');
}
```

### Step 3: Add Device Information Service

```dart
Future<void> addDeviceInfoService() async {
  final service = GATTService(
    uuid: UUID.fromString('180A'),
    isPrimary: true,
    characteristics: [
      // Manufacturer name
      GATTCharacteristic.immutable(
        uuid: UUID.fromString('2A29'),
        value: Uint8List.fromList('BikeControl'.codeUnits),
        descriptors: [],
      ),
      // Serial number
      GATTCharacteristic.immutable(
        uuid: UUID.fromString('2A25'),
        value: Uint8List.fromList('EMULATOR-001'.codeUnits),
        descriptors: [],
      ),
      // Hardware revision
      GATTCharacteristic.immutable(
        uuid: UUID.fromString('2A27'),
        value: Uint8List.fromList('A.0'.codeUnits),
        descriptors: [],
      ),
      // Firmware revision
      GATTCharacteristic.immutable(
        uuid: UUID.fromString('2A26'),
        value: Uint8List.fromList('1.2.0'.codeUnits),
        descriptors: [],
      ),
    ],
    includedServices: [],
  );
  
  await _peripheralManager.addService(service);
}
```

### Step 4: Add Battery Service

```dart
Future<void> addBatteryService() async {
  final batteryChar = GATTCharacteristic.mutable(
    uuid: UUID.fromString('2A19'),
    descriptors: [],
    properties: [
      GATTCharacteristicProperty.read,
      GATTCharacteristicProperty.notify,
    ],
    permissions: [GATTCharacteristicPermission.read],
  );
  
  final service = GATTService(
    uuid: UUID.fromString('180F'),
    isPrimary: true,
    characteristics: [batteryChar],
    includedServices: [],
  );
  
  await _peripheralManager.addService(service);
}
```

### Step 5: Start Advertising

```dart
Future<void> startAdvertising() async {
  // For Zwift Ride emulator
  final advertisement = Advertisement(
    name: 'KICKR BIKE PRO 1337',  // Or any name
    serviceUUIDs: [
      UUID.fromString('FC82'),  // Short UUID for Ride
    ],
  );
  
  await _peripheralManager.startAdvertising(advertisement);
  print('Advertising as Zwift Ride');
}

// For legacy devices (Click/Play)
Future<void> startAdvertisingLegacy() async {
  final advertisement = Advertisement(
    name: 'Zwift Play',
    serviceUUIDs: [
      UUID.fromString('00000001-19CA-4651-86E5-FA29DCDD09D1'),
    ],
  );
  
  await _peripheralManager.startAdvertising(advertisement);
}
```

### Step 6: Handle Write Requests (Handshake)

```dart
void setupWriteHandler() {
  _peripheralManager.characteristicWriteRequested.forEach((event) async {
    final value = event.request.value;
    
    print('Write request: ${hexDump(value)}');
    
    // Check for handshake
    if (value.startsWith([0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e])) {
      print('Handshake received');
      await handleHandshake(event.central);
    }
    
    // Respond to write
    await _peripheralManager.respondWriteRequest(event.request);
  });
}

Future<void> handleHandshake(Central central) async {
  // Send handshake response
  final response = Uint8List.fromList([
    0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e,  // "RideOn"
  ]);
  
  await _peripheralManager.notifyCharacteristic(
    central,
    _syncTxChar!,
    value: response,
  );
  
  print('Handshake response sent');
  
  // Start sending keepalive messages
  startKeepalive(central);
}
```

### Step 7: Send Button Press Notifications

#### For Zwift Ride

```dart
Future<void> sendButtonPress(
  Central central,
  RideButtonMask button,
) async {
  // Create button press message
  final status = RideKeyPadStatus()
    ..buttonMap = (~button.mask) & 0xFFFFFFFF
    ..analogPaddles.clear();
  
  final message = Uint8List.fromList([
    Opcode.CONTROLLER_NOTIFICATION.value,
    ...status.writeToBuffer(),
  ]);
  
  // Send notification
  await _peripheralManager.notifyCharacteristic(
    central,
    _asyncChar!,
    value: message,
  );
  
  print('Button press sent: ${button.name}');
}

// Example usage
await sendButtonPress(central, RideButtonMask.A_BTN);
```

#### For Zwift Play

```dart
Future<void> sendPlayButtonPress(
  Central central,
  PlayButton button,
) async {
  final status = PlayKeyPadStatus()
    ..rightPad = PlayButtonStatus.ON
    ..buttonARight = (button == PlayButton.A) 
      ? PlayButtonStatus.ON 
      : PlayButtonStatus.OFF
    ..buttonBDown = (button == PlayButton.B) 
      ? PlayButtonStatus.ON 
      : PlayButtonStatus.OFF
    // ... more buttons
    ..analogLR = 0;
  
  final message = Uint8List.fromList([
    0x07,  // PLAY_NOTIFICATION_MESSAGE_TYPE
    ...status.writeToBuffer(),
  ]);
  
  await _peripheralManager.notifyCharacteristic(
    central,
    _asyncChar!,
    value: message,
  );
}
```

### Step 8: Send Battery Updates

```dart
Future<void> sendBatteryUpdate(Central central, int percentage) async {
  final notification = BatteryNotification()
    ..newPercLevel = percentage;
  
  final message = Uint8List.fromList([
    Opcode.BATTERY_NOTIF.value,
    ...notification.writeToBuffer(),
  ]);
  
  await _peripheralManager.notifyCharacteristic(
    central,
    _asyncChar!,
    value: message,
  );
}
```

### Step 9: Implement Keepalive

```dart
Timer? _keepaliveTimer;

void startKeepalive(Central central) {
  _keepaliveTimer?.cancel();
  
  _keepaliveTimer = Timer.periodic(Duration(seconds: 5), (timer) {
    if (_connectedCentral == null) {
      timer.cancel();
      return;
    }
    
    // Send "no buttons pressed" message
    sendNoButtonsPressed(central);
  });
}

Future<void> sendNoButtonsPressed(Central central) async {
  final status = RideKeyPadStatus()
    ..buttonMap = 0xFFFFFFFF  // All released
    ..analogPaddles.clear();
  
  final message = Uint8List.fromList([
    Opcode.CONTROLLER_NOTIFICATION.value,
    ...status.writeToBuffer(),
  ]);
  
  await _peripheralManager.notifyCharacteristic(
    central,
    _syncTxChar!,
    value: message,
  );
}
```

## Complete Emulator Example

```dart
class ZwiftRideEmulator {
  late final PeripheralManager _peripheralManager;
  Central? _central;
  GATTCharacteristic? _asyncChar;
  GATTCharacteristic? _syncTxChar;
  
  Future<void> start() async {
    _peripheralManager = PeripheralManager();
    
    // Setup listeners
    setupListeners();
    
    // Wait for Bluetooth
    while (_peripheralManager.state != BluetoothLowEnergyState.poweredOn) {
      await Future.delayed(Duration(seconds: 1));
    }
    
    // Add services
    await addDeviceInfoService();
    await addBatteryService();
    await addZwiftService();
    
    // Setup handlers
    setupWriteHandler();
    setupReadHandler();
    
    // Start advertising
    await startAdvertising();
    
    print('Emulator started!');
  }
  
  Future<void> stop() async {
    await _peripheralManager.stopAdvertising();
    await _peripheralManager.removeAllServices();
  }
  
  // Simulate button press
  Future<void> pressButton(RideButtonMask button) async {
    if (_central == null) {
      print('No device connected');
      return;
    }
    
    // Send button down
    await sendButtonPress(_central!, button);
    
    // Wait 100ms
    await Future.delayed(Duration(milliseconds: 100));
    
    // Send button up
    await sendNoButtonsPressed(_central!);
  }
}

// Usage
void main() async {
  final emulator = ZwiftRideEmulator();
  await emulator.start();
  
  // Wait for Zwift to connect
  await Future.delayed(Duration(seconds: 10));
  
  // Simulate button presses
  await emulator.pressButton(RideButtonMask.A_BTN);
  await Future.delayed(Duration(seconds: 1));
  await emulator.pressButton(RideButtonMask.SHFT_UP_R_BTN);
}
```

## Testing Your Emulator

### 1. Connect with Zwift

1. Start your emulator
2. Open Zwift app
3. Go to controller pairing
4. Look for your emulated device name
5. Pair and test

### 2. Verify with BikeControl

Use BikeControl itself to connect to your emulator:
1. Run BikeControl
2. Scan for devices
3. Connect to your emulator
4. Test button presses

### 3. Monitor BLE Traffic

Use platform-specific tools:

**Android**: nRF Connect app
**iOS**: LightBlue app
**macOS**: Bluetooth Explorer (Xcode)

## Platform-Specific Notes

### Android Permissions

```dart
// Request permissions
await Permission.bluetoothAdvertise.request();
await Permission.bluetooth.request();
await Permission.bluetoothConnect.request();
```

**AndroidManifest.xml**:
```xml
<uses-permission android:name="android.permission.BLUETOOTH" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" />
<uses-permission android:name="android.permission.BLUETOOTH_ADVERTISE" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
```

### iOS Configuration

**Info.plist**:
```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Required to emulate Zwift controller</string>
<key>NSBluetoothPeripheralUsageDescription</key>
<string>Required to advertise as BLE peripheral</string>
```

## Advanced Features

### Supporting Multiple Devices

```dart
// Track multiple connections
Map<Central, DeviceState> connectedDevices = {};

_peripheralManager.connectionStateChanged.forEach((state) {
  if (state.state == ConnectionState.connected) {
    connectedDevices[state.central] = DeviceState();
  } else {
    connectedDevices.remove(state.central);
  }
});
```

### Paddle Simulation

```dart
Future<void> sendPaddlePress(
  Central central,
  RideAnalogKeyLocation location,
  int value,  // -100 to +100
) async {
  final paddle = RideAnalogKeyPress()
    ..location = location
    ..analogValue = value;
  
  final status = RideKeyPadStatus()
    ..buttonMap = 0xFFFFFFFF
    ..analogPaddles.add(paddle);
  
  final message = Uint8List.fromList([
    Opcode.CONTROLLER_NOTIFICATION.value,
    ...status.writeToBuffer(),
  ]);
  
  await _peripheralManager.notifyCharacteristic(
    central,
    _asyncChar!,
    value: message,
  );
}

// Usage
await sendPaddlePress(central, RideAnalogKeyLocation.L0, 100);
```

### Responding to GET Requests

```dart
void setupReadHandler() {
  _peripheralManager.characteristicReadRequested.forEach((event) async {
    if (event.characteristic.uuid.toString() == '2A19') {
      // Battery level read
      await _peripheralManager.respondReadRequestWithValue(
        event.request,
        value: Uint8List.fromList([100]),  // 100% battery
      );
    }
  });
}
```

## Debugging

### Enable Logging

```dart
void logMessage(String direction, Uint8List bytes) {
  final timestamp = DateTime.now().toString().split(' ').last;
  final hex = bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join(' ');
  print('$timestamp $direction: $hex');
}
```

### Common Issues

**Issue**: Device not appearing in scan

**Solution**: 
- Check Bluetooth is powered on
- Verify advertising is started
- Ensure service UUIDs are correct

**Issue**: Connection drops immediately

**Solution**:
- Respond to all write requests
- Send periodic keepalive messages
- Handle read requests properly

## Best Practices

1. **Always send handshake response** immediately
2. **Implement keepalive** to maintain connection
3. **Release all buttons** between presses
4. **Test with real Zwift** before relying on emulator
5. **Log all BLE operations** during development
6. **Handle disconnections** gracefully
7. **Clean up resources** when stopping

## Next Steps

- Review [Examples](EXAMPLES.md) for complete code samples
- See [Protocol Basics](PROTOCOL_BASICS.md) for BLE details
- Check [Message Types](MESSAGE_TYPES.md) for message formats

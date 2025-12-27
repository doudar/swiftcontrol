# Code Examples - Practical Implementations

## Overview

This document provides complete, working code examples for implementing the Zwift Hardware API. All examples are based on the BikeControl implementation.

## Example 1: Connect to Zwift Click

### Complete Implementation

```dart
import 'package:universal_ble/universal_ble.dart';
import 'package:bike_control/bluetooth/devices/zwift/protocol/zwift.pb.dart';

class ZwiftClickConnection {
  String? deviceId;
  String? serviceUuid;
  String? asyncCharUuid;
  String? syncRxCharUuid;
  String? syncTxCharUuid;
  
  int? batteryLevel;
  bool isConnected = false;
  
  // Button press callback
  Function(List<String> buttons)? onButtonPress;
  
  Future<void> connect() async {
    // 1. Scan for devices
    print('Scanning for Zwift Click...');
    
    UniversalBle.onScanResult = (device) async {
      if (device.name == 'Zwift Click') {
        print('Found Zwift Click: ${device.deviceId}');
        
        // Stop scanning
        await UniversalBle.stopScan();
        
        // Connect to device
        await connectToDevice(device.deviceId);
      }
    };
    
    await UniversalBle.startScan(
      scanFilter: ScanFilter(
        withServices: ['00000001-19CA-4651-86E5-FA29DCDD09D1'],
      ),
    );
  }
  
  Future<void> connectToDevice(String id) async {
    deviceId = id;
    
    // 2. Connect
    print('Connecting...');
    await UniversalBle.connect(deviceId!);
    
    // 3. Discover services
    print('Discovering services...');
    await Future.delayed(Duration(milliseconds: 500));
    
    final services = await UniversalBle.discoverServices(deviceId!);
    
    final service = services.firstWhere(
      (s) => s.uuid.toLowerCase() == '00000001-19ca-4651-86e5-fa29dcdd09d1',
    );
    
    serviceUuid = service.uuid;
    
    // 4. Get characteristics
    asyncCharUuid = service.characteristics
      .firstWhere((c) => c.uuid.toLowerCase() == '00000002-19ca-4651-86e5-fa29dcdd09d1')
      .uuid;
    
    syncRxCharUuid = service.characteristics
      .firstWhere((c) => c.uuid.toLowerCase() == '00000003-19ca-4651-86e5-fa29dcdd09d1')
      .uuid;
    
    syncTxCharUuid = service.characteristics
      .firstWhere((c) => c.uuid.toLowerCase() == '00000004-19ca-4651-86e5-fa29dcdd09d1')
      .uuid;
    
    // 5. Setup listener
    setupListener();
    
    // 6. Subscribe to characteristics
    print('Subscribing to notifications...');
    await UniversalBle.subscribeNotifications(
      deviceId!,
      serviceUuid!,
      asyncCharUuid!,
    );
    
    await UniversalBle.subscribeIndications(
      deviceId!,
      serviceUuid!,
      syncTxCharUuid!,
    );
    
    // 7. Send handshake
    print('Sending handshake...');
    final handshake = Uint8List.fromList([
      0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e,  // "RideOn"
    ]);
    
    await UniversalBle.write(
      deviceId!,
      serviceUuid!,
      syncRxCharUuid!,
      handshake,
      withoutResponse: true,
    );
    
    print('Connected to Zwift Click!');
  }
  
  void setupListener() {
    UniversalBle.onValueChanged = (id, charUuid, value) {
      if (id != deviceId) return;
      
      if (charUuid.toLowerCase() == asyncCharUuid!.toLowerCase()) {
        handleNotification(value);
      } else if (charUuid.toLowerCase() == syncTxCharUuid!.toLowerCase()) {
        handleIndication(value);
      }
    };
  }
  
  void handleIndication(Uint8List bytes) {
    // Check for handshake response
    if (bytes.startsWith([0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e])) {
      print('Handshake acknowledged');
      isConnected = true;
    }
  }
  
  void handleNotification(Uint8List bytes) {
    if (bytes.isEmpty) return;
    
    final messageType = bytes[0];
    final payload = bytes.sublist(1);
    
    switch (messageType) {
      case 0x15:  // Empty/keepalive
        break;
        
      case 0x19:  // Battery
        batteryLevel = payload[1];
        print('Battery: $batteryLevel%');
        break;
        
      case 0x37:  // Click button notification
        handleButtonNotification(payload);
        break;
    }
  }
  
  void handleButtonNotification(Uint8List payload) {
    final status = ClickKeyPadStatus.fromBuffer(payload);
    
    final buttons = <String>[];
    if (status.buttonPlus == PlayButtonStatus.ON) {
      buttons.add('PLUS');
    }
    if (status.buttonMinus == PlayButtonStatus.ON) {
      buttons.add('MINUS');
    }
    
    if (buttons.isNotEmpty) {
      print('Buttons pressed: ${buttons.join(", ")}');
      onButtonPress?.call(buttons);
    }
  }
  
  Future<void> disconnect() async {
    if (deviceId != null) {
      await UniversalBle.disconnect(deviceId!);
      isConnected = false;
      print('Disconnected');
    }
  }
}

// Usage
void main() async {
  final connection = ZwiftClickConnection();
  
  connection.onButtonPress = (buttons) {
    print('User pressed: $buttons');
    // Handle button actions
  };
  
  await connection.connect();
  
  // Wait for button presses
  await Future.delayed(Duration(minutes: 5));
  
  await connection.disconnect();
}
```

---

## Example 2: Connect to Zwift Play

```dart
class ZwiftPlayConnection {
  // ... similar setup as ZwiftClickConnection
  
  void handleButtonNotification(Uint8List payload) {
    final status = PlayKeyPadStatus.fromBuffer(payload);
    
    final buttons = <String>[];
    
    if (status.rightPad == PlayButtonStatus.ON) {
      // Action mode
      if (status.buttonYUp == PlayButtonStatus.ON) buttons.add('Y');
      if (status.buttonZLeft == PlayButtonStatus.ON) buttons.add('Z');
      if (status.buttonARight == PlayButtonStatus.ON) buttons.add('A');
      if (status.buttonBDown == PlayButtonStatus.ON) buttons.add('B');
      if (status.buttonShift == PlayButtonStatus.ON) buttons.add('SHIFT_RIGHT');
      
      // Check paddle
      if (status.analogLR.abs() == 100) {
        buttons.add('PADDLE_RIGHT');
      }
    } else {
      // Navigation mode
      if (status.buttonYUp == PlayButtonStatus.ON) buttons.add('UP');
      if (status.buttonZLeft == PlayButtonStatus.ON) buttons.add('LEFT');
      if (status.buttonARight == PlayButtonStatus.ON) buttons.add('RIGHT');
      if (status.buttonBDown == PlayButtonStatus.ON) buttons.add('DOWN');
      if (status.buttonShift == PlayButtonStatus.ON) buttons.add('SHIFT_LEFT');
      
      // Check paddle
      if (status.analogLR.abs() == 100) {
        buttons.add('PADDLE_LEFT');
      }
    }
    
    if (buttons.isNotEmpty) {
      print('Buttons pressed: ${buttons.join(", ")}');
      onButtonPress?.call(buttons);
    }
  }
  
  // Vibrate the controller
  Future<void> vibrate() async {
    final vibrateCommand = Uint8List.fromList([
      0x12, 0x12, 0x08, 0x0A, 0x06, 0x08, 
      0x02, 0x10, 0x00, 0x18, 0x20
    ]);
    
    await UniversalBle.write(
      deviceId!,
      serviceUuid!,
      syncRxCharUuid!,
      vibrateCommand,
      withoutResponse: true,
    );
    
    print('Vibration sent');
  }
}
```

---

## Example 3: Connect to Zwift Ride

```dart
import 'package:bike_control/bluetooth/devices/zwift/protocol/zp.pb.dart';

class ZwiftRideConnection {
  // Service UUID is different for Ride
  static const SERVICE_UUID = '0000FC82-0000-1000-8000-00805F9B34FB';
  
  // ... similar connection setup
  
  void handleNotification(Uint8List bytes) {
    if (bytes.isEmpty) return;
    
    final opcode = Opcode.valueOf(bytes[0]);
    final payload = bytes.sublist(1);
    
    switch (opcode) {
      case Opcode.RIDE_ON:
        print('Handshake acknowledged');
        isConnected = true;
        break;
        
      case Opcode.BATTERY_NOTIF:
        handleBatteryNotification(payload);
        break;
        
      case Opcode.CONTROLLER_NOTIFICATION:
        handleButtonNotification(payload);
        break;
        
      case Opcode.LOG_DATA:
        final log = LogDataNotification.fromBuffer(payload);
        print('Device log: ${log.message}');
        break;
    }
  }
  
  void handleBatteryNotification(Uint8List payload) {
    final notification = BatteryNotification.fromBuffer(payload);
    batteryLevel = notification.newPercLevel;
    print('Battery: $batteryLevel%');
  }
  
  void handleButtonNotification(Uint8List payload) {
    final status = RideKeyPadStatus.fromBuffer(payload);
    
    final buttons = <String>[];
    
    // Check digital buttons (inverted logic: 0 = pressed)
    if (status.buttonMap & 0x00001 == 0) buttons.add('LEFT');
    if (status.buttonMap & 0x00002 == 0) buttons.add('UP');
    if (status.buttonMap & 0x00004 == 0) buttons.add('RIGHT');
    if (status.buttonMap & 0x00008 == 0) buttons.add('DOWN');
    if (status.buttonMap & 0x00010 == 0) buttons.add('A');
    if (status.buttonMap & 0x00020 == 0) buttons.add('B');
    if (status.buttonMap & 0x00040 == 0) buttons.add('Y');
    if (status.buttonMap & 0x00080 == 0) buttons.add('Z');
    if (status.buttonMap & 0x00100 == 0) buttons.add('SHIFT_UP_L');
    if (status.buttonMap & 0x00200 == 0) buttons.add('SHIFT_DN_L');
    if (status.buttonMap & 0x01000 == 0) buttons.add('SHIFT_UP_R');
    if (status.buttonMap & 0x02000 == 0) buttons.add('SHIFT_DN_R');
    
    // Check analog paddles
    for (final paddle in status.analogPaddles) {
      if (paddle.analogValue.abs() >= 25) {
        final location = paddle.location == RideAnalogKeyLocation.L0 
          ? 'LEFT' 
          : 'RIGHT';
        buttons.add('PADDLE_$location (${paddle.analogValue})');
      }
    }
    
    if (buttons.isNotEmpty) {
      print('Buttons pressed: ${buttons.join(", ")}');
      onButtonPress?.call(buttons);
    }
  }
  
  // Request device information
  Future<void> requestDeviceInfo() async {
    final get = Get()..dataObjectId = DO.PAGE_DEV_INFO.value;
    
    final command = Uint8List.fromList([
      Opcode.GET.value,
      ...get.writeToBuffer(),
    ]);
    
    await UniversalBle.write(
      deviceId!,
      serviceUuid!,
      syncRxCharUuid!,
      command,
      withoutResponse: true,
    );
  }
  
  void handleGetResponse(Uint8List payload) {
    final response = GetResponse.fromBuffer(payload);
    
    if (response.dataObjectId == DO.PAGE_DEV_INFO.value) {
      final devInfo = DevInfoPage.fromBuffer(response.dataObjectData);
      print('Device: ${String.fromCharCodes(devInfo.deviceName)}');
      print('Serial: ${String.fromCharCodes(devInfo.serialNumber)}');
      print('Firmware: ${devInfo.systemFwVersion.join(".")}');
    }
  }
}
```

---

## Example 4: Trigger Actions in Zwift

```dart
import 'package:keypress_simulator/keypress_simulator.dart';

class ZwiftActionHandler {
  final _keypress = KeypressSimulator();
  
  Future<void> handleButtonAction(String button) async {
    // Map buttons to Zwift keyboard shortcuts
    final keyMap = {
      'PLUS': 'ArrowUp',        // Shift up
      'MINUS': 'ArrowDown',      // Shift down
      'A': 'Space',              // Select / Power-up
      'B': 'Escape',             // Back
      'Z': 'F',                  // Elbow flick
      'Y': 'Space',              // Power-up
      'UP': 'H',                 // Toggle UI
      'DOWN': 'ArrowDown',       // U-turn
      'LEFT': 'ArrowLeft',       // Steer left
      'RIGHT': 'ArrowRight',     // Steer right
      'SHIFT_UP_R': 'ArrowUp',
      'SHIFT_DN_L': 'ArrowDown',
    };
    
    final key = keyMap[button];
    if (key != null) {
      await _keypress.simulateKeyPress(key);
      print('Sent key: $key');
    }
  }
}

// Combined usage
void main() async {
  final connection = ZwiftRideConnection();
  final actionHandler = ZwiftActionHandler();
  
  connection.onButtonPress = (buttons) async {
    for (final button in buttons) {
      await actionHandler.handleButtonAction(button);
    }
  };
  
  await connection.connect();
}
```

---

## Example 5: Multi-Device Management

```dart
class ZwiftDeviceManager {
  final Map<String, ZwiftDevice> devices = {};
  
  Future<void> scanAndConnect() async {
    UniversalBle.onScanResult = (device) async {
      if (isZwiftDevice(device)) {
        print('Found ${device.name}');
        
        // Create appropriate device instance
        final zwiftDevice = createDevice(device);
        await zwiftDevice.connect();
        
        devices[device.deviceId] = zwiftDevice;
      }
    };
    
    await UniversalBle.startScan();
  }
  
  bool isZwiftDevice(BleDevice device) {
    return device.name == 'Zwift Click' ||
           device.name == 'Zwift Play' ||
           device.name == 'Zwift Ride';
  }
  
  ZwiftDevice createDevice(BleDevice device) {
    switch (device.name) {
      case 'Zwift Click':
        return ZwiftClickConnection();
      case 'Zwift Play':
        return ZwiftPlayConnection();
      case 'Zwift Ride':
        return ZwiftRideConnection();
      default:
        throw Exception('Unknown device: ${device.name}');
    }
  }
  
  Future<void> disconnectAll() async {
    for (final device in devices.values) {
      await device.disconnect();
    }
    devices.clear();
  }
}
```

---

## Example 6: Button Debouncing

```dart
class ButtonDebouncer {
  final Duration debounceTime;
  final Map<String, Timer> _timers = {};
  final Function(String) onButtonPress;
  
  ButtonDebouncer({
    this.debounceTime = const Duration(milliseconds: 200),
    required this.onButtonPress,
  });
  
  void handleButtonPress(String button) {
    // Cancel existing timer
    _timers[button]?.cancel();
    
    // Create new timer
    _timers[button] = Timer(debounceTime, () {
      onButtonPress(button);
      _timers.remove(button);
    });
  }
  
  void dispose() {
    for (final timer in _timers.values) {
      timer.cancel();
    }
    _timers.clear();
  }
}

// Usage
final debouncer = ButtonDebouncer(
  onButtonPress: (button) {
    print('Debounced button press: $button');
    handleAction(button);
  },
);

connection.onButtonPress = (buttons) {
  for (final button in buttons) {
    debouncer.handleButtonPress(button);
  }
};
```

---

## Example 7: Logging and Debugging

```dart
class BLELogger {
  bool enabled = true;
  
  void logConnection(String deviceId, String event) {
    if (!enabled) return;
    final timestamp = DateTime.now().toString();
    print('[$timestamp] CONNECTION: $deviceId - $event');
  }
  
  void logMessage(String direction, Uint8List bytes) {
    if (!enabled) return;
    
    final timestamp = DateTime.now().toString().split(' ').last;
    final hex = bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join(' ');
    final ascii = String.fromCharCodes(bytes.where((b) => b >= 32 && b < 127));
    
    print('$timestamp $direction: $hex');
    if (ascii.isNotEmpty) {
      print('           ASCII: $ascii');
    }
  }
  
  void logButton(String button, bool isPressed) {
    if (!enabled) return;
    
    final state = isPressed ? 'PRESSED' : 'RELEASED';
    print('[BUTTON] $button $state');
  }
}

// Usage
final logger = BLELogger();

UniversalBle.onConnectionChanged = (deviceId, isConnected) {
  logger.logConnection(deviceId, isConnected ? 'CONNECTED' : 'DISCONNECTED');
};

UniversalBle.onValueChanged = (deviceId, charUuid, value) {
  logger.logMessage('RX', value);
  processMessage(value);
};
```

---

## Example 8: Error Handling and Recovery

```dart
class RobustZwiftConnection {
  int connectionAttempts = 0;
  final int maxAttempts = 3;
  bool isReconnecting = false;
  
  Future<void> connectWithRetry() async {
    while (connectionAttempts < maxAttempts) {
      try {
        await connect();
        connectionAttempts = 0;  // Reset on success
        return;
      } catch (e) {
        connectionAttempts++;
        print('Connection attempt $connectionAttempts failed: $e');
        
        if (connectionAttempts < maxAttempts) {
          final delay = Duration(seconds: connectionAttempts * 2);
          print('Retrying in ${delay.inSeconds}s...');
          await Future.delayed(delay);
        }
      }
    }
    
    throw Exception('Failed to connect after $maxAttempts attempts');
  }
  
  void setupAutoReconnect() {
    UniversalBle.onConnectionChanged = (deviceId, isConnected) {
      if (!isConnected && !isReconnecting) {
        isReconnecting = true;
        print('Connection lost, attempting to reconnect...');
        
        connectWithRetry().then((_) {
          print('Reconnected successfully');
          isReconnecting = false;
        }).catchError((e) {
          print('Reconnection failed: $e');
          isReconnecting = false;
        });
      }
    };
  }
}
```

---

## Helper Functions

### Hex Dump Utility

```dart
String hexDump(Uint8List bytes) {
  return bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join(' ');
}

void prettyPrintMessage(String label, Uint8List bytes) {
  print('=== $label ===');
  print('Length: ${bytes.length} bytes');
  print('Hex: ${hexDump(bytes)}');
  print('Dec: ${bytes.join(' ')}');
  
  final ascii = String.fromCharCodes(bytes.where((b) => b >= 32 && b < 127));
  if (ascii.isNotEmpty) {
    print('ASCII: $ascii');
  }
  print('');
}
```

### List Extension Helper

```dart
extension Uint8ListExtension on Uint8List {
  bool startsWith(List<int> prefix) {
    if (length < prefix.length) return false;
    for (int i = 0; i < prefix.length; i++) {
      if (this[i] != prefix[i]) return false;
    }
    return true;
  }
  
  bool equals(List<int> other) {
    if (length != other.length) return false;
    for (int i = 0; i < length; i++) {
      if (this[i] != other[i]) return false;
    }
    return true;
  }
}
```

## Next Steps

- Review [Troubleshooting](TROUBLESHOOTING.md) for common issues
- See [API Overview](API_OVERVIEW.md) for architecture details
- Check [Device Types](DEVICE_TYPES.md) for device-specific information

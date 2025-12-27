# Troubleshooting Guide

## Common Issues and Solutions

This guide helps you troubleshoot problems when implementing or using the Zwift Hardware API.

---

## Connection Issues

### Device Not Found During Scan

**Symptoms**:
- Device doesn't appear in scan results
- Scan times out without finding device

**Possible Causes & Solutions**:

1. **Device is already connected**
   - Disconnect from other apps (Zwift Companion, Zwift app)
   - Power cycle the device
   - Check Bluetooth settings for paired devices

2. **Bluetooth not powered on**
   ```dart
   if (bluetoothState != BluetoothLowEnergyState.poweredOn) {
     print('Bluetooth is off');
     // Prompt user to enable Bluetooth
   }
   ```

3. **Missing permissions** (Android)
   ```dart
   // Request all necessary permissions
   await Permission.bluetooth.request();
   await Permission.bluetoothScan.request();
   await Permission.bluetoothConnect.request();
   await Permission.location.request();  // Required on Android < 12
   ```

4. **Incorrect service UUID filter**
   ```dart
   // Make sure you're using the right UUID
   await UniversalBle.startScan(
     scanFilter: ScanFilter(
       withServices: [
         '00000001-19CA-4651-86E5-FA29DCDD09D1',  // Legacy
         '0000FC82-0000-1000-8000-00805F9B34FB',  // Ride
       ],
     ),
   );
   ```

5. **Device out of range or battery dead**
   - Move closer to device
   - Replace batteries
   - Charge device

---

### Connection Established But Drops Immediately

**Symptoms**:
- Device connects then disconnects within seconds
- No handshake response received

**Possible Causes & Solutions**:

1. **Handshake not sent**
   ```dart
   // Must send handshake immediately after subscribing
   await UniversalBle.subscribeNotifications(...);
   await UniversalBle.subscribeIndications(...);
   
   // Send handshake right away
   await UniversalBle.write(..., RIDE_ON, ...);
   ```

2. **Write request not acknowledged**
   ```dart
   // For emulators: always respond to write requests
   _peripheralManager.characteristicWriteRequested.forEach((event) async {
     // Process write
     handleWrite(event.request.value);
     
     // MUST acknowledge the write
     await _peripheralManager.respondWriteRequest(event.request);
   });
   ```

3. **Device already connected elsewhere**
   - Close Zwift app
   - Close Zwift Companion app
   - Disconnect from other Bluetooth devices

4. **Insufficient connection interval**
   - BLE connection parameters may need adjustment
   - Device may require faster communication

---

### Service Not Found Error

**Symptoms**:
```
Exception: Zwift service not found
```

**Possible Causes & Solutions**:

1. **Outdated firmware**
   - Update firmware via Zwift Companion app
   - Check device firmware version
   - Latest versions:
     - Zwift Click: 1.1.0
     - Zwift Play: 1.3.1
     - Zwift Ride: 1.2.0

2. **Wrong device model**
   - Verify you're connecting to the correct side (left for Ride/Click v2)
   - Check manufacturer data to identify device type

3. **Services not fully discovered**
   ```dart
   // Add delay after connection
   await UniversalBle.connect(deviceId);
   await Future.delayed(Duration(milliseconds: 500));
   final services = await UniversalBle.discoverServices(deviceId);
   ```

---

## Message Processing Issues

### No Messages Received After Connection

**Symptoms**:
- Connection successful
- No button press notifications
- No battery updates

**Possible Causes & Solutions**:

1. **Not subscribed to characteristics**
   ```dart
   // Subscribe BEFORE handshake
   await UniversalBle.subscribeNotifications(deviceId, serviceUuid, asyncCharUuid);
   await UniversalBle.subscribeIndications(deviceId, serviceUuid, syncTxCharUuid);
   ```

2. **No message listener**
   ```dart
   // Set up listener before subscribing
   UniversalBle.onValueChanged = (deviceId, charUuid, value) {
     processMessage(value);
   };
   ```

3. **Zwift Click v2 stopped sending events**
   - Known issue with Click v2
   - Send reset command:
   ```dart
   await sendCommand(Uint8List.fromList([0x22]));  // RESET opcode
   ```

---

### Handshake Response Not Received

**Symptoms**:
- Handshake sent but no response
- Connection times out

**Possible Causes & Solutions**:

1. **Subscribed AFTER handshake**
   ```dart
   // WRONG ORDER:
   await sendHandshake();
   await subscribe();  // Too late!
   
   // CORRECT ORDER:
   await subscribe();
   await sendHandshake();
   ```

2. **Listening to wrong characteristic**
   ```dart
   // Handshake response comes on Sync TX characteristic
   if (charUuid == syncTxCharUuid) {
     handleHandshakeResponse(value);
   }
   ```

3. **Device expecting encrypted handshake**
   - Most devices support unencrypted mode
   - Public key exchange happens after basic handshake

---

### Protobuf Parsing Errors

**Symptoms**:
```
FormatException: Invalid protobuf message
```

**Possible Causes & Solutions**:

1. **Wrong message type**
   ```dart
   try {
     final status = RideKeyPadStatus.fromBuffer(bytes);
   } catch (e) {
     print('Not a RideKeyPadStatus message: $e');
     // Try different message type
   }
   ```

2. **Corrupted data**
   ```dart
   // Log raw bytes to investigate
   print('Raw bytes: ${bytes.map((b) => b.toRadixString(16)).join(' ')}');
   ```

3. **Missing opcode byte**
   ```dart
   // For modern devices, skip opcode before parsing
   final opcode = bytes[0];
   final payload = bytes.sublist(1);
   final message = RideKeyPadStatus.fromBuffer(payload);
   ```

---

## Button Event Issues

### Duplicate Button Events

**Symptoms**:
- Same button press received multiple times
- Actions triggered repeatedly

**Solution - Implement Deduplication**:
```dart
List<ControllerButton>? _lastButtons;

void handleButtonPress(List<ControllerButton> buttons) {
  if (_lastButtons == null || !listEquals(_lastButtons, buttons)) {
    processButtons(buttons);
    _lastButtons = buttons;
  }
}
```

---

### Button Presses Not Detected

**Symptoms**:
- Physical button press doesn't trigger event
- Some buttons work, others don't

**Possible Causes & Solutions**:

1. **Button map logic inverted (Zwift Ride)**
   ```dart
   // Button pressed = bit is 0 (not 1)
   if (status.buttonMap & buttonMask == 0) {  // Note: == 0
     buttonPressed();
   }
   ```

2. **Analog paddle threshold too high**
   ```dart
   // Lower threshold if paddles not registering
   const int threshold = 25;  // Try 10 if issues
   
   if (paddle.analogValue.abs() >= threshold) {
     handlePaddle();
   }
   ```

3. **Mode switch not detected (Zwift Play)**
   ```dart
   // Check rightPad state for mode
   if (status.rightPad == PlayButtonStatus.ON) {
     // Action mode
   } else {
     // Navigation mode
   }
   ```

---

### Wrong Button Actions Triggered

**Symptoms**:
- Pressing A button triggers B action
- Navigation buttons do wrong thing

**Solution - Verify Button Mapping**:
```dart
// Check button map values
print('Button map: 0x${status.buttonMap.toRadixString(16)}');
print('Expected: 0x${(~expectedMask).toRadixString(16)}');

// Verify each bit
for (int i = 0; i < 16; i++) {
  final bit = (status.buttonMap >> i) & 1;
  print('Bit $i: $bit ${bit == 0 ? 'PRESSED' : 'released'}');
}
```

---

## Platform-Specific Issues

### Android

**Issue**: Location permission required for scanning

**Solution**:
```xml
<!-- AndroidManifest.xml -->
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" 
                 android:usesPermissionFlags="neverForLocation" />
```

```dart
// Request at runtime
if (Platform.isAndroid) {
  await Permission.location.request();
}
```

**Issue**: App crashes on Android 12+

**Solution**: Update target SDK and permissions:
```xml
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
```

---

### iOS/macOS

**Issue**: Bluetooth usage description missing

**Solution** - Add to Info.plist:
```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Required to connect to Zwift controllers</string>
```

**Issue**: Background connection drops

**Solution** - Enable background modes:
```xml
<key>UIBackgroundModes</key>
<array>
  <string>bluetooth-central</string>
</array>
```

---

### Web

**Issue**: Device not appearing in browser

**Solution**:
- Use HTTPS or localhost
- Click button to trigger scan (user gesture required)
- Manufacturer data may not be available

```javascript
// Web Bluetooth API
const device = await navigator.bluetooth.requestDevice({
  filters: [{
    services: ['00000001-19ca-4651-86e5-fa29dcdd09d1']
  }]
});
```

---

## Emulator Issues

### Emulator Not Appearing in Zwift

**Possible Causes & Solutions**:

1. **Wrong service UUID advertised**
   ```dart
   // Use correct UUID for device type
   Advertisement(
     serviceUUIDs: [UUID.fromString('FC82')],  // Ride
   )
   ```

2. **Missing device information service**
   ```dart
   // Add required BLE services
   await addDeviceInfoService();
   await addBatteryService();
   await addZwiftService();
   ```

3. **Not responding to writes**
   ```dart
   // Always acknowledge writes
   _peripheralManager.characteristicWriteRequested.forEach((event) async {
     await _peripheralManager.respondWriteRequest(event.request);
   });
   ```

---

### Emulator Connection Drops

**Symptoms**:
- Zwift connects then immediately disconnects
- "Device unavailable" error

**Solutions**:

1. **Send keepalive messages**
   ```dart
   Timer.periodic(Duration(seconds: 5), (timer) {
     if (_central != null) {
       sendNoButtonsPressed(_central!);
     }
   });
   ```

2. **Handle all characteristic operations**
   ```dart
   // Respond to reads
   _peripheralManager.characteristicReadRequested.forEach((event) async {
     await _peripheralManager.respondReadRequestWithValue(
       event.request,
       value: defaultValue,
     );
   });
   ```

---

## Performance Issues

### High CPU Usage

**Possible Causes & Solutions**:

1. **Too frequent message processing**
   ```dart
   // Batch updates
   Timer? _updateTimer;
   
   void scheduleUpdate() {
     _updateTimer?.cancel();
     _updateTimer = Timer(Duration(milliseconds: 100), () {
       processUpdates();
     });
   }
   ```

2. **Excessive logging**
   ```dart
   // Use debug flags
   final bool debugMode = kDebugMode;
   
   if (debugMode) {
     print('Debug: ...');
   }
   ```

---

### Battery Drain

**Solutions**:

1. **Reduce BLE scan frequency**
   ```dart
   // Stop scanning after device found
   UniversalBle.onScanResult = (device) async {
     if (isTargetDevice(device)) {
       await UniversalBle.stopScan();
     }
   };
   ```

2. **Disconnect when not in use**
   ```dart
   // Disconnect during inactivity
   if (idleTime > Duration(minutes: 5)) {
     await disconnect();
   }
   ```

---

## Debugging Techniques

### Enable Verbose Logging

```dart
class BLEDebugger {
  static void logAll() {
    UniversalBle.onConnectionChanged = (id, connected) {
      print('[BLE] Connection: $id -> $connected');
    };
    
    UniversalBle.onValueChanged = (id, char, value) {
      final hex = value.map((b) => b.toRadixString(16).padLeft(2, '0')).join(' ');
      print('[BLE] RX $char: $hex');
    };
    
    UniversalBle.onAvailabilityChanged = (state) {
      print('[BLE] State: $state');
    };
  }
}
```

---

### Analyze Message Flow

```dart
class MessageAnalyzer {
  final List<MessageLog> logs = [];
  
  void logMessage(String direction, Uint8List bytes) {
    logs.add(MessageLog(
      timestamp: DateTime.now(),
      direction: direction,
      bytes: bytes,
    ));
  }
  
  void printSummary() {
    print('=== Message Summary ===');
    print('Total messages: ${logs.length}');
    
    final byType = <int, int>{};
    for (final log in logs) {
      final type = log.bytes.isNotEmpty ? log.bytes[0] : 0;
      byType[type] = (byType[type] ?? 0) + 1;
    }
    
    print('By type:');
    byType.forEach((type, count) {
      print('  0x${type.toRadixString(16)}: $count');
    });
  }
}
```

---

### Test with Known-Good Data

```dart
void testMessageParsing() {
  // Test with known message
  final knownMessage = Uint8List.fromList([
    0x37, 0x08, 0x00,  // Click: Plus button pressed
  ]);
  
  try {
    final status = ClickKeyPadStatus.fromBuffer(knownMessage.sublist(1));
    assert(status.buttonPlus == PlayButtonStatus.ON);
    print('✓ Message parsing works');
  } catch (e) {
    print('✗ Message parsing failed: $e');
  }
}
```

---

## Getting Help

If you're still experiencing issues:

1. **Check the logs**:
   - Enable verbose BLE logging
   - Look for error messages
   - Note when the issue occurs

2. **Verify with another app**:
   - Test device with Zwift app
   - Try BikeControl app
   - Use nRF Connect (Android) or LightBlue (iOS)

3. **Collect diagnostic information**:
   - Device model and firmware version
   - Platform (Android/iOS/etc.) and version
   - BLE library version
   - Sample of BLE message logs

4. **Search existing issues**:
   - Check BikeControl GitHub issues
   - Search for similar problems
   - Review closed issues for solutions

5. **Create detailed bug report**:
   - Describe the problem clearly
   - Include steps to reproduce
   - Attach relevant logs
   - Mention what you've tried

---

## Quick Reference Checklist

Before reporting an issue, verify:

- [ ] Bluetooth is powered on
- [ ] Device is charged/has batteries
- [ ] Correct permissions granted
- [ ] Not connected to other apps
- [ ] Using correct service UUID
- [ ] Subscribed before handshake
- [ ] Message listener configured
- [ ] Firmware is up to date
- [ ] BLE logging enabled
- [ ] Tested with known-good device

---

## Additional Resources

- **BikeControl GitHub**: https://github.com/doudar/swiftcontrol
- **Zwift Support**: https://support.zwift.com
- **BLE Debugging Tools**:
  - Android: nRF Connect
  - iOS: LightBlue
  - macOS: Bluetooth Explorer (Xcode)
  
## Next Steps

- Review [Examples](EXAMPLES.md) for working implementations
- Check [Protocol Basics](PROTOCOL_BASICS.md) for BLE fundamentals
- See [API Overview](API_OVERVIEW.md) for architecture details

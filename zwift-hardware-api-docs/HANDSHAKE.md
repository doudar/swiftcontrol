# Handshake Process - Connection Establishment

## Overview

The handshake process establishes communication between your application and a Zwift device. This document details the complete connection sequence.

## Connection Sequence Overview

```
1. SCAN for Zwift devices
2. CONNECT to device
3. DISCOVER services and characteristics
4. SUBSCRIBE to notification/indication characteristics
5. SEND handshake message ("RideOn")
6. RECEIVE handshake response
7. BEGIN normal operation
```

## Step-by-Step Implementation

### Step 1: Scan for Devices

#### Scan by Service UUID

```dart
// Start BLE scan
await UniversalBle.startScan(
  scanFilter: ScanFilter(
    withServices: [
      "00000001-19CA-4651-86E5-FA29DCDD09D1",  // Legacy service
      "0000FC82-0000-1000-8000-00805F9B34FB",  // Ride service
    ],
  ),
);

// Listen for scan results
UniversalBle.onScanResult = (device) {
  if (isZwiftDevice(device)) {
    print('Found Zwift device: ${device.name}');
    devices.add(device);
  }
};
```

#### Validate Zwift Device

```dart
bool isZwiftDevice(BleDevice device) {
  // Check manufacturer ID
  final manufacturerData = device.manufacturerData[0x094A];
  if (manufacturerData == null || manufacturerData.isEmpty) {
    return false;
  }
  
  // Check device type
  final deviceType = manufacturerData[0];
  return [0x02, 0x03, 0x07, 0x08, 0x09, 0x0A, 0x0B].contains(deviceType);
}
```

### Step 2: Connect to Device

```dart
Future<void> connectToDevice(String deviceId) async {
  try {
    // Connect with timeout
    await UniversalBle.connect(deviceId).timeout(
      Duration(seconds: 10),
      onTimeout: () => throw TimeoutException('Connection timeout'),
    );
    
    print('Connected to device');
  } catch (e) {
    print('Connection failed: $e');
    rethrow;
  }
}
```

### Step 3: Discover Services

```dart
Future<void> discoverServices(String deviceId) async {
  // Wait for services to be available
  await Future.delayed(Duration(milliseconds: 500));
  
  // Discover all services
  final services = await UniversalBle.discoverServices(deviceId);
  
  // Find Zwift custom service
  zwiftService = services.firstWhereOrNull(
    (s) => s.uuid.toLowerCase() == 
      "00000001-19ca-4651-86e5-fa29dcdd09d1" ||
      s.uuid.toLowerCase() == 
      "0000fc82-0000-1000-8000-00805f9b34fb",
  );
  
  if (zwiftService == null) {
    throw Exception(
      'Zwift service not found. '
      'Please update firmware via Zwift Companion app.'
    );
  }
  
  print('Found Zwift service: ${zwiftService.uuid}');
}
```

### Step 4: Get Characteristics

```dart
const ASYNC_CHAR_UUID = "00000002-19ca-4651-86e5-fa29dcdd09d1";
const SYNC_RX_CHAR_UUID = "00000003-19ca-4651-86e5-fa29dcdd09d1";
const SYNC_TX_CHAR_UUID = "00000004-19ca-4651-86e5-fa29dcdd09d1";

Future<void> getCharacteristics() async {
  final characteristics = zwiftService!.characteristics;
  
  asyncCharacteristic = characteristics.firstWhereOrNull(
    (c) => c.uuid.toLowerCase() == ASYNC_CHAR_UUID,
  );
  
  syncRxCharacteristic = characteristics.firstWhereOrNull(
    (c) => c.uuid.toLowerCase() == SYNC_RX_CHAR_UUID,
  );
  
  syncTxCharacteristic = characteristics.firstWhereOrNull(
    (c) => c.uuid.toLowerCase() == SYNC_TX_CHAR_UUID,
  );
  
  if (asyncCharacteristic == null || 
      syncRxCharacteristic == null || 
      syncTxCharacteristic == null) {
    throw Exception('Required characteristics not found');
  }
  
  print('All characteristics found');
}
```

### Step 5: Subscribe to Characteristics

**Important**: Subscribe BEFORE sending handshake to avoid missing the response!

```dart
Future<void> subscribeToCharacteristics(String deviceId) async {
  // Subscribe to async notifications (button presses)
  await UniversalBle.subscribeNotifications(
    deviceId,
    zwiftService!.uuid,
    asyncCharacteristic!.uuid,
  );
  print('Subscribed to async notifications');
  
  // Subscribe to sync indications (responses)
  await UniversalBle.subscribeIndications(
    deviceId,
    zwiftService!.uuid,
    syncTxCharacteristic!.uuid,
  );
  print('Subscribed to sync indications');
  
  // Wait for subscriptions to be active
  await Future.delayed(Duration(milliseconds: 100));
}
```

### Step 6: Set Up Message Listener

```dart
void setupMessageListener(String deviceId) {
  UniversalBle.onValueChanged = (String id, String charUuid, Uint8List value) {
    if (id != deviceId) return;
    
    if (charUuid.toLowerCase() == asyncCharacteristic!.uuid.toLowerCase()) {
      handleAsyncNotification(value);
    } else if (charUuid.toLowerCase() == syncTxCharacteristic!.uuid.toLowerCase()) {
      handleSyncIndication(value);
    }
  };
}
```

### Step 7: Send Handshake

The handshake message is the ASCII string "RideOn":

```dart
const RIDE_ON = [0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e];  // "RideOn"

Future<void> performHandshake(String deviceId) async {
  final handshakeData = Uint8List.fromList(RIDE_ON);
  
  await UniversalBle.write(
    deviceId,
    zwiftService!.uuid,
    syncRxCharacteristic!.uuid,
    handshakeData,
    withoutResponse: true,  // Important: use write without response
  );
  
  print('Handshake sent');
}
```

### Step 8: Receive Handshake Response

The device responds differently based on device type:

#### Zwift Click Response

```
[0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e]  // "RideOn" echo
[0x01, 0x03]                          // Response signature
[...32 bytes...]                       // Device public key
```

**Total**: 40 bytes

#### Zwift Play Response

```
[0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e]  // "RideOn" echo
[0x01, 0x04]                          // Response signature  
[...32 bytes...]                       // Device public key
```

**Total**: 40 bytes

#### Zwift Ride/Click v2 Response

```
[0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e]  // "RideOn" echo
// In unencrypted mode, just the echo (6 bytes)
```

**Processing Response**:

```dart
final RESPONSE_START_CLICK = [0x01, 0x03];
final RESPONSE_START_PLAY = [0x01, 0x04];
final RESPONSE_START_CLICK_V2 = [0x02, 0x03];

void handleSyncIndication(Uint8List bytes) {
  if (bytes.startsWith(RIDE_ON)) {
    // This is the handshake response
    processHandshakeResponse(bytes);
  } else {
    // Other sync messages
    processSyncMessage(bytes);
  }
}

void processHandshakeResponse(Uint8List bytes) {
  if (bytes.length < 6) {
    throw Exception('Invalid handshake response');
  }
  
  // Check for "RideOn"
  if (!bytes.sublist(0, 6).equals(RIDE_ON)) {
    throw Exception('Invalid handshake signature');
  }
  
  if (bytes.length >= 8) {
    // Extract response type
    final responseType = bytes.sublist(6, 8);
    
    if (responseType.equals(RESPONSE_START_CLICK)) {
      deviceType = ZwiftDeviceType.click;
    } else if (responseType.equals(RESPONSE_START_PLAY)) {
      deviceType = ZwiftDeviceType.play;
    } else if (responseType.equals(RESPONSE_START_CLICK_V2)) {
      deviceType = ZwiftDeviceType.clickV2;
    }
    
    // Extract public key if present (bytes 8-40)
    if (bytes.length >= 40) {
      final publicKey = bytes.sublist(8, 40);
      print('Device public key: ${hexDump(publicKey)}');
      // Note: BikeControl doesn't use encryption
    }
  } else {
    // Zwift Ride - just echo in unencrypted mode
    deviceType = ZwiftDeviceType.ride;
  }
  
  print('Handshake complete: $deviceType');
  isConnected = true;
}
```

### Step 9: Device-Specific Initialization

#### Zwift Click v2 Only

Click v2 requires an additional initialization command:

```dart
if (deviceType == ZwiftDeviceType.clickV2) {
  await sendCommandBuffer(Uint8List.fromList([0xFF, 0x04, 0x00]));
  print('Click v2 initialization sent');
}
```

#### Zwift Ride - Request Device Info (Optional)

```dart
if (deviceType == ZwiftDeviceType.ride) {
  // Request device information
  final getDeviceInfo = Uint8List.fromList([
    0x08,  // GET opcode
    0x08,  // Length byte
    0x00,  // Data object: PAGE_DEV_INFO
  ]);
  
  await sendCommand(getDeviceInfo);
}
```

## Complete Connection Example

```dart
class ZwiftDeviceConnection {
  String? deviceId;
  BleService? zwiftService;
  BleCharacteristic? asyncCharacteristic;
  BleCharacteristic? syncRxCharacteristic;
  BleCharacteristic? syncTxCharacteristic;
  ZwiftDeviceType? deviceType;
  bool isConnected = false;
  
  Future<void> connect(BleDevice device) async {
    try {
      deviceId = device.deviceId;
      
      // 1. Connect
      await UniversalBle.connect(deviceId!);
      print('✓ Connected');
      
      // 2. Discover services
      await discoverServices(deviceId!);
      print('✓ Services discovered');
      
      // 3. Get characteristics
      await getCharacteristics();
      print('✓ Characteristics found');
      
      // 4. Set up listener
      setupMessageListener(deviceId!);
      print('✓ Listener setup');
      
      // 5. Subscribe to characteristics
      await subscribeToCharacteristics(deviceId!);
      print('✓ Subscriptions active');
      
      // 6. Perform handshake
      await performHandshake(deviceId!);
      print('✓ Handshake sent');
      
      // 7. Wait for handshake response
      await waitForHandshake();
      print('✓ Handshake complete');
      
      // 8. Device-specific initialization
      await deviceSpecificInit();
      print('✓ Device ready');
      
    } catch (e) {
      print('Connection failed: $e');
      await disconnect();
      rethrow;
    }
  }
  
  Future<void> waitForHandshake() async {
    final completer = Completer<void>();
    
    final timer = Timer(Duration(seconds: 5), () {
      if (!completer.isCompleted) {
        completer.completeError(TimeoutException('Handshake timeout'));
      }
    });
    
    // Wait for isConnected to be set by handshake response handler
    while (!isConnected && !completer.isCompleted) {
      await Future.delayed(Duration(milliseconds: 100));
    }
    
    timer.cancel();
    
    if (!isConnected) {
      await completer.future;  // Will throw timeout
    }
  }
  
  Future<void> deviceSpecificInit() async {
    if (deviceType == ZwiftDeviceType.clickV2) {
      await sendCommandBuffer(Uint8List.fromList([0xFF, 0x04, 0x00]));
    }
  }
  
  Future<void> disconnect() async {
    if (deviceId != null) {
      await UniversalBle.disconnect(deviceId!);
      isConnected = false;
    }
  }
}
```

## Connection State Machine

```
┌─────────────┐
│ DISCONNECTED│
└──────┬──────┘
       │ connect()
       ▼
┌─────────────┐
│ CONNECTING  │
└──────┬──────┘
       │ services discovered
       ▼
┌─────────────┐
│ DISCOVERING │
└──────┬──────┘
       │ characteristics found
       ▼
┌─────────────┐
│ SUBSCRIBING │
└──────┬──────┘
       │ subscriptions active
       ▼
┌─────────────┐
│ HANDSHAKING │
└──────┬──────┘
       │ handshake response received
       ▼
┌─────────────┐
│ CONNECTED   │◄────┐
└──────┬──────┘     │
       │            │
       │ messages   │
       └────────────┘
```

## Error Handling

### Service Not Found

```dart
if (zwiftService == null) {
  throw ZwiftException(
    'Zwift service not found',
    recovery: 'Update firmware via Zwift Companion app',
  );
}
```

### Characteristic Not Found

```dart
if (syncRxCharacteristic == null) {
  throw ZwiftException(
    'Sync RX characteristic not found',
    recovery: 'Device may be incompatible or damaged',
  );
}
```

### Subscription Failed

```dart
try {
  await UniversalBle.subscribeNotifications(...);
} catch (e) {
  throw ZwiftException(
    'Failed to subscribe to notifications',
    recovery: 'Try reconnecting the device',
    originalError: e,
  );
}
```

### Handshake Timeout

```dart
final handshakeResponse = await waitForHandshake().timeout(
  Duration(seconds: 5),
  onTimeout: () => throw ZwiftException(
    'Handshake timeout',
    recovery: 'Ensure device is powered on and in range',
  ),
);
```

## Platform-Specific Considerations

### Android

```dart
// Request permissions before scanning
await Permission.bluetoothScan.request();
await Permission.bluetoothConnect.request();

// Location permission needed for scanning (Android < 12)
if (androidVersion < 31) {
  await Permission.location.request();
}
```

### iOS/macOS

```dart
// Check Bluetooth state
if (BluetoothState != BluetoothLowEnergyState.poweredOn) {
  throw Exception('Bluetooth is not powered on');
}

// Ensure background modes are configured if needed
```

### Web

```dart
// User gesture required for Web Bluetooth
button.onClick.listen((_) async {
  final device = await navigator.bluetooth.requestDevice({
    'filters': [
      {'services': ['00000001-19ca-4651-86e5-fa29dcdd09d1']}
    ]
  });
});
```

## Reconnection Strategy

```dart
class ReconnectionManager {
  int maxAttempts = 3;
  Duration initialDelay = Duration(seconds: 1);
  
  Future<void> reconnect(ZwiftDevice device) async {
    int attempt = 0;
    Duration delay = initialDelay;
    
    while (attempt < maxAttempts) {
      try {
        print('Reconnection attempt ${attempt + 1}/$maxAttempts');
        
        await device.connect();
        print('Reconnected successfully');
        return;
        
      } catch (e) {
        attempt++;
        
        if (attempt >= maxAttempts) {
          throw Exception('Reconnection failed after $maxAttempts attempts');
        }
        
        print('Attempt failed, retrying in ${delay.inSeconds}s...');
        await Future.delayed(delay);
        
        // Exponential backoff
        delay *= 2;
      }
    }
  }
}
```

## Testing Handshake

### Manual Testing

```dart
void testHandshake() async {
  print('=== Testing Handshake ===');
  
  // 1. Connect
  print('1. Connecting...');
  await connect();
  
  // 2. Send handshake
  print('2. Sending handshake...');
  await sendHandshake();
  
  // 3. Wait for response
  print('3. Waiting for response...');
  final response = await waitForResponse(timeout: Duration(seconds: 5));
  
  // 4. Validate
  print('4. Validating response...');
  if (response.startsWith(RIDE_ON)) {
    print('✓ Valid handshake response');
    print('  Response: ${hexDump(response)}');
  } else {
    print('✗ Invalid response');
  }
}
```

### Debugging Tips

```dart
// Log all BLE operations
void enableDebugLogging() {
  UniversalBle.onConnectionChanged = (deviceId, isConnected) {
    print('[BLE] Connection: $deviceId -> $isConnected');
  };
  
  UniversalBle.onValueChanged = (deviceId, charUuid, value) {
    print('[BLE] RX $charUuid: ${hexDump(value)}');
  };
}

// Hex dump utility
String hexDump(Uint8List bytes) {
  return bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join(' ');
}
```

## Best Practices

1. **Always subscribe before handshake** - Ensures you receive the response
2. **Use timeouts** - BLE operations can hang indefinitely
3. **Handle disconnections gracefully** - BLE is inherently unstable
4. **Implement reconnection logic** - Devices disconnect frequently
5. **Log extensively in debug mode** - Essential for troubleshooting
6. **Check firmware versions** - Notify users of updates
7. **Validate responses** - Don't assume devices behave correctly

## Common Issues

### Issue: Handshake Response Not Received

**Cause**: Subscribed after sending handshake

**Solution**: Always subscribe before sending handshake

### Issue: Service Not Found

**Cause**: Outdated firmware

**Solution**: Prompt user to update via Zwift Companion app

### Issue: Connection Drops Immediately

**Cause**: Already connected to another app (e.g., Zwift itself)

**Solution**: Ensure device is disconnected from other apps

### Issue: Characteristics Empty

**Cause**: Services not fully discovered

**Solution**: Add delay after discovering services

```dart
await Future.delayed(Duration(milliseconds: 500));
```

## Next Steps

- Review [Message Types](MESSAGE_TYPES.md) for handling incoming messages
- See [Protocol Buffers](PROTOBUF.md) for parsing message payloads
- Check [Examples](EXAMPLES.md) for complete working implementations

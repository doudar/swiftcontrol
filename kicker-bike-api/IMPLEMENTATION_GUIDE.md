# Implementation Guide for FTMS Trainers

## Overview

This guide provides a step-by-step walkthrough for adding KICKR BIKE protocol support to your existing FTMS trainer. Follow these steps in order to achieve a working implementation.

## Prerequisites

Before starting, ensure you have:

- ✅ Working FTMS trainer with BLE support
- ✅ FTMS characteristics implemented (Indoor Bike Data, Simulation Parameters, etc.)
- ✅ Network connectivity (WiFi/Ethernet)
- ✅ Development environment with:
  - TCP/IP socket support
  - mDNS/Bonjour library
  - Protocol Buffers library
  - BLE GATT knowledge

## Phase 1: mDNS Service Advertisement

### Step 1.1: Install mDNS Library

Choose based on your platform:

**Linux/Embedded:**
```bash
# Avahi
sudo apt-get install avahi-daemon avahi-utils

# Or use a library like mdns-js, dns-sd
```

**iOS/macOS:**
```swift
// Built-in Bonjour support
import Foundation
```

**Android:**
```kotlin
// Built-in NSD support
import android.net.nsd.NsdManager
```

**Dart/Flutter:**
```yaml
dependencies:
  nsd: ^2.0.0
```

### Step 1.2: Create Service Advertisement

```dart
// Example using nsd package
import 'package:nsd/nsd.dart';
import 'dart:io';

class KickrBikeAdvertiser {
  Registration? _registration;
  
  Future<void> start() async {
    // Get local IP
    final ip = await _getLocalIP();
    
    // Register service
    _registration = await register(
      Service(
        name: 'KICKR BIKE PRO ${getSerialNumber()}',
        type: '_wahoo-fitness-tnp._tcp',
        port: 36867,
        addresses: [ip],
        txt: {
          'ble-service-uuids': Uint8List.fromList('FC82'.codeUnits),
          'mac-address': Uint8List.fromList(getMacAddress().codeUnits),
          'serial-number': Uint8List.fromList(getSerialNumber().codeUnits),
        },
      ),
    );
    
    print('KICKR BIKE service advertised');
  }
  
  Future<InternetAddress> _getLocalIP() async {
    final interfaces = await NetworkInterface.list();
    for (final interface in interfaces) {
      for (final addr in interface.addresses) {
        if (addr.type == InternetAddressType.IPv4 && !addr.isLoopback) {
          return addr;
        }
      }
    }
    throw Exception('No network interface found');
  }
  
  void stop() {
    if (_registration != null) {
      unregister(_registration!);
    }
  }
}
```

### Step 1.3: Verify Advertisement

```bash
# Test with dns-sd
dns-sd -B _wahoo-fitness-tnp._tcp

# Should see your service listed
```

## Phase 2: TCP Server

### Step 2.1: Create TCP Server

```dart
class KickrBikeTCPServer {
  ServerSocket? _server;
  Socket? _client;
  int _lastMessageId = 0;
  
  Future<void> start() async {
    _server = await ServerSocket.bind(
      InternetAddress.anyIPv6,
      36867,
      shared: true,
      v6Only: false,
    );
    
    print('TCP server listening on port 36867');
    
    _server!.listen(_handleClient);
  }
  
  void _handleClient(Socket socket) {
    _client = socket;
    print('Client connected: ${socket.remoteAddress}:${socket.remotePort}');
    
    socket.listen(
      _handleData,
      onDone: () {
        print('Client disconnected');
        _client = null;
      },
      onError: (error) {
        print('Socket error: $error');
        _client = null;
      },
    );
  }
  
  void _handleData(List<int> data) {
    // Process messages (Phase 3)
  }
  
  void stop() {
    _client?.close();
    _server?.close();
  }
}
```

### Step 2.2: Test TCP Server

```bash
# Test connection
nc localhost 36867

# Or use telnet
telnet localhost 36867
```

## Phase 3: Protocol Handler

### Step 3.1: Message Parser

```dart
class MessageParser {
  static ParsedMessage parse(List<int> data) {
    final buffer = List<int>.from(data);
    
    final version = _takeUint8(buffer);
    final messageId = _takeUint8(buffer);
    final sequence = _takeUint8(buffer);
    final responseCode = _takeUint8(buffer);
    final length = _takeUint16BE(buffer);
    final body = buffer.take(length).toList();
    
    return ParsedMessage(
      version: version,
      messageId: messageId,
      sequence: sequence,
      responseCode: responseCode,
      body: body,
    );
  }
  
  static int _takeUint8(List<int> buffer) {
    final value = buffer[0];
    buffer.removeAt(0);
    return value;
  }
  
  static int _takeUint16BE(List<int> buffer) {
    final value = (buffer[0] << 8) | buffer[1];
    buffer.removeRange(0, 2);
    return value;
  }
}

class ParsedMessage {
  final int version;
  final int messageId;
  final int sequence;
  final int responseCode;
  final List<int> body;
  
  ParsedMessage({
    required this.version,
    required this.messageId,
    required this.sequence,
    required this.responseCode,
    required this.body,
  });
}
```

### Step 3.2: Message Handler

```dart
void _handleData(List<int> data) {
  final msg = MessageParser.parse(data);
  
  switch (msg.messageId) {
    case 0x01:  // DISCOVER_SERVICES
      _handleDiscoverServices(msg);
      break;
    case 0x02:  // DISCOVER_CHARACTERISTICS
      _handleDiscoverCharacteristics(msg);
      break;
    case 0x03:  // READ_CHARACTERISTIC
      _handleReadCharacteristic(msg);
      break;
    case 0x04:  // WRITE_CHARACTERISTIC
      _handleWriteCharacteristic(msg);
      break;
    case 0x05:  // ENABLE_NOTIFICATIONS
      _handleEnableNotifications(msg);
      break;
    default:
      print('Unknown message type: ${msg.messageId}');
  }
}
```

### Step 3.3: Implement Handlers

```dart
void _handleDiscoverServices(ParsedMessage msg) {
  // Respond with FC82 service UUID
  final serviceUuid = _hexToBytes('0000FC8200001000800000805F9B34FB');
  _sendResponse(msg.messageId, msg.sequence, serviceUuid);
}

void _handleDiscoverCharacteristics(ParsedMessage msg) {
  final serviceUuid = msg.body.take(16).toList();
  
  final responseBody = [
    ...serviceUuid,
    // Sync RX (Write)
    ..._hexToBytes('0000000319CA465186E5FA29DCDD09D1'),
    0x02,
    // Async TX (Notify)
    ..._hexToBytes('0000000219CA465186E5FA29DCDD09D1'),
    0x04,
    // Sync TX (Notify)
    ..._hexToBytes('0000000419CA465186E5FA29DCDD09D1'),
    0x04,
  ];
  
  _sendResponse(msg.messageId, msg.sequence, responseBody);
}

void _sendResponse(int messageId, int sequence, List<int> body) {
  final header = [
    0x01,  // Version
    messageId,
    sequence,
    0x00,  // Success
    (body.length >> 8) & 0xFF,
    body.length & 0xFF,
  ];
  
  _client?.add([...header, ...body]);
}
```

## Phase 4: Protobuf Integration

### Step 4.1: Copy Protobuf Definitions

Copy from BikeControl repository:
```
lib/bluetooth/devices/zwift/protocol/zwift.proto
lib/bluetooth/devices/zwift/protocol/zp.proto
lib/bluetooth/devices/zwift/protocol/zp_vendor.proto
```

### Step 4.2: Generate Code

```bash
# For Dart
protoc --dart_out=. zwift.proto zp.proto zp_vendor.proto

# For Python
protoc --python_out=. zwift.proto zp.proto zp_vendor.proto

# For C++
protoc --cpp_out=. zwift.proto zp.proto zp_vendor.proto
```

### Step 4.3: Parse Protobuf Messages

```dart
import 'package:your_project/protocol/zwift.pb.dart';

void _handleControllerNotification(List<int> data) {
  // data[0] is opcode (0x07), rest is protobuf
  final protobufData = data.sublist(1);
  
  final status = RideKeyPadStatus.fromBuffer(protobufData);
  
  // Check buttons
  if (status.buttonMap & 0x01000 == 0) {
    // Right shift up pressed
    handleShiftUp();
  }
  
  // Check paddles
  for (final paddle in status.analogPaddles) {
    if (paddle.analogValue.abs() >= 25) {
      handlePaddle(paddle.location.value, paddle.analogValue);
    }
  }
}
```

## Phase 5: Handshake Implementation

### Step 5.1: Handle RideOn

```dart
void _handleWriteCharacteristic(ParsedMessage msg) {
  final charUuid = msg.body.take(16).toList();
  final data = msg.body.skip(16).toList();
  
  // Acknowledge write
  _sendResponse(msg.messageId, msg.sequence, charUuid);
  
  // Check for RideOn handshake
  if (_isRideOn(data)) {
    print('RideOn handshake received');
    _sendRideOnResponse();
    _scheduleKeepAlive();
  }
}

bool _isRideOn(List<int> data) {
  return data.length == 6 &&
         data[0] == 0x52 &&  // 'R'
         data[1] == 0x69 &&  // 'i'
         data[2] == 0x64 &&  // 'd'
         data[3] == 0x65 &&  // 'e'
         data[4] == 0x4f &&  // 'O'
         data[5] == 0x6e;    // 'n'
}

void _sendRideOnResponse() {
  final response = [
    0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e,  // "RideOn"
    0x01, 0x03,                           // Signature
  ];
  
  _sendNotification(
    _hexToBytes('0000000419CA465186E5FA29DCDD09D1'),  // Sync TX
    response,
  );
}
```

### Step 5.2: Keep-Alive

```dart
Timer? _keepAliveTimer;

void _scheduleKeepAlive() {
  _keepAliveTimer?.cancel();
  _keepAliveTimer = Timer.periodic(Duration(seconds: 5), (_) {
    if (_client != null) {
      _sendKeepAlive();
    }
  });
}

void _sendKeepAlive() {
  final data = _hexToBytes(
    'B70100002041201C00180004001B4F00B701000020798EC5BDEFCBE4563418269E4926FBE1'
  );
  
  _sendNotification(
    _hexToBytes('0000000419CA465186E5FA29DCDD09D1'),  // Sync TX
    data,
  );
}
```

## Phase 6: Gear System

### Step 6.1: Implement Gear Logic

```dart
class GearSystem {
  int currentGear = 12;
  
  static const gearRatios = [
    0.50, 0.55, 0.60, 0.65, 0.70, 0.75, 0.80, 0.85,
    0.90, 0.95, 1.00, 1.05, 1.10, 1.15, 1.20, 1.25,
    1.30, 1.35, 1.40, 1.45, 1.50, 1.55, 1.60, 1.65,
  ];
  
  void shiftUp() {
    if (currentGear < gearRatios.length - 1) {
      currentGear++;
    }
  }
  
  void shiftDown() {
    if (currentGear > 0) {
      currentGear--;
    }
  }
  
  double getRatio() => gearRatios[currentGear];
}
```

### Step 6.2: Map to FTMS

```dart
class FTMSIntegration {
  final GearSystem gearSystem;
  double _baseGrade = 0.0;
  
  FTMSIntegration(this.gearSystem);
  
  void onZwiftGradeChange(double grade) {
    _baseGrade = grade;
    _updateSimulation();
  }
  
  void onGearChange() {
    _updateSimulation();
  }
  
  void _updateSimulation() {
    final ratio = gearSystem.getRatio();
    final effectiveGrade = (_baseGrade * ratio).clamp(-20.0, 20.0);
    
    // Write to FTMS Indoor Bike Simulation Parameters (0x2AD5)
    final data = _encodeSimulationParams(
      windSpeed: 0.0,
      grade: effectiveGrade,
      crr: 0.004,
      cw: 0.51,
    );
    
    writeFTMSCharacteristic('00002AD5-0000-1000-8000-00805F9B34FB', data);
  }
  
  Uint8List _encodeSimulationParams({
    required double windSpeed,
    required double grade,
    required double crr,
    required double cw,
  }) {
    final data = ByteData(7);
    data.setInt16(0, (windSpeed * 1000).round(), Endian.little);
    data.setInt16(2, (grade * 100).round(), Endian.little);
    data.setUint8(4, (crr * 10000).round());
    data.setUint8(5, (cw * 100).round());
    return data.buffer.asUint8List();
  }
}
```

## Phase 7: Button to Gear Mapping

### Step 7.1: Handle Button Events

```dart
void handleButtonPress(int buttonMap) {
  // Right shift up
  if (buttonMap & 0x01000 == 0) {
    gearSystem.shiftUp();
    ftmsIntegration.onGearChange();
    sendButtonNotification(0x01000, pressed: true);
  }
  
  // Right shift down
  if (buttonMap & 0x02000 == 0) {
    gearSystem.shiftUp();
    ftmsIntegration.onGearChange();
    sendButtonNotification(0x02000, pressed: true);
  }
  
  // Left shift up/down
  if (buttonMap & 0x00100 == 0 || buttonMap & 0x00200 == 0) {
    gearSystem.shiftDown();
    ftmsIntegration.onGearChange();
    sendButtonNotification(0x00100, pressed: true);
  }
}
```

### Step 7.2: Send Button Release

```dart
void sendButtonNotification(int buttonMask, {required bool pressed}) {
  final status = RideKeyPadStatus()
    ..buttonMap = pressed ? (~buttonMask) & 0xFFFFFFFF : 0xFFFFFFFF
    ..analogPaddles.clear();
  
  final data = [
    0x07,  // CONTROLLER_NOTIFICATION opcode
    ...status.writeToBuffer(),
  ];
  
  _sendNotification(
    _hexToBytes('0000000219CA465186E5FA29DCDD09D1'),  // Async TX
    data,
  );
  
  // Send release after 200ms
  if (pressed) {
    Timer(Duration(milliseconds: 200), () {
      sendButtonNotification(buttonMask, pressed: false);
    });
  }
}
```

## Phase 8: Testing

### Step 8.1: Unit Tests

```dart
void testGearSystem() {
  final gears = GearSystem();
  assert(gears.currentGear == 12);
  
  gears.shiftUp();
  assert(gears.currentGear == 13);
  
  gears.shiftDown();
  assert(gears.currentGear == 12);
}

void testProtocol() {
  final msg = MessageParser.parse([
    0x01,  // Version
    0x01,  // Message ID
    0x00,  // Sequence
    0x00,  // Response code
    0x00, 0x00,  // Length
  ]);
  
  assert(msg.messageId == 0x01);
}
```

### Step 8.2: Integration Tests

1. **Test mDNS**: Verify service is discoverable from Zwift device
2. **Test TCP**: Connect with Zwift and verify handshake
3. **Test Gears**: Shift gears and verify resistance changes
4. **Test SIM Mode**: Ride in Zwift SIM mode with gear changes
5. **Test ERG Mode**: Ride in Zwift ERG workout with gear changes

## Phase 9: Optimization

### Step 9.1: Performance

- Use efficient protobuf serialization
- Minimize allocations in hot paths
- Cache frequently used values

### Step 9.2: Reliability

- Handle client disconnections gracefully
- Implement reconnection logic
- Add error recovery

### Step 9.3: User Experience

- Add visual gear indicator
- Provide haptic feedback (if available)
- Log events for debugging

## Deployment Checklist

- [ ] mDNS service advertisement working
- [ ] TCP server accepting connections
- [ ] Handshake completing successfully
- [ ] Button events parsed correctly
- [ ] Gear changes applied to FTMS
- [ ] SIM mode working with gears
- [ ] ERG mode working (if applicable)
- [ ] Edge cases handled (min/max gear)
- [ ] Client disconnection handled
- [ ] Keep-alive messages sent
- [ ] Tested with real Zwift connection
- [ ] Performance acceptable
- [ ] User documentation created

## Common Issues

See **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** for solutions to common problems.

## Next Steps

1. Test thoroughly with Zwift
2. Optimize performance
3. Add user documentation
4. Consider adding support for other buttons (navigation, action buttons)
5. Implement battery status reporting (if applicable)

## Support

For questions:
- Review the BikeControl source code
- Check this documentation
- Open an issue in the BikeControl repository

## Reference Implementation

Complete working example: `lib/bluetooth/devices/zwift/ftms_mdns_emulator.dart` in BikeControl repository.

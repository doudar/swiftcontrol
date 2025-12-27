# TCP Protocol - BLE over TCP

## Overview

The Wahoo TNP (Trainer Network Protocol) encapsulates Bluetooth Low Energy (BLE) GATT operations over TCP/IP. This allows Zwift to communicate with the trainer as if it were a BLE device, but over the network instead.

## Connection

### Server Setup

Your trainer must:
1. Listen on TCP port **36867**
2. Accept connections from Zwift (typically from same subnet)
3. Support both IPv4 and IPv6 (optional but recommended)

### Example Server Creation

```dart
// Bind to IPv6 (with v6Only: false for dual-stack)
_tcpServer = await ServerSocket.bind(
  InternetAddress.anyIPv6,
  36867,
  shared: true,
  v6Only: false,
);

// Accept connections
_tcpServer!.listen((Socket socket) {
  print('Client connected: ${socket.remoteAddress.address}:${socket.remotePort}');
  
  // Handle incoming data
  socket.listen((List<int> data) {
    processMessage(data);
  });
});
```

## Message Format

All messages (both client→server and server→client) use the same header structure:

### Message Header (6 bytes)

```
Byte 0: Protocol Version (0x01)
Byte 1: Message ID (0x01-0x06)
Byte 2: Sequence Number (0x00-0xFF, rolling)
Byte 3: Response Code (0x00 = success, 0x01-0x07 = error)
Bytes 4-5: Body Length (16-bit big-endian)
```

### Message Body

```
Bytes 6+: Message-specific payload
```

### Total Message Structure

```
┌──────────┬────────────┬──────────┬───────────┬─────────┬──────────┐
│ Version  │ Message ID │ Sequence │ Response  │ Length  │   Body   │
│ (1 byte) │  (1 byte)  │ (1 byte) │  (1 byte) │(2 bytes)│(N bytes) │
└──────────┴────────────┴──────────┴───────────┴─────────┴──────────┘
```

## Message Types

### Message IDs

```dart
const DC_MESSAGE_DISCOVER_SERVICES = 0x01;
const DC_MESSAGE_DISCOVER_CHARACTERISTICS = 0x02;
const DC_MESSAGE_READ_CHARACTERISTIC = 0x03;
const DC_MESSAGE_WRITE_CHARACTERISTIC = 0x04;
const DC_MESSAGE_ENABLE_CHARACTERISTIC_NOTIFICATIONS = 0x05;
const DC_MESSAGE_CHARACTERISTIC_NOTIFICATION = 0x06;
```

### Response Codes

```dart
const DC_RC_REQUEST_COMPLETED_SUCCESSFULLY = 0x00;
const DC_RC_UNKNOWN_MESSAGE_TYPE = 0x01;
const DC_RC_UNEXPECTED_ERROR = 0x02;
const DC_RC_SERVICE_NOT_FOUND = 0x03;
const DC_RC_CHARACTERISTIC_NOT_FOUND = 0x04;
const DC_RC_CHARACTERISTIC_OPERATION_NOT_SUPPORTED = 0x05;
const DC_RC_CHARACTERISTIC_WRITE_FAILED_INVALID_SIZE = 0x06;
const DC_RC_UNKNOWN_PROTOCOL_VERSION = 0x07;
```

## Protocol Flow

### 1. Service Discovery

**Client → Server (Discover Services)**

```
Header:
  Version: 0x01
  Message ID: 0x01 (DISCOVER_SERVICES)
  Sequence: 0x00
  Response Code: 0x00
  Length: 0x00 0x00 (no body)

Body: (empty)
```

**Server → Client (Services Response)**

```
Header:
  Version: 0x01
  Message ID: 0x01 (DISCOVER_SERVICES)
  Sequence: 0x00 (same as request)
  Response Code: 0x00 (success)
  Length: 0x00 0x10 (16 bytes)

Body:
  16 bytes: Service UUID (Zwift Ride FC82 service)
  
Example body (hex):
  00 00 fc 82 00 00 10 00 80 00 00 80 5f 9b 34 fb
  
This is: 0000FC82-0000-1000-8000-00805F9B34FB
```

### 2. Characteristic Discovery

**Client → Server (Discover Characteristics)**

```
Header:
  Version: 0x01
  Message ID: 0x02 (DISCOVER_CHARACTERISTICS)
  Sequence: 0x01
  Response Code: 0x00
  Length: 0x00 0x10 (16 bytes)

Body:
  16 bytes: Service UUID to discover characteristics for
```

**Server → Client (Characteristics Response)**

```
Header:
  Version: 0x01
  Message ID: 0x02 (DISCOVER_CHARACTERISTICS)
  Sequence: 0x01
  Response Code: 0x00
  Length: 0x00 0x43 (67 bytes)

Body:
  16 bytes: Service UUID (repeated)
  
  For each characteristic:
    16 bytes: Characteristic UUID
    1 byte: Properties (bit mask)
  
  Repeat for 3 characteristics (Sync RX, Async TX, Sync TX)

Properties:
  0x01 = Read
  0x02 = Write
  0x03 = Indicate
  0x04 = Notify
```

Example response body:
```
Service UUID (16 bytes):
  00 00 fc 82 00 00 10 00 80 00 00 80 5f 9b 34 fb

Sync RX Characteristic (17 bytes):
  UUID: 00 00 00 03 19 ca 46 51 86 e5 fa 29 dc dd 09 d1
  Properties: 0x02 (Write)

Async TX Characteristic (17 bytes):
  UUID: 00 00 00 02 19 ca 46 51 86 e5 fa 29 dc dd 09 d1
  Properties: 0x04 (Notify)

Sync TX Characteristic (17 bytes):
  UUID: 00 00 00 04 19 ca 46 51 86 e5 fa 29 dc dd 09 d1
  Properties: 0x04 (Notify)
```

### 3. Enable Notifications

**Client → Server (Enable Notifications)**

```
Header:
  Version: 0x01
  Message ID: 0x05 (ENABLE_CHARACTERISTIC_NOTIFICATIONS)
  Sequence: 0x02
  Response Code: 0x00
  Length: 0x00 0x11 (17 bytes)

Body:
  16 bytes: Characteristic UUID
  1 byte: Enable (0x01) or Disable (0x00)
```

**Server → Client (Acknowledgment)**

```
Header:
  Version: 0x01
  Message ID: 0x05
  Sequence: 0x02 (same as request)
  Response Code: 0x00
  Length: 0x00 0x10 (16 bytes)

Body:
  16 bytes: Characteristic UUID (echo)
```

### 4. Write Characteristic

**Client → Server (Write Request)**

```
Header:
  Version: 0x01
  Message ID: 0x04 (WRITE_CHARACTERISTIC)
  Sequence: 0x03
  Response Code: 0x00
  Length: [16 + data length]

Body:
  16 bytes: Characteristic UUID
  N bytes: Data to write
```

Example - "RideOn" handshake:
```
Body (hex):
  Characteristic UUID (Sync RX):
    00 00 00 03 19 ca 46 51 86 e5 fa 29 dc dd 09 d1
  Data (6 bytes):
    52 69 64 65 4f 6e  (ASCII: "RideOn")
```

**Server → Client (Write Acknowledgment)**

```
Header:
  Version: 0x01
  Message ID: 0x04
  Sequence: 0x03
  Response Code: 0x00
  Length: 0x00 0x10

Body:
  16 bytes: Characteristic UUID (echo)
```

### 5. Characteristic Notification

**Server → Client (Notification)**

```
Header:
  Version: 0x01
  Message ID: 0x06 (CHARACTERISTIC_NOTIFICATION)
  Sequence: [next sequence number]
  Response Code: 0x00
  Length: [16 + notification data length]

Body:
  16 bytes: Characteristic UUID
  N bytes: Notification data
```

Example - "RideOn" response:
```
Body (hex):
  Characteristic UUID (Sync TX):
    00 00 00 04 19 ca 46 51 86 e5 fa 29 dc dd 09 d1
  Data:
    52 69 64 65 4f 6e  ("RideOn")
    01 03              (Response signature)
```

## Parsing Messages

### Reading Header

```dart
final mutable = data.toList();

// Parse header
final msgVersion = mutable.takeUInt8();      // Byte 0
final msgId = mutable.takeUInt8();           // Byte 1
final seqNum = mutable.takeUInt8();          // Byte 2
final respCode = mutable.takeUInt8();        // Byte 3
final length = mutable.takeUInt16BE();       // Bytes 4-5

// Parse body
final body = mutable.takeBytes(length);
```

### Helper Functions

```dart
extension on List<int> {
  int takeUInt8() {
    final value = this[0];
    removeAt(0);
    return value;
  }

  int takeUInt16BE() {
    final value = (this[0] << 8) | this[1];
    removeAt(0);
    removeAt(0);
    return value;
  }

  List<int> takeBytes(int length) {
    final value = sublist(0, length);
    removeRange(0, length);
    return value;
  }
}
```

## Building Responses

### Creating Response Header

```dart
Uint8List buildHeader(int messageId, int seqNum, int responseCode, int bodyLength) {
  return Uint8List.fromList([
    0x01,                        // Protocol version
    messageId,                   // Message ID
    seqNum,                      // Sequence (echo from request)
    responseCode,                // 0x00 = success
    (bodyLength >> 8) & 0xFF,    // Length high byte
    bodyLength & 0xFF,           // Length low byte
  ]);
}
```

### Sending Response

```dart
void sendResponse(Socket socket, int msgId, int seqNum, List<int> body) {
  final header = buildHeader(
    msgId,
    seqNum,
    DC_RC_REQUEST_COMPLETED_SUCCESSFULLY,
    body.length,
  );
  
  final response = [...header, ...body];
  socket.add(response);
}
```

## UUID Conversion

### UUID to Bytes

UUIDs are sent as 16 raw bytes (not ASCII string):

```dart
List<int> uuidToBytes(String uuid) {
  // Remove dashes: "0000FC82-0000-1000-8000-00805F9B34FB" → "0000FC8200001000800000805F9B34FB"
  final hex = uuid.replaceAll('-', '');
  
  final bytes = <int>[];
  for (var i = 0; i < hex.length; i += 2) {
    final byte = hex.substring(i, i + 2);
    bytes.add(int.parse(byte, radix: 16));
  }
  return bytes;
}
```

### Bytes to UUID

```dart
String bytesToUuid(List<int> bytes) {
  final hex = bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join('');
  return '${hex.substring(0, 8)}-${hex.substring(8, 12)}-${hex.substring(12, 16)}-${hex.substring(16, 20)}-${hex.substring(20)}';
}
```

## Sequence Numbers

- Start at 0 for client messages
- Increment for each new message
- Roll over at 256 (0xFF → 0x00)
- Server echoes client sequence in responses
- Server uses own sequence for notifications

```dart
var lastMessageId = 0;

// For notifications
final seqNum = (lastMessageId + 1) % 256;
lastMessageId = seqNum;
```

## Error Handling

### Invalid Message

If you receive an invalid message:

```dart
void sendError(Socket socket, int seqNum, int errorCode) {
  final header = buildHeader(
    0x01,  // Or original message ID
    seqNum,
    errorCode,
    0,  // No body
  );
  socket.add(header);
}
```

Common errors:
- `0x01` - Unknown message type
- `0x03` - Service not found
- `0x04` - Characteristic not found

## Implementation Checklist

- [ ] Create TCP server on port 36867
- [ ] Parse message headers (6 bytes)
- [ ] Handle DISCOVER_SERVICES (0x01)
- [ ] Handle DISCOVER_CHARACTERISTICS (0x02)
- [ ] Handle ENABLE_CHARACTERISTIC_NOTIFICATIONS (0x05)
- [ ] Handle WRITE_CHARACTERISTIC (0x04)
- [ ] Send CHARACTERISTIC_NOTIFICATION (0x06)
- [ ] Implement UUID conversion helpers
- [ ] Manage sequence numbers
- [ ] Handle client disconnection

## Reference Code

See BikeControl implementation:
- File: `lib/bluetooth/devices/zwift/ftms_mdns_emulator.dart`
- Lines: 103-316 (TCP server and message handling)

## Next Steps

After implementing the TCP protocol, proceed to **[BLE_GATT.md](BLE_GATT.md)** to learn about simulating BLE GATT services and characteristics.

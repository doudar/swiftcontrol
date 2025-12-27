# BLE GATT Simulation

## Overview

This document describes how to simulate Bluetooth Low Energy (BLE) GATT (Generic Attribute Profile) services and characteristics over the TCP protocol. Your trainer needs to respond to discovery requests as if it were a BLE device.

## Service Structure

The KICKR BIKE emulation exposes one primary BLE service with three characteristics:

```
Service: Zwift Ride Service
  UUID: 0000FC82-0000-1000-8000-00805F9B34FB
  Type: Primary Service
  
  Characteristics:
    1. Sync RX (Write)
       UUID: 00000003-19CA-4651-86E5-FA29DCDD09D1
       Properties: Write
       
    2. Async TX (Notify)
       UUID: 00000002-19CA-4651-86E5-FA29DCDD09D1
       Properties: Notify
       
    3. Sync TX (Notify/Indicate)
       UUID: 00000004-19CA-4651-86E5-FA29DCDD09D1
       Properties: Notify
```

## Service Discovery

When Zwift sends a `DISCOVER_SERVICES` message (0x01), respond with the Zwift Ride service UUID.

### Request Format

```
Message ID: 0x01 (DISCOVER_SERVICES)
Body: (empty)
```

### Response Format

```
Message ID: 0x01
Body: 16-byte service UUID
```

### Implementation

```dart
case DC_MESSAGE_DISCOVER_SERVICES:
  // Zwift Ride service UUID in hex (no dashes)
  const serviceUuidHex = '0000FC8200001000800000805F9B34FB';
  final body = hexToBytes(serviceUuidHex);
  
  final header = buildHeader(
    DC_MESSAGE_DISCOVER_SERVICES,
    seqNum,
    DC_RC_REQUEST_COMPLETED_SUCCESSFULLY,
    body.length,
  );
  
  socket.add([...header, ...body]);
  break;
```

### Expected Output

```
Response (hex):
01 01 00 00 00 10 00 00 fc 82 00 00 10 00 80 00 00 80 5f 9b 34 fb

Breakdown:
  01 - Protocol version
  01 - DISCOVER_SERVICES message ID
  00 - Sequence number
  00 - Success response code
  00 10 - Length: 16 bytes
  00 00 fc 82 00 00 10 00 80 00 00 80 5f 9b 34 fb - Service UUID
```

## Characteristic Discovery

When Zwift sends a `DISCOVER_CHARACTERISTICS` message (0x02), respond with all three characteristics for the service.

### Request Format

```
Message ID: 0x02 (DISCOVER_CHARACTERISTICS)
Body: 16-byte service UUID
```

### Response Format

```
Message ID: 0x02
Body:
  16 bytes: Service UUID (echo)
  [For each characteristic:]
    16 bytes: Characteristic UUID
    1 byte: Properties bit mask
```

### Characteristic Properties

```dart
const PROP_READ = 0x01;
const PROP_WRITE = 0x02;
const PROP_INDICATE = 0x03;
const PROP_NOTIFY = 0x04;
```

### Implementation

```dart
case DC_MESSAGE_DISCOVER_CHARACTERISTICS:
  final rawUUID = body.takeBytes(16);
  final serviceUUID = bytesToUuid(rawUUID);
  
  if (serviceUUID == '0000FC82-0000-1000-8000-00805F9B34FB') {
    final responseBody = [
      ...rawUUID,  // Echo service UUID
      
      // Sync RX characteristic (Write)
      ...hexToBytes('0000000319CA465186E5FA29DCDD09D1'),
      0x02,  // Write property
      
      // Async TX characteristic (Notify)
      ...hexToBytes('0000000219CA465186E5FA29DCDD09D1'),
      0x04,  // Notify property
      
      // Sync TX characteristic (Notify)
      ...hexToBytes('0000000419CA465186E5FA29DCDD09D1'),
      0x04,  // Notify property
    ];
    
    final header = buildHeader(
      DC_MESSAGE_DISCOVER_CHARACTERISTICS,
      seqNum,
      DC_RC_REQUEST_COMPLETED_SUCCESSFULLY,
      responseBody.length,
    );
    
    socket.add([...header, ...responseBody]);
  }
  break;
```

### Expected Output

```
Response (hex):
01 02 01 00 00 43 [service UUID] [char1 UUID] 02 [char2 UUID] 04 [char3 UUID] 04

Total body length: 67 bytes (0x43)
  = 16 (service) + 17 (char1) + 17 (char2) + 17 (char3)
```

## Read Characteristic

Handle `READ_CHARACTERISTIC` (0x03) if requested. For KICKR BIKE, reads are rarely used.

### Request Format

```
Message ID: 0x03
Body: 16-byte characteristic UUID
```

### Response Format

```
Message ID: 0x03
Body: 16-byte characteristic UUID + characteristic value
```

### Implementation

```dart
case DC_MESSAGE_READ_CHARACTERISTIC:
  final rawUUID = body.takeBytes(16);
  
  // For KICKR BIKE, we typically just echo the UUID with no data
  final responseBody = rawUUID;
  
  final header = buildHeader(
    DC_MESSAGE_READ_CHARACTERISTIC,
    seqNum,
    DC_RC_REQUEST_COMPLETED_SUCCESSFULLY,
    responseBody.length,
  );
  
  socket.add([...header, ...responseBody]);
  break;
```

## Write Characteristic

Handle `WRITE_CHARACTERISTIC` (0x04) - this is how Zwift sends commands.

### Request Format

```
Message ID: 0x04
Body:
  16 bytes: Characteristic UUID
  N bytes: Data to write
```

### Response Format

```
Message ID: 0x04
Body: 16-byte characteristic UUID (acknowledgment)
```

### Implementation

```dart
case DC_MESSAGE_WRITE_CHARACTERISTIC:
  final rawUUID = body.takeBytes(16);
  final characteristicUUID = bytesToUuid(rawUUID);
  final characteristicData = body.takeBytes(body.length);
  
  print('Write to $characteristicUUID: ${bytesToHex(characteristicData)}');
  
  // Send acknowledgment
  final ackHeader = buildHeader(
    DC_MESSAGE_WRITE_CHARACTERISTIC,
    seqNum,
    DC_RC_REQUEST_COMPLETED_SUCCESSFULLY,
    rawUUID.length,
  );
  socket.add([...ackHeader, ...rawUUID]);
  
  // Process the write (see ZWIFT_RIDE_PROTOCOL.md)
  final response = handleZwiftRideWrite(characteristicUUID, characteristicData);
  
  if (response != null) {
    // Send notification with response
    sendNotification(socket, characteristicUUID, response);
  }
  break;
```

## Enable Characteristic Notifications

Handle `ENABLE_CHARACTERISTIC_NOTIFICATIONS` (0x05) to enable or disable notifications.

### Request Format

```
Message ID: 0x05
Body:
  16 bytes: Characteristic UUID
  1 byte: Enable (0x01) or Disable (0x00)
```

### Response Format

```
Message ID: 0x05
Body: 16-byte characteristic UUID (acknowledgment)
```

### Implementation

```dart
case DC_MESSAGE_ENABLE_CHARACTERISTIC_NOTIFICATIONS:
  final rawUUID = body.takeBytes(16);
  final characteristicUUID = bytesToUuid(rawUUID);
  final enabled = body.takeUInt8();
  
  print('Notifications ${enabled == 1 ? "enabled" : "disabled"} for $characteristicUUID');
  
  // Track which characteristics have notifications enabled
  if (enabled == 1) {
    notificationsEnabled.add(characteristicUUID);
  } else {
    notificationsEnabled.remove(characteristicUUID);
  }
  
  // Send acknowledgment
  final header = buildHeader(
    DC_MESSAGE_ENABLE_CHARACTERISTIC_NOTIFICATIONS,
    seqNum,
    DC_RC_REQUEST_COMPLETED_SUCCESSFULLY,
    rawUUID.length,
  );
  
  socket.add([...header, ...rawUUID]);
  break;
```

## Sending Notifications

Your server sends notifications to inform Zwift of events (button presses, battery level, etc.).

### Notification Format

```
Message ID: 0x06 (CHARACTERISTIC_NOTIFICATION)
Body:
  16 bytes: Characteristic UUID
  N bytes: Notification data
```

### Implementation

```dart
void sendNotification(Socket socket, String characteristicUuid, List<int> data) {
  // Get next sequence number
  final seqNum = (lastMessageId + 1) % 256;
  lastMessageId = seqNum;
  
  // Build notification body
  final responseBody = [
    ...uuidToBytes(characteristicUuid),
    ...data,
  ];
  
  // Build message
  final header = buildHeader(
    DC_MESSAGE_CHARACTERISTIC_NOTIFICATION,
    seqNum,
    DC_RC_REQUEST_COMPLETED_SUCCESSFULLY,
    responseBody.length,
  );
  
  final message = [...header, ...responseBody];
  socket.add(message);
}
```

### Example - Send Button Press

```dart
// Button press notification on Async TX characteristic
final buttonData = [
  0x07,  // Opcode: CONTROLLER_NOTIFICATION
  ...buildRideKeyPadStatus(pressedButtons),
];

sendNotification(
  socket,
  '00000002-19CA-4651-86E5-FA29DCDD09D1',  // Async TX
  buttonData,
);
```

### Example - Send Battery Level

```dart
// Battery notification on Async TX characteristic
final batteryData = [
  0x19,  // Opcode: BATTERY_NOTIF
  ...buildBatteryNotification(85),  // 85% battery
];

sendNotification(
  socket,
  '00000002-19CA-4651-86E5-FA29DCDD09D1',  // Async TX
  batteryData,
);
```

## Characteristic Roles

### Sync RX (Write)

```
UUID: 00000003-19CA-4651-86E5-FA29DCDD09D1
Direction: Zwift → Trainer
Purpose: Commands from Zwift (handshake, configuration)
```

Typical writes:
- "RideOn" handshake (6 bytes: `52 69 64 65 4f 6e`)
- Configuration commands
- Firmware update requests

### Async TX (Notify)

```
UUID: 00000002-19CA-4651-86E5-FA29DCDD09D1
Direction: Trainer → Zwift
Purpose: Asynchronous events (button presses, battery)
```

Typical notifications:
- Button press events (opcode 0x07)
- Battery level updates (opcode 0x19)
- Log messages (opcode 0x31)

### Sync TX (Notify)

```
UUID: 00000004-19CA-4651-86E5-FA29DCDD09D1
Direction: Trainer → Zwift
Purpose: Synchronous responses to writes
```

Typical notifications:
- "RideOn" handshake response
- Command acknowledgments
- Data object responses

## Complete Flow Example

### Handshake Sequence

```
1. Zwift → WRITE to Sync RX
   Data: "RideOn" (52 69 64 65 4f 6e)
   
2. Trainer → ACK the write
   Response: Echo Sync RX UUID
   
3. Trainer → NOTIFY on Sync TX
   Data: "RideOn" + signature (52 69 64 65 4f 6e 01 03)
```

### Button Press Sequence

```
1. User presses shift up button on controller

2. Trainer → NOTIFY on Async TX
   Data: [0x07, ...RideKeyPadStatus protobuf...]
   
3. Zwift processes button press
```

## Error Handling

### Service Not Found

```dart
if (serviceUUID != '0000FC82-0000-1000-8000-00805F9B34FB') {
  final header = buildHeader(
    DC_MESSAGE_DISCOVER_CHARACTERISTICS,
    seqNum,
    DC_RC_SERVICE_NOT_FOUND,
    0,  // No body
  );
  socket.add(header);
}
```

### Characteristic Not Found

```dart
if (!isValidCharacteristic(characteristicUUID)) {
  final header = buildHeader(
    msgId,
    seqNum,
    DC_RC_CHARACTERISTIC_NOT_FOUND,
    0,
  );
  socket.add(header);
}
```

## Implementation Checklist

- [ ] Respond to service discovery with FC82 service
- [ ] Return three characteristics on discovery
- [ ] Set correct properties (Write=0x02, Notify=0x04)
- [ ] Handle write characteristic requests
- [ ] Acknowledge writes immediately
- [ ] Enable/disable notifications as requested
- [ ] Send notifications with correct sequence numbers
- [ ] Use correct characteristic UUIDs for notifications
- [ ] Handle errors gracefully

## Testing

### Verify Service Discovery

```
Expected: Service UUID 0000FC82-0000-1000-8000-00805F9B34FB
```

### Verify Characteristics

```
Expected:
  Sync RX:   00000003-19CA-4651-86E5-FA29DCDD09D1 (Write)
  Async TX:  00000002-19CA-4651-86E5-FA29DCDD09D1 (Notify)
  Sync TX:   00000004-19CA-4651-86E5-FA29DCDD09D1 (Notify)
```

### Verify Handshake

```
Write "RideOn" → Should get "RideOn" + signature back
```

## Reference Code

See BikeControl implementation:
- File: `lib/bluetooth/devices/zwift/ftms_mdns_emulator.dart`
- Lines: 165-296 (Message handling)

## Next Steps

After implementing BLE GATT simulation, proceed to **[ZWIFT_RIDE_PROTOCOL.md](ZWIFT_RIDE_PROTOCOL.md)** to learn about the Zwift Ride controller messages and button events.

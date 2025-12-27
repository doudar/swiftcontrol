# Troubleshooting Guide

## Overview

This guide helps you diagnose and fix common issues when implementing the KICKR BIKE protocol for FTMS trainers.

## mDNS / Service Discovery Issues

### Issue: Zwift Cannot Find Trainer

**Symptoms:**
- Trainer not appearing in Zwift's device list
- mDNS service not visible with dns-sd

**Diagnosis:**

```bash
# Check if service is advertising
dns-sd -B _wahoo-fitness-tnp._tcp

# Should output something like:
# Browsing for _wahoo-fitness-tnp._tcp
# DATE: ---Tue 27 Dec 2024---
# 16:52:36.796  ...STARTING...
# 16:52:36.897  Add   3  6 local.  _wahoo-fitness-tnp._tcp.  KICKR BIKE PRO 1337
```

**Solutions:**

1. **Check firewall**:
   ```bash
   # Allow mDNS (port 5353 UDP)
   sudo ufw allow 5353/udp
   
   # Allow TCP port 36867
   sudo ufw allow 36867/tcp
   ```

2. **Verify network**:
   - Ensure trainer and Zwift are on the same subnet
   - Check WiFi isolation is disabled
   - Try wired connection if possible

3. **Check service configuration**:
   ```dart
   // Verify all TXT records are present
   txt: {
     'ble-service-uuids': Uint8List.fromList('FC82'.codeUnits),  // Required
     'mac-address': Uint8List.fromList('50-50-25-6C-66-9C'.codeUnits),  // Required
     'serial-number': Uint8List.fromList('244700181'.codeUnits),  // Required
   }
   ```

4. **Restart mDNS daemon** (Linux):
   ```bash
   sudo systemctl restart avahi-daemon
   ```

5. **Check service name uniqueness**:
   - Must be unique on network
   - Try changing serial number if duplicate exists

### Issue: Service Advertised but Zwift Doesn't Connect

**Symptoms:**
- Service visible with dns-sd
- Zwift shows trainer but connection fails

**Solutions:**

1. **Verify port 36867 is open**:
   ```bash
   # Check if server is listening
   netstat -an | grep 36867
   
   # Should show: tcp  0  0  :::36867  :::*  LISTEN
   ```

2. **Test TCP connection**:
   ```bash
   # Try connecting manually
   telnet [trainer-ip] 36867
   
   # Or with nc
   nc [trainer-ip] 36867
   ```

3. **Check IP address in mDNS**:
   ```dart
   // Ensure you're advertising the correct IP
   final ip = await getLocalIP();
   print('Advertising on IP: $ip');
   ```

## TCP Protocol Issues

### Issue: Connection Established but Immediately Closes

**Symptoms:**
- Client connects
- Connection closes within seconds
- No data exchanged

**Diagnosis:**

Add logging to your TCP server:

```dart
socket.listen(
  (data) {
    print('Received: ${bytesToHex(data)}');
    _handleData(data);
  },
  onDone: () {
    print('Client disconnected');
  },
  onError: (error) {
    print('Socket error: $error');
  },
);
```

**Solutions:**

1. **Check message parsing**:
   - Ensure you're not consuming extra bytes
   - Verify message length calculation is correct

2. **Send proper responses**:
   - Every request must have a response
   - Response sequence number must match request

3. **Handle errors gracefully**:
   ```dart
   try {
     _handleMessage(msg);
   } catch (e) {
     print('Error handling message: $e');
     // Don't close socket on error
   }
   ```

### Issue: Invalid Message Format

**Symptoms:**
- Parsing errors
- Unexpected message lengths
- Malformed responses

**Diagnosis:**

Log raw bytes:

```dart
void _handleData(List<int> data) {
  print('Raw data (${data.length} bytes): ${bytesToHex(data)}');
  
  try {
    final msg = MessageParser.parse(data);
    print('Parsed: ID=${msg.messageId}, Seq=${msg.sequence}, Len=${msg.body.length}');
  } catch (e) {
    print('Parse error: $e');
    print('First 10 bytes: ${data.take(10).map((b) => b.toRadixString(16)).join(' ')}');
  }
}
```

**Solutions:**

1. **Verify header format**:
   ```
   [Version] [MsgID] [Seq] [RespCode] [Length-HI] [Length-LO] [Body...]
   ```

2. **Check endianness**:
   ```dart
   // Length is big-endian (high byte first)
   final length = (bytes[4] << 8) | bytes[5];
   ```

3. **Validate body length**:
   ```dart
   if (buffer.length < length) {
     print('Incomplete message: expected $length bytes, got ${buffer.length}');
     // Wait for more data or handle error
   }
   ```

## BLE GATT Simulation Issues

### Issue: Service Not Found

**Symptoms:**
- Zwift sends DISCOVER_SERVICES
- Returns "Service Not Found" error

**Solutions:**

1. **Check UUID format**:
   ```dart
   // Correct: 16 bytes, no dashes
   final uuid = hexToBytes('0000FC8200001000800000805F9B34FB');
   
   // Length must be exactly 16
   assert(uuid.length == 16);
   ```

2. **Verify response structure**:
   ```dart
   void _handleDiscoverServices(ParsedMessage msg) {
     final body = hexToBytes('0000FC8200001000800000805F9B34FB');
     
     _sendResponse(
       msg.messageId,  // Must match request
       msg.sequence,   // Must match request
       body,           // 16 bytes
     );
   }
   ```

### Issue: Characteristic Discovery Fails

**Symptoms:**
- Service discovery works
- Characteristic discovery fails or returns wrong data

**Solutions:**

1. **Check characteristic UUIDs**:
   ```dart
   // Must include all 3 characteristics
   final chars = [
     ...serviceUuid,  // Echo service UUID first
     // Sync RX
     ...hexToBytes('0000000319CA465186E5FA29DCDD09D1'),
     0x02,  // Write property
     // Async TX
     ...hexToBytes('0000000219CA465186E5FA29DCDD09D1'),
     0x04,  // Notify property
     // Sync TX
     ...hexToBytes('0000000419CA465186E5FA29DCDD09D1'),
     0x04,  // Notify property
   ];
   
   // Total: 16 (service) + 17*3 (chars) = 67 bytes
   assert(chars.length == 67);
   ```

2. **Verify properties**:
   ```dart
   const PROP_WRITE = 0x02;
   const PROP_NOTIFY = 0x04;
   
   // Not 0x01, 0x03 (those are different)
   ```

## Handshake Issues

### Issue: RideOn Handshake Fails

**Symptoms:**
- Write characteristic succeeds
- No response from trainer
- Zwift times out

**Diagnosis:**

Log write requests:

```dart
void _handleWriteCharacteristic(ParsedMessage msg) {
  final uuid = msg.body.take(16).toList();
  final data = msg.body.skip(16).toList();
  
  print('Write to ${bytesToUuid(uuid)}');
  print('Data: ${bytesToHex(data)} (${String.fromCharCodes(data)})');
}
```

**Solutions:**

1. **Check for RideOn**:
   ```dart
   // Must match exactly: "RideOn"
   final isRideOn = data.length == 6 &&
                    data[0] == 0x52 &&  // 'R'
                    data[1] == 0x69 &&  // 'i'
                    data[2] == 0x64 &&  // 'd'
                    data[3] == 0x65 &&  // 'e'
                    data[4] == 0x4f &&  // 'O'
                    data[5] == 0x6e;    // 'n'
   ```

2. **Send correct response**:
   ```dart
   void _sendRideOnResponse() {
     final response = [
       0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e,  // "RideOn"
       0x01, 0x03,                           // Signature (important!)
     ];
     
     _sendNotification(
       hexToBytes('0000000419CA465186E5FA29DCDD09D1'),  // Sync TX UUID
       response,
     );
   }
   ```

3. **Use correct characteristic**:
   - Response must be on **Sync TX** (0x0004...)
   - Not Async TX or Sync RX

4. **Start keep-alive**:
   ```dart
   void _sendRideOnResponse() {
     // ... send response
     
     // Start keep-alive after handshake
     Timer(Duration(seconds: 5), _sendKeepAlive);
   }
   ```

## Protobuf Issues

### Issue: Cannot Parse RideKeyPadStatus

**Symptoms:**
- Protobuf parsing throws exception
- Invalid message errors

**Solutions:**

1. **Check opcode**:
   ```dart
   // First byte is opcode, rest is protobuf
   final opcode = data[0];
   final protobufData = data.sublist(1);
   
   if (opcode == 0x07) {  // CONTROLLER_NOTIFICATION
     final status = RideKeyPadStatus.fromBuffer(protobufData);
   }
   ```

2. **Verify protobuf library**:
   ```dart
   // Make sure you have protobuf package
   import 'package:protobuf/protobuf.dart';
   
   // And generated files
   import 'package:your_project/protocol/zwift.pb.dart';
   ```

3. **Re-generate protobuf files**:
   ```bash
   protoc --dart_out=. zwift.proto
   ```

### Issue: Button Map Not Detecting Presses

**Symptoms:**
- Receiving button notifications
- All buttons appear released

**Solutions:**

1. **Check button logic** (inverted!):
   ```dart
   // WRONG: Checking if bit is set
   if (buttonMap & mask != 0) { }
   
   // CORRECT: Checking if bit is clear (pressed = 0)
   if (buttonMap & mask == 0) { }
   
   // OR using BikeControl pattern:
   if (buttonMap & mask == PlayButtonStatus.ON.value) { }
   ```

2. **Log button map**:
   ```dart
   print('Button map: 0x${buttonMap.toRadixString(16)}');
   print('Checking mask 0x${mask.toRadixString(16)}');
   print('Result: ${(buttonMap & mask)}');
   ```

## Gear/Incline Issues

### Issue: Gears Not Changing Resistance

**Symptoms:**
- Button presses detected
- Gear changes logged
- No resistance change in Zwift

**Solutions:**

1. **Verify FTMS update**:
   ```dart
   void applyGearChange() {
     print('Applying gear ${gearSystem.currentGear}, ratio ${gearSystem.getRatio()}');
     
     final effectiveGrade = baseGrade * gearSystem.getRatio();
     print('Base grade: $baseGrade%, Effective: $effectiveGrade%');
     
     updateFTMSSimulation(grade: effectiveGrade);
   }
   ```

2. **Check FTMS characteristic**:
   ```dart
   // Indoor Bike Simulation Parameters UUID
   const simParamsUUID = '00002AD5-0000-1000-8000-00805F9B34FB';
   
   // Verify it's being written
   void updateFTMSSimulation({required double grade}) {
     final data = _encode(grade);
     print('Writing to FTMS: ${bytesToHex(data)}');
     writeBLECharacteristic(simParamsUUID, data);
   }
   ```

3. **Verify Zwift mode**:
   - SIM mode: Gears should affect grade
   - ERG mode: Gears affect feel, not power
   - Resistance mode: May not work in Zwift

### Issue: Resistance Too High/Low

**Symptoms:**
- Gears work but scale is wrong
- Too easy or too hard

**Solutions:**

1. **Adjust gear ratios**:
   ```dart
   // Make smaller steps
   static const gearRatios = [
     0.90, 0.92, 0.94, 0.96, 0.98, 1.00,
     1.02, 1.04, 1.06, 1.08, 1.10,
   ];
   ```

2. **Scale the multiplier**:
   ```dart
   // Reduce impact of gears
   final effectiveGrade = baseGrade + (ratio - 1.0) * 2.0;
   // Instead of: baseGrade * ratio
   ```

3. **Clamp values**:
   ```dart
   final effectiveGrade = (baseGrade * ratio).clamp(-20.0, 20.0);
   ```

## Performance Issues

### Issue: High CPU Usage

**Solutions:**

1. **Optimize message parsing**:
   ```dart
   // Cache frequently used values
   final _serviceUuidBytes = hexToBytes('0000FC8200001000800000805F9B34FB');
   
   // Reuse buffers
   final _messageBuffer = ByteData(1024);
   ```

2. **Reduce logging**:
   ```dart
   // Only log in debug mode
   if (kDebugMode) {
     print('Message: $msg');
   }
   ```

3. **Batch updates**:
   ```dart
   // Don't update FTMS on every button press
   Timer? _updateTimer;
   
   void scheduleUpdate() {
     _updateTimer?.cancel();
     _updateTimer = Timer(Duration(milliseconds: 100), () {
       updateFTMSSimulation();
     });
   }
   ```

### Issue: Latency in Gear Changes

**Solutions:**

1. **Process messages immediately**:
   ```dart
   socket.listen((data) {
     // Don't await async operations here
     _handleDataSync(data);
   });
   ```

2. **Optimize protobuf**:
   ```dart
   // Parse once, cache result
   final status = RideKeyPadStatus.fromBuffer(data);
   _cachedStatus = status;
   ```

## Debugging Tools

### Enable Verbose Logging

```dart
void _handleData(List<int> data) {
  if (kDebugMode) {
    print('=== Received Data ===');
    print('Length: ${data.length}');
    print('Hex: ${bytesToHex(data)}');
    print('ASCII: ${String.fromCharCodes(data.where((b) => b >= 32 && b < 127))}');
  }
  
  // Process...
}
```

### Packet Capture

Use Wireshark to capture traffic:

```bash
# Capture on port 36867
sudo tcpdump -i any port 36867 -w kickr-bike.pcap

# Then analyze with Wireshark
wireshark kickr-bike.pcap
```

### Test with Mock Client

Create a simple test client:

```dart
void testConnection() async {
  final socket = await Socket.connect('localhost', 36867);
  
  // Test service discovery
  final discoverMsg = [
    0x01,  // Version
    0x01,  // DISCOVER_SERVICES
    0x00,  // Sequence
    0x00,  // Response code
    0x00, 0x00,  // Length: 0
  ];
  
  socket.add(discoverMsg);
  
  socket.listen((response) {
    print('Response: ${bytesToHex(response)}');
  });
}
```

## Common Error Messages

### "Service Not Found" (0x03)

- Check service UUID matches `0000FC82...`
- Verify 16-byte UUID format

### "Characteristic Not Found" (0x04)

- Check characteristic UUIDs
- Ensure all 3 characteristics are returned
- Verify properties are correct

### "Operation Not Supported" (0x05)

- Check characteristic properties
- Write characteristic must have Write property (0x02)
- Notification characteristics must have Notify property (0x04)

### "Invalid Size" (0x06)

- Check message body length
- Verify protobuf encoding is correct

## Getting Help

If you're still having issues:

1. **Check BikeControl source code**:
   - `lib/bluetooth/devices/zwift/ftms_mdns_emulator.dart`
   - `lib/bluetooth/devices/zwift/zwift_ride.dart`

2. **Enable debug logging**:
   - Set `kDebugMode = true`
   - Log all messages and responses

3. **Compare with reference implementation**:
   - Run BikeControl's emulator
   - Compare message flow

4. **Test incrementally**:
   - Start with mDNS only
   - Add TCP server
   - Add protocol handling
   - Add handshake
   - Add button parsing
   - Add gear integration

5. **Document your findings**:
   - Note what works
   - Note what doesn't
   - Share logs when asking for help

## Verification Checklist

- [ ] mDNS service is discoverable (`dns-sd -B`)
- [ ] TCP server accepts connections (`nc localhost 36867`)
- [ ] Service discovery returns FC82 service
- [ ] Characteristic discovery returns 3 characteristics
- [ ] Write acknowledgments sent immediately
- [ ] RideOn handshake completes successfully
- [ ] Keep-alive messages sent every 5 seconds
- [ ] Button presses parsed correctly
- [ ] Gear changes detected
- [ ] FTMS simulation parameters updated
- [ ] Resistance changes in Zwift

## Reference Logs

Expected message flow:

```
1. Client connects
2. DISCOVER_SERVICES → FC82 service
3. DISCOVER_CHARACTERISTICS → 3 characteristics
4. ENABLE_NOTIFICATIONS (Async TX)
5. ENABLE_NOTIFICATIONS (Sync TX)
6. WRITE "RideOn" → NOTIFY "RideOn" + signature
7. Keep-alive every 5s
8. Button press → CONTROLLER_NOTIFICATION
9. Gear change → Update FTMS
```

## Additional Resources

- BikeControl source code: https://github.com/doudar/swiftcontrol
- FTMS specification: https://www.bluetooth.com/specifications/specs/fitness-machine-service-1-0/
- Protobuf documentation: https://developers.google.com/protocol-buffers

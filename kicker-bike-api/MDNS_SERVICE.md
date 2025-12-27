# mDNS Service Advertisement

## Overview

The KICKR BIKE protocol uses mDNS (Multicast DNS, also known as Bonjour on Apple platforms) to advertise the trainer's availability on the local network. Zwift discovers the trainer automatically without requiring manual IP configuration.

## Service Type

The service must be advertised with the following type:

```
_wahoo-fitness-tnp._tcp
```

Where:
- `wahoo-fitness-tnp` is the service name (TNP = Trainer Network Protocol)
- `_tcp` indicates it's a TCP-based service

## Service Configuration

### Service Name

```
KICKR BIKE PRO [serial]
```

Example: `KICKR BIKE PRO 1337`

You can use any serial number or identifier. The BikeControl implementation uses `1337` as a placeholder.

### Port

```
36867
```

The TCP server must listen on this specific port. Zwift expects KICKR BIKE devices to use this port.

### TXT Records

The service advertisement must include three TXT records:

#### 1. ble-service-uuids

```
Key: ble-service-uuids
Value: FC82
```

This indicates the BLE service UUID (short form) that the device exposes. The full UUID is `0000FC82-0000-1000-8000-00805F9B34FB` (Zwift Ride service).

The value should be ASCII-encoded bytes of the string "FC82".

#### 2. mac-address

```
Key: mac-address
Value: [MAC address in format XX-XX-XX-XX-XX-XX]
```

Example: `50-50-25-6C-66-9C`

This should be the MAC address of your device. Use the actual MAC address of your network interface or generate a stable identifier.

The value should be ASCII-encoded bytes of the MAC address string with dashes.

#### 3. serial-number

```
Key: serial-number
Value: [Serial number as string]
```

Example: `244700181`

This is the device serial number. Use your trainer's actual serial number or generate a unique identifier.

The value should be ASCII-encoded bytes of the serial number string.

## Example mDNS Advertisement

### Using nsd (Dart/Flutter)

```dart
import 'package:nsd/nsd.dart';

final registration = await register(
  Service(
    name: 'KICKR BIKE PRO 1337',
    type: '_wahoo-fitness-tnp._tcp',
    port: 36867,
    addresses: [localIP],  // Your local IP address
    txt: {
      'ble-service-uuids': Uint8List.fromList('FC82'.codeUnits),
      'mac-address': Uint8List.fromList('50-50-25-6C-66-9C'.codeUnits),
      'serial-number': Uint8List.fromList('244700181'.codeUnits),
    },
  ),
);
```

### Using Avahi (Linux)

Create a service file in `/etc/avahi/services/kickr-bike.service`:

```xml
<?xml version="1.0" standalone='no'?>
<!DOCTYPE service-group SYSTEM "avahi-service.dtd">
<service-group>
  <name>KICKR BIKE PRO 1337</name>
  <service>
    <type>_wahoo-fitness-tnp._tcp</type>
    <port>36867</port>
    <txt-record>ble-service-uuids=FC82</txt-record>
    <txt-record>mac-address=50-50-25-6C-66-9C</txt-record>
    <txt-record>serial-number=244700181</txt-record>
  </service>
</service-group>
```

### Using dns-sd (macOS/Linux)

```bash
dns-sd -R "KICKR BIKE PRO 1337" _wahoo-fitness-tnp._tcp . 36867 \
  ble-service-uuids=FC82 \
  mac-address=50-50-25-6C-66-9C \
  serial-number=244700181
```

### Using Bonjour (macOS/iOS - Objective-C)

```objc
NSNetService *service = [[NSNetService alloc] 
    initWithDomain:@"local." 
    type:@"_wahoo-fitness-tnp._tcp." 
    name:@"KICKR BIKE PRO 1337" 
    port:36867];

NSDictionary *txtDict = @{
    @"ble-service-uuids": [@"FC82" dataUsingEncoding:NSUTF8StringEncoding],
    @"mac-address": [@"50-50-25-6C-66-9C" dataUsingEncoding:NSUTF8StringEncoding],
    @"serial-number": [@"244700181" dataUsingEncoding:NSUTF8StringEncoding]
};

NSData *txtData = [NSNetService dataFromTXTRecordDictionary:txtDict];
[service setTXTRecordData:txtData];
[service publish];
```

## Network Interface Selection

### Getting Local IP

The service should advertise on the primary network interface. Typically:

- **Wired**: Prefer Ethernet interfaces
- **Wireless**: Use WiFi interface if no wired connection
- **Avoid**: Loopback (127.0.0.1) and link-local addresses

Example code to find the appropriate IP:

```dart
final interfaces = await NetworkInterface.list();
InternetAddress? localIP;

for (final interface in interfaces) {
  for (final addr in interface.addresses) {
    if (addr.type == InternetAddressType.IPv4 && !addr.isLoopback) {
      localIP = addr;
      break;
    }
  }
  if (localIP != null) break;
}
```

## Discovery Testing

### Verify Your Advertisement

#### Using dns-sd

```bash
# Browse for services
dns-sd -B _wahoo-fitness-tnp._tcp

# Resolve a specific service
dns-sd -L "KICKR BIKE PRO 1337" _wahoo-fitness-tnp._tcp
```

Expected output:
```
Lookup KICKR BIKE PRO 1337._wahoo-fitness-tnp._tcp.local
DATE: ---Tue 27 Dec 2024---
16:52:36.796  ...STARTING...
16:52:36.897  KICKR\032BIKE\032PRO\0321337._wahoo-fitness-tnp._tcp.local. can be reached at KICKR-BIKE.local.:36867 (interface 4)
 ble-service-uuids=FC82 mac-address=50-50-25-6C-66-9C serial-number=244700181
```

#### Using avahi-browse (Linux)

```bash
avahi-browse -r _wahoo-fitness-tnp._tcp
```

#### Network Scanner Tools

- **iOS**: Discovery - DNS-SD Browser app
- **Android**: BonjourBrowser app  
- **macOS**: Discovery app or dns-sd command
- **Windows**: Bonjour Browser

## Troubleshooting

### Service Not Appearing

1. **Check firewall** - Ensure UDP port 5353 (mDNS) is open
2. **Verify network** - Both devices must be on same subnet
3. **Check service name** - Must be unique on the network
4. **Restart mDNS** - Some systems cache service advertisements

### Zwift Not Connecting

1. **Verify port 36867** - TCP server must be listening
2. **Check TXT records** - All three are required
3. **Validate IP address** - Must be reachable from Zwift device
4. **Test with dns-sd** - Confirm service is discoverable

### Multiple Network Interfaces

If your device has multiple network interfaces:

1. **Advertise on all** - Or just the interface Zwift is using
2. **Match addresses** - TXT MAC should match the advertising interface
3. **Avoid conflicts** - Don't use the same service name twice

## Implementation Checklist

- [ ] Choose unique service name (e.g., "KICKR BIKE PRO [serial]")
- [ ] Set service type to `_wahoo-fitness-tnp._tcp`
- [ ] Configure port 36867
- [ ] Get local IP address (non-loopback, IPv4)
- [ ] Create TXT record: `ble-service-uuids=FC82`
- [ ] Create TXT record: `mac-address=[your MAC]`
- [ ] Create TXT record: `serial-number=[your serial]`
- [ ] Start mDNS advertisement
- [ ] Verify with dns-sd or similar tool
- [ ] Test discovery from Zwift

## Reference Code

See the BikeControl implementation:
- File: `lib/bluetooth/devices/zwift/ftms_mdns_emulator.dart`
- Lines: 44-88 (startServer method)

## Next Steps

Once your mDNS service is advertising, proceed to **[TCP_PROTOCOL.md](TCP_PROTOCOL.md)** to implement the TCP server that handles incoming connections from Zwift.

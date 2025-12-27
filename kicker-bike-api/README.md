# Kicker Bike API Specification

## Purpose

This documentation provides the complete specification for implementing the **Wahoo KICKR BIKE Protocol** (also known as the Wahoo TNP - Trainer Network Protocol) to enable existing FTMS (Fitness Machine Service) trainers to communicate with Zwift using the Zwift Ride controller protocol.

The primary goal is to allow developers to add this service to existing trainers that already support BLE FTMS, enabling them to:
1. Advertise as a KICKR BIKE PRO device over mDNS
2. Accept BLE connections over TCP/IP (Wahoo's TNP protocol)
3. Process Zwift Ride controller commands
4. Handle gear changes and incline adjustments

## Overview

The KICKR BIKE protocol allows Zwift to connect to a smart trainer over the network (via mDNS/TCP) instead of directly via Bluetooth. This approach:
- Enables simultaneous connections from multiple apps
- Works around Bluetooth connection limits
- Provides a bridge between BLE controllers and the trainer app

### Architecture

```
┌─────────────────┐         ┌──────────────────┐         ┌─────────────────┐
│  Zwift Ride     │   BLE   │  Your FTMS       │  mDNS/  │     Zwift       │
│  Controller     │────────▶│  Trainer with    │  TCP    │  Application    │
│  (Handlebar)    │         │  KICKR BIKE API  │◀────────│                 │
└─────────────────┘         └──────────────────┘         └─────────────────┘
                                      │
                                      │ FTMS BLE
                                      ▼
                             (Your existing trainer)
```

### Protocol Stack

1. **mDNS Service Advertisement** - Wahoo-fitness-tnp service
2. **TCP Server** - Port 36867
3. **BLE-over-TCP Protocol** - Custom message framing
4. **BLE GATT Simulation** - Service/characteristic discovery
5. **Zwift Ride Protocol** - Protobuf messages for button events

## Documentation Structure

This specification is organized into the following documents:

1. **[MDNS_SERVICE.md](MDNS_SERVICE.md)** - mDNS service advertisement and discovery
2. **[TCP_PROTOCOL.md](TCP_PROTOCOL.md)** - TCP/IP message framing and protocol
3. **[BLE_GATT.md](BLE_GATT.md)** - GATT service and characteristic simulation
4. **[ZWIFT_RIDE_PROTOCOL.md](ZWIFT_RIDE_PROTOCOL.md)** - Zwift Ride controller messages
5. **[GEAR_AND_INCLINE.md](GEAR_AND_INCLINE.md)** - Gear changes and incline handling (with examples)
6. **[IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md)** - Step-by-step implementation for FTMS trainers
7. **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Common issues and debugging

## Quick Start for FTMS Trainer Developers

If you already have an FTMS trainer and want to add KICKR BIKE compatibility:

1. **Read [MDNS_SERVICE.md](MDNS_SERVICE.md)** - Learn how to advertise your trainer
2. **Implement [TCP_PROTOCOL.md](TCP_PROTOCOL.md)** - Create the TCP server
3. **Simulate [BLE_GATT.md](BLE_GATT.md)** - Handle BLE service discovery over TCP
4. **Parse [ZWIFT_RIDE_PROTOCOL.md](ZWIFT_RIDE_PROTOCOL.md)** - Process button events
5. **Map to FTMS** using [GEAR_AND_INCLINE.md](GEAR_AND_INCLINE.md) - Convert to trainer commands
6. **Follow [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md)** - Complete integration

## Key Components

### Service Advertisement (mDNS)

```
Service Name: KICKR BIKE PRO [serial]
Service Type: _wahoo-fitness-tnp._tcp
Port: 36867
TXT Records:
  - ble-service-uuids: FC82
  - mac-address: [device MAC]
  - serial-number: [device serial]
```

### BLE Service (Simulated over TCP)

```
Service UUID: 0000FC82-0000-1000-8000-00805F9B34FB (Zwift Ride)

Characteristics:
  - Sync RX:  00000003-19CA-4651-86E5-FA29DCDD09D1 (Write)
  - Async TX: 00000002-19CA-4651-86E5-FA29DCDD09D1 (Notify)
  - Sync TX:  00000004-19CA-4651-86E5-FA29DCDD09D1 (Notify)
```

### Message Types

The protocol handles several message types:

- **Discovery** - Service and characteristic discovery
- **Read/Write** - GATT read/write operations
- **Notifications** - Button events and status updates
- **Handshake** - "RideOn" connection establishment

## Protocol Flow

```
1. Zwift discovers trainer via mDNS
2. Zwift connects to TCP port 36867
3. Zwift sends DISCOVER_SERVICES
   → Trainer responds with FC82 service UUID
4. Zwift sends DISCOVER_CHARACTERISTICS
   → Trainer responds with characteristic UUIDs and properties
5. Zwift enables notifications on Async and Sync TX
6. Zwift writes "RideOn" handshake to Sync RX
   → Trainer responds with "RideOn" acknowledgment
7. Zwift writes commands (gear changes, etc.)
   → Trainer sends button notifications
   → Trainer updates FTMS parameters
```

## Gear Shift Example

When a user presses the shift up button on the Zwift Ride controller:

```
1. Button Press → RideKeyPadStatus protobuf message
   buttonMap: 0xFFFEFFF (all bits set except SHFT_UP_R_BTN at 0x01000)

2. Your Trainer → Detect shift up command

3. Update Gear → Calculate new gear ratio
   Previous: Gear 12 (1:1 ratio)
   New: Gear 13 (1.08:1 ratio, ~8% harder)

4. Adjust Simulation → Update incline if in SIM mode
   Previous: 2% grade
   New: 2.8% grade (or increase resistance if in ERG mode)

5. Send FTMS → Update Indoor Bike Data characteristic
   - New power target (if ERG)
   - New simulation parameters (if SIM)
```

Detailed examples are in [GEAR_AND_INCLINE.md](GEAR_AND_INCLINE.md).

## Prerequisites

To implement this protocol, you should have:

- **Existing FTMS Implementation** - Your trainer already supports BLE FTMS
- **Network Capabilities** - TCP/IP server and mDNS support
- **Protobuf Library** - For parsing Zwift Ride messages
- **Understanding of BLE GATT** - To simulate services over TCP

## Technical Requirements

- **mDNS/Bonjour** - For service advertisement
- **TCP Server** - Listen on port 36867
- **Protocol Buffers** - Version 3+ (for Zwift Ride messages)
- **BLE FTMS** - Already implemented in your trainer

## Protobuf Definitions

The Zwift Ride protocol uses Protocol Buffers. Key message types:

```protobuf
message RideKeyPadStatus {
  uint32 buttonMap = 1;                              // Button state mask
  repeated RideAnalogKeyPress analogPaddles = 3;     // Paddle positions
}

message BatteryNotification {
  uint32 newPercLevel = 1;  // 0-100
}

message StatusResponse {
  uint32 command = 1;
  uint32 status = 2;
}
```

Full protobuf definitions are in the reference implementation at:
`lib/bluetooth/devices/zwift/protocol/`

## Reference Implementation

This specification is based on the BikeControl implementation:
- **Source**: [doudar/swiftcontrol](https://github.com/doudar/swiftcontrol)
- **Key File**: `lib/bluetooth/devices/zwift/ftms_mdns_emulator.dart`
- **Protocol Buffers**: `lib/bluetooth/devices/zwift/protocol/zp.pb.dart`

## Security Considerations

- The protocol operates over local network only
- No authentication is implemented in the basic protocol
- Encryption is optional and not fully documented
- Consider firewall rules to restrict access to port 36867

## Compliance and Disclaimers

This is an **unofficial** implementation guide based on reverse engineering. 

- **Wahoo** owns all rights to the KICKR BIKE and TNP protocol
- **Zwift** owns all rights to the Zwift Ride protocol
- This documentation is for educational and interoperability purposes only

Use this specification responsibly and in compliance with:
- Wahoo's and Zwift's Terms of Service
- Local laws and regulations
- Bluetooth SIG specifications
- Network protocol standards

## Support and Contributing

For questions or issues:
- Review the [TROUBLESHOOTING.md](TROUBLESHOOTING.md) guide
- Check the BikeControl repository issues
- Refer to the source code implementation

Corrections and improvements to this documentation are welcome.

## Version History

- **v1.0** (2024) - Initial specification
  - mDNS service advertisement
  - BLE-over-TCP protocol
  - Zwift Ride controller support
  - Gear and incline examples

## Next Steps

Ready to implement? Start with **[MDNS_SERVICE.md](MDNS_SERVICE.md)** to learn about service advertisement!

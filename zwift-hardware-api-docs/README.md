# Zwift Hardware API - Unofficial Implementation Guide

> **Note**: This is an unofficial implementation guide created by reverse-engineering Zwift hardware devices. This documentation is based on the BikeControl (formerly SwiftControl) implementation.

## Overview

This documentation provides a comprehensive guide to implementing the Zwift Hardware API, which enables communication with Zwift controllers such as:

- **Zwift Click** (BC1) - Single button controller
- **Zwift Click v2** - Enhanced button controller  
- **Zwift Play** (RC1) - Dual controller with navigation and action buttons
- **Zwift Ride** - Full handlebar controller with shifters and navigation

## Purpose

This guide is designed to help developers:

1. Understand the Bluetooth Low Energy (BLE) protocol used by Zwift devices
2. Implement communication with Zwift hardware
3. Build emulators or compatible devices
4. Create applications that integrate with Zwift controllers

## Documentation Structure

This documentation is organized into the following sections:

### Core Documentation

1. **[API Overview](API_OVERVIEW.md)** - Architecture and design patterns
2. **[Protocol Basics](PROTOCOL_BASICS.md)** - BLE services, characteristics, and communication
3. **[Device Types](DEVICE_TYPES.md)** - Detailed information about each Zwift device
4. **[Message Types](MESSAGE_TYPES.md)** - Data structures and message formats
5. **[Handshake Process](HANDSHAKE.md)** - Connection establishment and initialization

### Implementation Guides

6. **[Button Mapping](BUTTON_MAPPING.md)** - Button definitions and action mappings
7. **[Protocol Buffers](PROTOBUF.md)** - Protobuf schemas and usage
8. **[Emulator Guide](EMULATOR.md)** - Building a Zwift device emulator
9. **[Code Examples](EXAMPLES.md)** - Practical implementation examples

### Additional Resources

10. **[Troubleshooting](TROUBLESHOOTING.md)** - Common issues and solutions

## Quick Start

To get started implementing the Zwift Hardware API:

1. **Read the [API Overview](API_OVERVIEW.md)** to understand the overall architecture
2. **Study [Protocol Basics](PROTOCOL_BASICS.md)** to learn about BLE communication
3. **Choose your device** from [Device Types](DEVICE_TYPES.md)
4. **Implement the [Handshake](HANDSHAKE.md)** process
5. **Parse messages** using [Message Types](MESSAGE_TYPES.md) and [Protocol Buffers](PROTOBUF.md)
6. **Test your implementation** with [Code Examples](EXAMPLES.md)

## Prerequisites

Before implementing the Zwift Hardware API, you should have:

- **BLE Knowledge**: Understanding of Bluetooth Low Energy concepts
- **Programming Skills**: Familiarity with your chosen language (Dart examples provided)
- **Protocol Buffers**: Basic understanding of protobuf serialization
- **Hardware**: Access to Zwift devices for testing (or use the emulator)

## Technical Requirements

- **Bluetooth 4.0+** (BLE support)
- **Protocol Buffer library** for your platform
- **Async/await support** for handling BLE callbacks
- **Byte manipulation** capabilities

## Key Concepts

### BLE Services and Characteristics

Zwift devices expose custom BLE services with specific UUIDs:
- **Service UUID**: `00000001-19CA-4651-86E5-FA29DCDD09D1` (older devices)
- **Service UUID**: `0000FC82-0000-1000-8000-00805F9B34FB` (Zwift Ride)

### Communication Pattern

1. **Discovery**: Scan for Zwift devices using manufacturer ID `2378` (0x094A)
2. **Connection**: Connect to the device and discover services
3. **Handshake**: Exchange "RideOn" message to establish communication
4. **Subscription**: Subscribe to notification characteristics
5. **Message Processing**: Parse incoming messages and handle button events

### Message Flow

```
App → Device: RideOn handshake
Device → App: RideOn acknowledgment + public key
Device → App: Battery level notifications
Device → App: Button press notifications
App → Device: Vibration commands (optional)
```

## Security Considerations

- Some devices support encryption (not fully documented here)
- Public key exchange happens during handshake
- Most BikeControl implementations use unencrypted mode for simplicity

## Contributing

This documentation is based on reverse engineering and may contain inaccuracies. Contributions, corrections, and improvements are welcome.

## Disclaimer

This is an **unofficial** implementation guide. Zwift, Inc. owns all rights to their hardware and protocols. This documentation is provided for educational purposes only.

Use this information responsibly and in compliance with:
- Zwift's Terms of Service
- Local laws and regulations
- Bluetooth SIG specifications

## Credits

This documentation is derived from the [BikeControl](https://github.com/doudar/swiftcontrol) project, an open-source application for controlling trainer apps using Zwift and other hardware.

## License

This documentation follows the same license as the BikeControl project. Please refer to the main repository's LICENSE file.

## Support

For questions or issues:
- Review the [Troubleshooting](TROUBLESHOOTING.md) guide
- Check the BikeControl repository issues
- Refer to the source code in the `lib/bluetooth/devices/zwift/` directory

## Version History

- **v1.0** (2024) - Initial documentation based on BikeControl implementation
  - Support for Zwift Click, Click v2, Play, and Ride
  - Protocol buffer definitions
  - Emulator implementation

## Next Steps

Ready to start? Head to the **[API Overview](API_OVERVIEW.md)** to begin your implementation journey!

/*
 * Copyright (C) 2020  Anthony Doud & Joel Baranick
 * All rights reserved
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "BLE_KickrBikeService.h"
#include "DirConManager.h"
#include "Main.h"
#include <Constants.h>
#include <vector>

// Gear ratio table: 24 gears from easiest (0.50) to hardest (1.65)
// These ratios are multiplied with the base gradient to simulate gear changes
const double BLE_KickrBikeService::gearRatios[KICKR_BIKE_NUM_GEARS] = {
    0.50, 0.55, 0.60, 0.65, 0.70, 0.75, 0.80, 0.85,  // Gears 1-8 (easy)
    0.90, 0.95, 1.00, 1.05, 1.10, 1.15, 1.20, 1.25,  // Gears 9-16 (medium)
    1.30, 1.35, 1.40, 1.45, 1.50, 1.55, 1.60, 1.65   // Gears 17-24 (hard)
};

BLE_KickrBikeService::BLE_KickrBikeService()
    : pKickrBikeService(nullptr),
      syncRxCharacteristic(nullptr),
      asyncTxCharacteristic(nullptr),
      syncTxCharacteristic(nullptr),
      currentGear(KICKR_BIKE_DEFAULT_GEAR),
      lastShifterPosition(-1),
      baseGradient(0.0),
      effectiveGradient(0.0),
      targetPower(0),
      isHandshakeComplete(false),
      isEnabled(false),
      lastKeepAliveTime(0),
      lastGradientUpdateTime(0) {}

void BLE_KickrBikeService::setupService(NimBLEServer *pServer, MyCharacteristicCallbacks *chrCallbacks) {
  // Create the Zwift Ride service (KICKR BIKE protocol)
  pKickrBikeService = spinBLEServer.pServer->createService(ZWIFT_RIDE_SERVICE_UUID);
  
  // Create the three characteristics according to KICKR BIKE specification:
  // 1. Sync RX - Write characteristic for receiving commands from Zwift
  syncRxCharacteristic = pKickrBikeService->createCharacteristic(
      ZWIFT_SYNC_RX_UUID, 
      NIMBLE_PROPERTY::WRITE);
  
  // 2. Async TX - Notify characteristic for asynchronous events (button presses, battery)
  asyncTxCharacteristic = pKickrBikeService->createCharacteristic(
      ZWIFT_ASYNC_TX_UUID, 
      NIMBLE_PROPERTY::NOTIFY);
  
  // 3. Sync TX - Notify characteristic for synchronous responses
  syncTxCharacteristic = pKickrBikeService->createCharacteristic(
      ZWIFT_SYNC_TX_UUID, 
      NIMBLE_PROPERTY::NOTIFY);
  
  // Set custom callback for Sync RX to handle RideOn handshake
  static KickrBikeCharacteristicCallbacks kickrBikeCallbacks;
  syncRxCharacteristic->setCallbacks(&kickrBikeCallbacks);
  
  // Start the service
  pKickrBikeService->start();
  
  // Add service UUID to DirCon MDNS (for discovery)
  DirConManager::addBleServiceUuid(pKickrBikeService->getUUID());
  
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE Service initialized with %d gears", KICKR_BIKE_NUM_GEARS);
}

void BLE_KickrBikeService::update() {
  // Send periodic keep-alive messages if handshake is complete
  if (isHandshakeComplete) {
    unsigned long currentTime = millis();
    // Send keep-alive every 5 seconds
    if (currentTime - lastKeepAliveTime >= 5000) {
      sendKeepAlive();
      lastKeepAliveTime = currentTime;
    }
  }
}

void BLE_KickrBikeService::shiftUp() {
  if (currentGear < KICKR_BIKE_NUM_GEARS - 1) {
    currentGear++;
    applyGearChange();
    SS2K_LOG(BLE_SERVER_LOG_TAG, "Shifted UP to gear %d (ratio: %.2f)", 
             currentGear + 1, getCurrentGearRatio());
  } else {
    SS2K_LOG(BLE_SERVER_LOG_TAG, "Already in highest gear");
  }
}

void BLE_KickrBikeService::shiftDown() {
  if (currentGear > 0) {
    currentGear--;
    applyGearChange();
    SS2K_LOG(BLE_SERVER_LOG_TAG, "Shifted DOWN to gear %d (ratio: %.2f)", 
             currentGear + 1, getCurrentGearRatio());
  } else {
    SS2K_LOG(BLE_SERVER_LOG_TAG, "Already in lowest gear");
  }
}

double BLE_KickrBikeService::getCurrentGearRatio() const {
  if (currentGear >= 0 && currentGear < KICKR_BIKE_NUM_GEARS) {
    return gearRatios[currentGear];
  }
  return 1.0;  // Default to neutral ratio
}

void BLE_KickrBikeService::applyGearChange() {
  // Recalculate effective gradient with new gear
  effectiveGradient = calculateEffectiveGrade(baseGradient, getCurrentGearRatio());
  
  // Apply to trainer if this service is enabled
  if (isEnabled) {
    applyGradientToTrainer();
  }
  
  // Optionally notify clients about gear change via async TX characteristic
  // This could be used to send gear status to connected apps
  uint8_t gearStatus[2] = {
    static_cast<uint8_t>(currentGear + 1),  // 1-indexed gear number
    static_cast<uint8_t>((getCurrentGearRatio() * 100))  // Ratio as percentage
  };
  
  asyncTxCharacteristic->setValue(gearStatus, sizeof(gearStatus));
  asyncTxCharacteristic->notify();
}

void BLE_KickrBikeService::setBaseGradient(double gradientPercent) {
  baseGradient = gradientPercent;
  effectiveGradient = calculateEffectiveGrade(baseGradient, getCurrentGearRatio());
  
  // Apply to trainer if enabled
  if (isEnabled) {
    applyGradientToTrainer();
  }
  
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: Base gradient set to %.2f%%", baseGradient);
}

double BLE_KickrBikeService::getEffectiveGradient() const {
  return effectiveGradient;
}

void BLE_KickrBikeService::applyGradientToTrainer() {
  // Only update if enough time has passed (100ms debounce)
  unsigned long currentTime = millis();
  if (currentTime - lastGradientUpdateTime < 100) {
    return;
  }
  lastGradientUpdateTime = currentTime;
  
  // Clamp to valid trainer limits (-20% to +20%)
  double clampedGradient = effectiveGradient;
  if (clampedGradient < -20.0) clampedGradient = -20.0;
  if (clampedGradient > 20.0) clampedGradient = 20.0;
  
  // Convert to 0.01% units for rtConfig
  int gradientUnits = static_cast<int>(clampedGradient * 100);
  
  // Update the target incline directly
  rtConfig->setTargetIncline(gradientUnits);
  
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: Applied gradient %.2f%% (gear %d, ratio %.2f)", 
           clampedGradient, currentGear + 1, getCurrentGearRatio());
}

void BLE_KickrBikeService::setTargetPower(int watts) {
  targetPower = watts;
  
  // In ERG mode, the power is fixed and gears affect the "feel"
  // This is handled by the trainer's power control logic
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: Target power set to %d watts", targetPower);
}

void BLE_KickrBikeService::updateTrainerPosition() {
  // This method updates the physical trainer position based on effective gradient
  // Only applies if the service is enabled and controlling the trainer
  if (!isEnabled) {
    return;
  }
  
  applyGradientToTrainer();
}

double BLE_KickrBikeService::calculateEffectiveGrade(double baseGrade, double gearRatio) {
  // Calculate effective grade by multiplying base grade with gear ratio
  // This simulates the feeling of shifting gears:
  // - Lower gear (ratio < 1.0) makes hills feel easier
  // - Higher gear (ratio > 1.0) makes hills feel harder
  return baseGrade * gearRatio;
}

void BLE_KickrBikeService::updateGearFromShifterPosition() {
  // Get current shifter position
  int currentShifterPosition = rtConfig->getShifterPosition();
  
  // Check if shifter position has changed
  if (lastShifterPosition == -1) {
    // First run, just store the position
    lastShifterPosition = currentShifterPosition;
    return;
  }
  
  if (currentShifterPosition == lastShifterPosition) {
    // No change, nothing to do
    return;
  }
  
  // Determine direction of shift
  if (currentShifterPosition > lastShifterPosition) {
    // Shifter moved up - shift to harder gear
    shiftUp();
  } else {
    // Shifter moved down - shift to easier gear
    shiftDown();
  }
  
  // Update last position
  lastShifterPosition = currentShifterPosition;
}

bool BLE_KickrBikeService::isRideOnMessage(const std::string& data) {
  // "RideOn" = 0x52 0x69 0x64 0x65 0x4f 0x6e
  return data.length() == 6 &&
         (uint8_t)data[0] == 0x52 &&  // 'R'
         (uint8_t)data[1] == 0x69 &&  // 'i'
         (uint8_t)data[2] == 0x64 &&  // 'd'
         (uint8_t)data[3] == 0x65 &&  // 'e'
         (uint8_t)data[4] == 0x4f &&  // 'O'
         (uint8_t)data[5] == 0x6e;    // 'n'
}

void BLE_KickrBikeService::processWrite(const std::string& value) {
  if (value.empty()) {
    SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: Received empty write");
    return;
  }
  
  // Check if this is the RideOn handshake (no opcode, just raw bytes)
  if (isRideOnMessage(value)) {
    SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: Received RideOn handshake");
    sendRideOnResponse();
    isHandshakeComplete = true;
    lastKeepAliveTime = millis();
    return;
  }
  
  // Process opcode-based messages
  uint8_t opcode = (uint8_t)value[0];
  const uint8_t* messageData = (const uint8_t*)value.data() + 1;
  size_t messageLength = value.length() - 1;
  
  switch (opcode) {
    case 0x08:  // GET - Request data object
      handleGetRequest(messageData, messageLength);
      break;
      
    case 0x22:  // RESET - Reset device
      handleReset();
      break;
      
    case 0x41:  // LOG_LEVEL_SET - Set log level
      handleSetLogLevel(messageData, messageLength);
      break;
      
    case 0x32:  // VENDOR_MESSAGE - Vendor-specific message
      handleVendorMessage(messageData, messageLength);
      break;
      
    case 0x07:  // CONTROLLER_NOTIFICATION - Button events (shouldn't be written to us, we send these)
      SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: Unexpected CONTROLLER_NOTIFICATION write");
      break;
      
    case 0x19:  // BATTERY_NOTIF - Battery updates (shouldn't be written to us, we send these)
      SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: Unexpected BATTERY_NOTIF write");
      break;
      
    default:
      // Log unknown opcodes for debugging
      SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: Received unknown opcode 0x%02X (%d bytes)", 
               opcode, value.length());
      break;
  }
}

void BLE_KickrBikeService::sendRideOnResponse() {
  // Respond with "RideOn" + signature bytes (0x01 0x03)
  uint8_t response[8] = {
    0x52, 0x69, 0x64, 0x65, 0x4f, 0x6e,  // "RideOn"
    0x01, 0x03                            // Signature
  };
  
  syncTxCharacteristic->setValue(response, sizeof(response));
  syncTxCharacteristic->notify();
  
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: Sent RideOn response");
}

void BLE_KickrBikeService::sendKeepAlive() {
  // Keep-alive message to maintain connection with Zwift
  // This is a protobuf-encoded message that tells Zwift we're still alive
  // The exact format comes from the BikeControl reference implementation
  uint8_t keepAliveData[] = {
    0xB7, 0x01, 0x00, 0x00, 0x20, 0x41, 0x20, 0x1C, 
    0x00, 0x18, 0x00, 0x04, 0x00, 0x1B, 0x4F, 0x00, 
    0xB7, 0x01, 0x00, 0x00, 0x20, 0x79, 0x8E, 0xC5, 
    0xBD, 0xEF, 0xCB, 0xE4, 0x56, 0x34, 0x18, 0x26, 
    0x9E, 0x49, 0x26, 0xFB, 0xE1
  };
  
  syncTxCharacteristic->setValue(keepAliveData, sizeof(keepAliveData));
  syncTxCharacteristic->notify();
  
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: Sent keep-alive");
}

// Opcode message handlers

void BLE_KickrBikeService::handleGetRequest(const uint8_t* data, size_t length) {
  // GET request - Zwift is requesting a data object
  // The data should contain an object ID (protobuf encoded)
  
  if (length < 1) {
    SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: GET request with no data");
    sendStatusResponse(0x02);  // Error status
    return;
  }
  
  // For now, we'll parse a simple object ID from the first bytes
  // In a full implementation, this would be protobuf decoded
  uint16_t objectId = 0;
  if (length >= 2) {
    objectId = ((uint16_t)data[1] << 8) | data[0];
  } else {
    objectId = data[0];
  }
  
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: GET request for object ID 0x%04X", objectId);
  
  // Respond with empty data for now (full implementation would return actual object data)
  sendGetResponse(objectId, nullptr, 0);
}

void BLE_KickrBikeService::handleReset() {
  // RESET command - Reset the device to default state
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: RESET command received");
  
  // Reset to default gear
  currentGear = KICKR_BIKE_DEFAULT_GEAR;
  baseGradient = 0.0;
  effectiveGradient = 0.0;
  targetPower = 0;
  
  // Apply reset state to trainer
  if (isEnabled) {
    applyGradientToTrainer();
  }
  
  // Send success status
  sendStatusResponse(0x00);  // Success
}

void BLE_KickrBikeService::handleSetLogLevel(const uint8_t* data, size_t length) {
  // LOG_LEVEL_SET - Set logging level
  if (length < 1) {
    SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: SET_LOG_LEVEL with no data");
    return;
  }
  
  uint8_t logLevel = data[0];
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: SET_LOG_LEVEL to %d", logLevel);
  
  // For now, just acknowledge - full implementation would adjust logging
  sendStatusResponse(0x00);  // Success
}

void BLE_KickrBikeService::handleVendorMessage(const uint8_t* data, size_t length) {
  // VENDOR_MESSAGE - Vendor-specific message
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: VENDOR_MESSAGE received (%d bytes)", length);
  
  // Log the message content for debugging
  if (length > 0) {
    SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: Vendor message first byte: 0x%02X", data[0]);
  }
  
  // Send success status
  sendStatusResponse(0x00);
}

void BLE_KickrBikeService::sendGetResponse(uint16_t objectId, const uint8_t* data, size_t length) {
  // Send GET_RESPONSE (opcode 0x3C) with the requested object data
  std::vector<uint8_t> response;
  response.push_back(0x3C);  // GET_RESPONSE opcode
  
  // Add object ID (little-endian)
  response.push_back(objectId & 0xFF);
  response.push_back((objectId >> 8) & 0xFF);
  
  // Add data if provided
  if (data && length > 0) {
    response.insert(response.end(), data, data + length);
  }
  
  syncTxCharacteristic->setValue(response.data(), response.size());
  syncTxCharacteristic->notify();
  
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: Sent GET_RESPONSE for object 0x%04X", objectId);
}

void BLE_KickrBikeService::sendStatusResponse(uint8_t status) {
  // Send STATUS_RESPONSE (opcode 0x12) with status code
  uint8_t response[2] = {
    0x12,   // STATUS_RESPONSE opcode
    status  // Status code (0x00 = success, others = error)
  };
  
  syncTxCharacteristic->setValue(response, sizeof(response));
  syncTxCharacteristic->notify();
  
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: Sent STATUS_RESPONSE (status: 0x%02X)", status);
}

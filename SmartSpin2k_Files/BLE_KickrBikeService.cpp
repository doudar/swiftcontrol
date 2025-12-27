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
      baseFTMSIncline(0.0) {}

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
  
  // Set callbacks for write operations
  syncRxCharacteristic->setCallbacks(chrCallbacks);
  
  // Start the service
  pKickrBikeService->start();
  
  // Add service UUID to DirCon MDNS (for discovery)
  DirConManager::addBleServiceUuid(pKickrBikeService->getUUID());
  
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE Service initialized with %d gears", KICKR_BIKE_NUM_GEARS);
}

void BLE_KickrBikeService::update() {
  // This method is called periodically to update the service state
  // Currently, the main work is done in updateGearFromShifterPosition()
  // which should be called from the main loop
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
  // Update the FTMS incline based on current gear
  updateFTMSIncline();
  
  // Optionally notify clients about gear change via async TX characteristic
  // This could be used to send gear status to connected apps
  uint8_t gearStatus[2] = {
    static_cast<uint8_t>(currentGear + 1),  // 1-indexed gear number
    static_cast<uint8_t>((getCurrentGearRatio() * 100))  // Ratio as percentage
  };
  
  asyncTxCharacteristic->setValue(gearStatus, sizeof(gearStatus));
  asyncTxCharacteristic->notify();
}

void BLE_KickrBikeService::updateFTMSIncline() {
  // Use the stored base incline (which should be updated when FTMS receives new values)
  // If we haven't received a base incline yet, get current target as fallback
  double baseIncline = baseFTMSIncline;
  
  // Calculate the effective incline based on current gear
  double effectiveGrade = calculateEffectiveGrade(baseIncline, getCurrentGearRatio());
  
  // Clamp to FTMS limits (-20% to +20%)
  if (effectiveGrade < -20.0) effectiveGrade = -20.0;
  if (effectiveGrade > 20.0) effectiveGrade = 20.0;
  
  // Convert to 0.01% units for FTMS
  int effectiveInclineUnits = static_cast<int>(effectiveGrade * 100);
  
  // Update the target incline
  rtConfig->setTargetIncline(effectiveInclineUnits);
  
  SS2K_LOG(BLE_SERVER_LOG_TAG, "KICKR BIKE: base=%.2f%%, gear=%.2f, effective=%.2f%%", 
           baseIncline, getCurrentGearRatio(), effectiveGrade);
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

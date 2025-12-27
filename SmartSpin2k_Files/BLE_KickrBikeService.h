/*
 * Copyright (C) 2020  Anthony Doud & Joel Baranick
 * All rights reserved
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <NimBLEDevice.h>
#include "BLE_Common.h"

// Gear system configuration for virtual shifting
#define KICKR_BIKE_NUM_GEARS 24
#define KICKR_BIKE_DEFAULT_GEAR 11  // Middle gear (0-indexed, so gear 12 in 1-indexed)

class BLE_KickrBikeService {
 public:
  BLE_KickrBikeService();
  void setupService(NimBLEServer *pServer, MyCharacteristicCallbacks *chrCallbacks);
  void update();
  
  // Gear management
  void shiftUp();
  void shiftDown();
  int getCurrentGear() const { return currentGear; }
  double getCurrentGearRatio() const;
  
  // Function to check shifter position and modify incline accordingly
  void updateGearFromShifterPosition();
  
 private:
  BLEService *pKickrBikeService;
  BLECharacteristic *syncRxCharacteristic;   // Write characteristic for commands
  BLECharacteristic *asyncTxCharacteristic;  // Notify characteristic for events
  BLECharacteristic *syncTxCharacteristic;   // Notify characteristic for responses
  
  // Gear system state
  int currentGear;
  int lastShifterPosition;
  double baseFTMSIncline;  // Store the base incline set by FTMS before gear modification
  
  // Gear ratio table (24 gears)
  static const double gearRatios[KICKR_BIKE_NUM_GEARS];
  
  // Helper methods
  void applyGearChange();
  void updateFTMSIncline();
  double calculateEffectiveGrade(double baseGrade, double gearRatio);
  
  // Allow FTMS service to notify us of base incline changes
  friend class BLE_Fitness_Machine_Service;
  void setBaseFTMSIncline(double incline) { baseFTMSIncline = incline; }
};

extern BLE_KickrBikeService kickrBikeService;

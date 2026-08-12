#ifndef _IOTSAESTIMOTE_H_
#define _IOTSAESTIMOTE_H_
#include "iotsa.h"
#include "iotsaApi.h"

// NimBLEDevice.h sets up #define-based compat aliases (BLEDevice, BLEScan,
// BLEAdvertisedDevice, etc. -> their Nim* equivalents). Including the old
// legacy header names directly is ambiguous: depending on the toolchain,
// they can resolve to the ESP32 core's own bundled (and here unwanted)
// classic BLE library instead of NimBLE-Arduino.
#include <NimBLEDevice.h>

#ifdef IOTSA_WITH_API
#define IotsaEstimoteModBaseMod IotsaApiMod
#else
#define IotsaEstimoteModBaseMod IotsaMod
#endif

struct NearableAdvertisement;
struct Estimote {
  uint8_t id[8];
  bool moving;
  int curMoveDuration;
  int curMovePeriod;
  int prevMoveDuration;
  int prevMovePeriod;
  float x, y, z;
  float temp;
  bool voltageStress;
  float voltage;

  bool seen;
};

class IotsaEstimoteMod : public IotsaEstimoteModBaseMod, public NimBLEScanCallbacks {
public:
  IotsaEstimoteMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaEstimoteModBaseMod(_app, _auth, early),
    pBLEScan(NULL),
    nKnownEstimote(0),
    nNewEstimote(0),
    estimotes(NULL)
  {}

  void setup() override;
  void serverSetup() override;
  void loop() override;
  String info() override;
  // BLE scan callbacks
  void onResult(const BLEAdvertisedDevice *advertisedDevice) override;
  void onScanEnd(const NimBLEScanResults& scanResults, int reason) override;
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void configLoad() override;
  void configSave() override;
  void handler();
  void _sensorData(struct NearableAdvertisement *pkt);
  String argument;
  BLEScan* pBLEScan;
  int nKnownEstimote;
  int nNewEstimote;
  struct Estimote *estimotes;
};

#endif

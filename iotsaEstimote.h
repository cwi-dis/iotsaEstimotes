#ifndef _IOTSAESTIMOTE_H_
#define _IOTSAESTIMOTE_H_
#include "iotsa.h"
#include "iotsaApi.h"

// NimBLEDevice.h sets up #define-based compat aliases (BLEDevice, BLEScan,
// BLEAdvertisedDevice, etc. -> their Nim* equivalents), which this file
// deliberately doesn't use (see cwi-dis/iotsa#185). Including the old
// legacy header names directly is ambiguous: depending on the toolchain,
// they can resolve to the ESP32 core's own bundled (and here unwanted)
// classic BLE library instead of NimBLE-Arduino.
#include <NimBLEDevice.h>

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

class IotsaEstimoteMod : public IotsaModule, public NimBLEScanCallbacks {
public:
  IotsaEstimoteMod(IotsaApplication &_app, bool early=false)
  : IotsaModule(_app, early),
    pBLEScan(NULL),
    nKnownEstimote(0),
    nNewEstimote(0),
    estimotes(NULL)
  {}

  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
  // BLE scan callbacks
  void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override;
  void onScanEnd(const NimBLEScanResults& scanResults, int reason) override;
protected:
  void getHandler(IotsaApiContext& ctx) override;
  void putHandler(IotsaApiContext& ctx) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Estimotes"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
  // GET /api/readings[?id=...], a raw web handler (it reads the query).
  void readingsHandler();
#endif
  void configLoad() override;
  void configSave() override;
  // Called in the loop task only: the estimotes array is not shared with the
  // BLE task (onResult() posts to the loop, cwi-dis/iotsa#236).
  void _sensorData(struct NearableAdvertisement *pkt);
  void _clear();
  int _find(const uint8_t *id, int from, int to);
  String argument;
  NimBLEScan* pBLEScan;
  int nKnownEstimote;
  int nNewEstimote;
  struct Estimote *estimotes;
};

#endif

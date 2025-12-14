#ifndef _IOTSAESTIMOTE_H_
#define _IOTSAESTIMOTE_H_
#include "iotsa.h"
#include "iotsaApi.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

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

class IotsaEstimoteMod : public IotsaEstimoteModBaseMod, public BLEAdvertisedDeviceCallbacks {
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
  // BLE callback
  void onResult(BLEAdvertisedDevice advertisedDevice);
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
